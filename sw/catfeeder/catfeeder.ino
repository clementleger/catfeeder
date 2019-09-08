#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <clsPCA9555.h>
#include <ST7036.h>
#include <Wire.h>
#include <time.h>
#include <DRV8825.h>
#include <EEPROM.h>
#include "wifi_params.h"

#define LCD_WIDTH	16
#define LCD_HEIGHT 2

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define xstr(s) str(s)
#define str(s) #s

#ifndef __unused
#define __unused __attribute__ ((unused))
#endif

#define CATFEEDER_DNS_NAME	"catfeeder"
#define CATFEEDER_VERSION	0x01

#define UTC_TIME_OFFSET		(1 * 3600)
#define UTC_DAYLIGHT_OFFSET	(3600)

/**
 * Configuration
 */
#define IR_SENSOR_PIN		A0
#define BEEPER_PIN		D0
#define UART_SPEED		115200

#define FEEDING_SLOT_COUNT	8

#define HOURS_PER_DAY		24
#define MINUTES_PER_HOUR	60
#define MINUTES_PER_DAY		(HOURS_PER_DAY * MINUTES_PER_HOUR)

#define GRAMS_PER_PORTION	3.6
#define GPP_INCREMENT		0.1

#define SEC_TO_MILLI		1000
#define MENU_TIMEOUT		(15 * SEC_TO_MILLI)
#define TIME_CHECKING		(1 * SEC_TO_MILLI)

#define FOOD_LEVEL_LOW_VAL	1024

/**
 * MOTOR
 */

#define MOTOR_STEPS		200
#define MOTOR_MICROSTEPS	1
#define MOTOR_SPEED_RPM		60
#define MOTOR_FEED_RATIO	1
#define MOTOR_EN_PIN		D6
#define MOTOR1_DIR_PIN		D8
#define MOTOR1_STEP_PIN		D7
#define MOTOR2_DIR_PIN		D5
#define MOTOR2_STEP_PIN		D4

/**
 * EEPROM
 */
#define EEPROM_VERSION_ADDR	0x0
#define EEPROM_TOTAL_GRAM_ADDR	0x1
#define EEPROM_GPP_ADDR		(EEPROM_TOTAL_GRAM_ADDR + sizeof(total_feeding_grams))
#define EEPROM_SLOT_CONF_ADDR	(EEPROM_GPP_ADDR + sizeof(grams_per_portion))

#define EEPROM_TOTAL_SIZE (EEPROM_SLOT_CONF_ADDR + FEEDING_SLOT_COUNT * sizeof(struct feeding_slot))

#define TIME_INCREMENT	15

/**
 * LCD characters
 */

#define CHAR_UP	(byte(0))
byte up[8] = {
  0b00100,
  0b01110,
  0b11111,
  0b00000,
  0b00000,
  0b00000,
  0b00000,
  0b00000,
};

#define CHAR_UPDOWN	1
byte updown[8] = {
  0b00100,
  0b01110,
  0b11111,
  0b00000,
  0b11111,
  0b01110,
  0b00100,
  0b00000
};

#define CHAR_DOWN	2
byte down[8] = {
  0b00000,
  0b00000,
  0b00000,
  0b00000,
  0b11111,
  0b01110,
  0b00100,
  0b00000
};

/**
 * Objects
 */

PCA9555 ioport(0x20);
ST7036 lcd(LCD_HEIGHT, LCD_WIDTH, 0x3E);
ESP8266WebServer server(80);
DRV8825 stepper(MOTOR_STEPS, MOTOR1_DIR_PIN, MOTOR1_STEP_PIN, MOTOR_EN_PIN);

typedef enum {
	FOOD_LEVEL_NORMAL,
	FOOD_LEVEL_LOW,
} food_level_t;

typedef enum {
	BUTTON_NONE = 0,
	BUTTON_UP,
	BUTTON_OK,
	BUTTON_RIGHT,
	BUTTON_LEFT,
	BUTTON_DOWN,
} button_t;


typedef enum {
	STAT_TOTAL_QTY = 0,
	STAT_GRAMS_PER_DAY = 1,
	STAT_LEVEL = 2,
	STAT_BLOCKED = 3,
	STAT_COUNT = 4,
} stat_t;

static float total_feeding_grams = 0;
static float grams_per_portion = GRAMS_PER_PORTION;

static bool feeder_is_blocked = false;

static byte manual_feed_parts = 0;

/**
 * Action and display prototypes
 */
void manual_feed_display(void *data) ;
void manual_feed_action(void *data, int button);

void stat_display(void *data) ;
void stat_action(void *data, int button);

void enable_display(void *data) ;
void enable_action(void *data, int button);

void skip_display(void *data) ;
void skip_action(void *data, int button);

void slot_quant_display(void *data) ;
void slot_quant_action(void *data, int button);

void time_display(void *data) ;
void time_action(void *data, int button);

void calibrate_display(void *data) ;
void calibrate_action(void *data, int button);

void feed_now_action(void *data, int button);
void feed_now_display(void *data) ;

void disp_running();
struct menu;

struct feeding_slot {
	byte hour;
	byte min;
	byte enable;
	byte qty;
	byte has_been_fed;
};

struct feeding_slot feeding_slots[FEEDING_SLOT_COUNT] =
{
	{
		7, 30, 1, 4, 0,
	},
	{
		9, 0, 1, 3, 0,
	},
	{
		11, 30, 1, 2, 0,
	},
	{
		14, 0, 1, 2, 0,
	},
	{
		16, 30, 1, 2, 0,
	},
	{
		19, 0, 1, 3, 0,
	},
	{
		21, 30, 1, 3, 0,
	},
	{
		23, 0, 1, 2, 0,
	},
};

/**
 * Menu stuff
 */
struct menu_entry {
	const char *name;
	void (* do_action)(void *data,int key);
	void (* display)(void *data);
	struct menu *sub_menu;
};

struct menu {
	const char *name;
	struct menu_entry *entries;
	byte entry_count;
	void *data;
};

extern struct menu main_menu;
extern struct menu configure_menu;

#define SLOT_MENU(__slot)		\
struct menu_entry slot_entries_ ##__slot[] = 	\
{	\
	{	\
		"Time",	\
		time_action,	\
		time_display,	\
		NULL,	\
	},	\
	{	\
		"Qty",	\
		slot_quant_action,	\
		slot_quant_display,	\
		NULL,	\
	},	\
	{	\
		"Feed",	\
		feed_now_action,	\
		feed_now_display,	\
		NULL,	\
	},	\
	{	\
		"Skip",	\
		skip_action,	\
		skip_display,	\
		NULL,	\
	},	\
	{	\
		"Enable",	\
		enable_action,	\
		enable_display,	\
		NULL,	\
	},	\
};	\
	\
struct menu slot_menu_##__slot = {	\
	"Slot " str(__slot),	\
	slot_entries_##__slot,	\
	5,	\
	(void *) (__slot - 1),		\
}

SLOT_MENU(1);
SLOT_MENU(2);
SLOT_MENU(3);
SLOT_MENU(4);
SLOT_MENU(5);
SLOT_MENU(6);
SLOT_MENU(7);
SLOT_MENU(8);

#define CONFIGURE_SLOT_ENTRY(__slot) 	\
	{				\
		"Slot " str(__slot),	\
		NULL,			\
		NULL,			\
		&slot_menu_ ## __slot,	\
	},
/**
 *  Configuration
 */
struct menu_entry configure_entries[] =
{
	CONFIGURE_SLOT_ENTRY(1)
	CONFIGURE_SLOT_ENTRY(2)
	CONFIGURE_SLOT_ENTRY(3)
	CONFIGURE_SLOT_ENTRY(4)
	CONFIGURE_SLOT_ENTRY(5)
	CONFIGURE_SLOT_ENTRY(6)
	CONFIGURE_SLOT_ENTRY(7)
	CONFIGURE_SLOT_ENTRY(8)
};

struct menu configure_menu = {
	"Config",
	configure_entries,
	FEEDING_SLOT_COUNT,
	NULL,
};

/**
 *  Main menu
 */
struct menu_entry main_entries[] = {
	{
		"Feed",
		manual_feed_action,
		manual_feed_display,
		NULL,
	},
	{
		"Stats",
		stat_action,
		stat_display,
		NULL,
	},
	{
		"Slots",
		NULL,
		NULL,
		&configure_menu,
	},
	{
		"Calibrate",
		calibrate_action,
		calibrate_display,
		NULL,
	},
};

struct menu main_menu = {
	(char *) "Main",
	main_entries,
	4,
	NULL,
};

static const char *food_level_str[] = {
	[FOOD_LEVEL_NORMAL] = "Normal",
	[FOOD_LEVEL_LOW] = "Low",
};

/**
 * Food related
 */

static food_level_t get_food_level()
{
	int val = analogRead(IR_SENSOR_PIN);

	if (val >= FOOD_LEVEL_LOW_VAL)
		return FOOD_LEVEL_LOW;

	return FOOD_LEVEL_NORMAL;
}

static void feed(int part)
{
	int orig_part = part;
	if (part == 0)	
		return;

	lcd_reset();

	if (feeder_is_blocked) {
		lcd.print("<Blocked !>");
		delay(1000);
		return;
	}

	lcd.print("<Feeding>");

	lcd.setCursor(0, 1);
	lcd.print(orig_part * grams_per_portion, 1);
	lcd.print(" grams");

	stepper.enable();

	stepper.startRotate(part * 360 * MOTOR_FEED_RATIO);

	do {
		if (button_pressed()) {
			stepper.stop();
			goto out;
		}
	} while(stepper.nextAction() > 0);

	total_feeding_grams += orig_part * grams_per_portion;

	eeprom_write_total_qty();

out:
	stepper.disable();
	lcd_reset();

}

/**
 *  EEPROM
 */
void eeprom_write_total_qty()
{
	EEPROM.put(EEPROM_TOTAL_GRAM_ADDR, total_feeding_grams);
}

void eeprom_read_total_qty()
{
	EEPROM.get(EEPROM_TOTAL_GRAM_ADDR, total_feeding_grams);
}

void eeprom_write_gram_per_portion()
{
	EEPROM.put(EEPROM_GPP_ADDR, grams_per_portion);
}

void eeprom_read_gram_per_portion()
{
	EEPROM.get(EEPROM_GPP_ADDR, grams_per_portion);
}

void eeprom_write_slot(int slot)
{
	unsigned int addr = EEPROM_SLOT_CONF_ADDR + slot * sizeof(struct feeding_slot);

	EEPROM.put(addr, feeding_slots[slot]);
}

void eeprom_read_slot(int slot)
{
	unsigned long addr = EEPROM_SLOT_CONF_ADDR + slot * sizeof(struct feeding_slot);

	EEPROM.get(addr, feeding_slots[slot]);
	feeding_slots[slot].has_been_fed = 0; 
}

/**
 * Web server
 */

static void server_handle_root()
{
	server.send(200, "text/plain", "hello from esp8266!");
}

static void server_handle_level()
{
	String message = "Level: ";
	message += food_level_str[get_food_level()];
	message += "\n";

	server.send(200, "text/plain", message);
}

static void server_handle_feed()
{
	String message = "File Not Found\n\n";
	server.send(404, "text/plain", message);
}

static void server_handle_not_found()
{
	String message = "File Not Found\n\n";
	server.send(404, "text/plain", message);
}

static void server_init()
{

	if (MDNS.begin(CATFEEDER_DNS_NAME))
		Serial.println("MDNS responder started");

	server.on("/", server_handle_root);
	server.on("/level", server_handle_level);
	server.on("/feed", server_handle_feed);
	server.onNotFound(server_handle_not_found);
	server.begin();

}

/**
 * Actions
 */

void quantity_action(int button, byte *qty)
{
	byte new_parts = *qty;
	switch (button) {
	case BUTTON_UP:
		new_parts++;
		break;
	case BUTTON_DOWN:
		if (new_parts > 0)
			new_parts--;
		break;
	}
	*qty = new_parts;
}

void quantity_display(byte *qty)
{
	if (*qty == 0)
		lcd.write(CHAR_UP);
	else
		lcd.write(CHAR_UPDOWN);

	lcd.print("Qty: ");
	lcd.print(*qty * grams_per_portion, 1);
	lcd.print(" grams");
}

void calibrate_display(__unused void *data) 
{
	if (grams_per_portion == 0)
		lcd.write(CHAR_UP);
	else
		lcd.write(CHAR_UPDOWN);

	lcd.print("Qty: ");
	lcd.print(grams_per_portion, 1);
	lcd.print(" grams");
}

void calibrate_action(__unused void *data, int button)
{
	switch (button) {
	case BUTTON_UP:
		grams_per_portion += GPP_INCREMENT;
		break;
	case BUTTON_DOWN:
		if (grams_per_portion > GPP_INCREMENT)
			grams_per_portion -= GPP_INCREMENT;
		break;
	}
	if (button == BUTTON_OK || button == BUTTON_LEFT)
		eeprom_write_gram_per_portion();
}

void manual_feed_display(__unused void *data) 
{
	quantity_display(&manual_feed_parts);
}

void manual_feed_action(__unused void *data, int button)
{
	if (button == BUTTON_OK) {
		feed(manual_feed_parts);
		manual_feed_parts = 0;
	} else if (button == BUTTON_LEFT) {
		manual_feed_parts = 0;
	} else {
		quantity_action(button, &manual_feed_parts);
	}
}

static byte cur_stat = 0;

void stat_display(__unused void *data) 
{
	float tmp = 0;

	lcd.print("                ");
	lcd.setCursor(0, 1);
	if (cur_stat == 0)
		lcd.write(CHAR_DOWN);
	else if (cur_stat == (STAT_COUNT - 1))
		lcd.write(CHAR_UP);
	else
		lcd.write(CHAR_UPDOWN);

	switch (cur_stat) {
		case STAT_TOTAL_QTY:
			lcd.print("Total: ");
			lcd.print(total_feeding_grams, 1);
			lcd.print(" g");
			break;
		case STAT_GRAMS_PER_DAY:
			lcd.print("Per day: ");
			for(int i = 0; i < FEEDING_SLOT_COUNT; i++)
				tmp += (feeding_slots[i].qty * grams_per_portion);
			lcd.print(tmp, 1);
			lcd.print(" g");
			break;
		case STAT_LEVEL:
			lcd.print("Level: ");
			lcd.print(analogRead(IR_SENSOR_PIN));
			break;
		case STAT_BLOCKED:
			lcd.print("Blocked: ");
			lcd.print(feeder_is_blocked ? "Yes" : "No");
			break;
		default:
			break;
	}
}

void stat_action( __unused void *data, int button)
{
	if (button == BUTTON_DOWN) {
		if (cur_stat != (STAT_COUNT - 1))
			cur_stat++;
	} else if (button == BUTTON_UP) {
		if (cur_stat != 0)
			cur_stat--;
	} else if (button == BUTTON_OK) {
		if (cur_stat == STAT_BLOCKED)
			feeder_is_blocked = false;
	}	
}


void time_display(void *data) 
{
	int slot_idx = (int) data;
	struct feeding_slot *slot = &feeding_slots[slot_idx];

	lcd.print(" ");

	if (slot->hour < 10)
		lcd.print("0");
	lcd.print(slot->hour);
	lcd.print(":");

	if (slot->min < 10)
		lcd.print("0");
	lcd.print(slot->min);

}

void time_action(void *data, int button)
{
	int slot_idx = (int) data;
	struct feeding_slot *slot = &feeding_slots[slot_idx];

	switch (button) {
	case BUTTON_UP:
		slot->min += TIME_INCREMENT;
		if (slot->min == 60) {
			slot->min = 0;
			slot->hour++;
			if (slot->hour == 24) {
				slot->hour = 0;
			}
		}
		break;
	case BUTTON_DOWN:
		if (slot->min == 0) {
			slot->min = 60 - TIME_INCREMENT;
			
			if (slot->hour == 0) {
				slot->hour = 23;
			} else {
				slot->hour--;
			}
		} else {
			slot->min -= TIME_INCREMENT;
		}
		break;
	case BUTTON_OK:
		eeprom_write_slot(slot_idx);
		break;
	}
}

void bool_display(byte value)
{

	if (value) {
		lcd.write(CHAR_DOWN);
		lcd.print(" On ");
	} else {
		lcd.write(CHAR_UP);
		lcd.print(" Off ");
	}
}

void bool_action(int button, byte *value)
{
	switch (button) {
	case BUTTON_UP:
		*value = 1;
		break;
	case BUTTON_DOWN:
		*value = 0;
		break;
	}
}

void enable_display(void *data) 
{
	int slot_idx = (int) data;
	bool_display(feeding_slots[slot_idx].enable);
}

void enable_action(void *data, int button)
{
	int slot_idx = (int) data;

	if (button == BUTTON_OK || button == BUTTON_LEFT)
		eeprom_write_slot(slot_idx);
	else 
		bool_action(button, &feeding_slots[slot_idx].enable);
}

void skip_display(void *data) 
{
	int slot_idx = (int) data;
	bool_display(feeding_slots[slot_idx].has_been_fed);
}

void skip_action(void *data, int button)
{
	int slot_idx = (int) data;
	bool_action(button, &feeding_slots[slot_idx].has_been_fed);
}

void feed_now_action(void *data, int button)
{
	int slot_idx = (int) data;
	
	if (button == BUTTON_OK) {
		feeding_slots[slot_idx].has_been_fed = 1;
		feed(feeding_slots[slot_idx].qty);
	}
}

void feed_now_display(__unused void *data) 
{
	lcd.print("Press enter");
}

void slot_quant_display(void *data) 
{
	int slot_idx = (int) data;

	quantity_display(&feeding_slots[slot_idx].qty);
}

void slot_quant_action(__unused void *data, int button)
{
	int slot_idx = (int) data;

	quantity_action(button, &feeding_slots[slot_idx].qty);

	if (button == BUTTON_OK || button == BUTTON_LEFT)
		eeprom_write_slot(slot_idx);
}

/**
 * Misc
 */
button_t button_pressed()
{	for (uint8_t i = 0; i < 5; i++){
		if (!ioport.digitalRead(i)) {
			delay(200);
			return (button_t) (i + 1);
		}
	}

	return BUTTON_NONE;
}

static void print_time()
{
	time_t now = time(nullptr);
	const tm *t = gmtime(&now);

	if (t->tm_hour < 10)
		lcd.print("0");
	lcd.print(t->tm_hour);
	lcd.print(":");
	if (t->tm_min < 10)
		lcd.print("0");
	lcd.print(t->tm_min);
}

void disp_running()
{
	lcd_reset();
	if (feeder_is_blocked) {
		lcd.setCursor(1, 0);
		lcd.print("<! Blocked !>");
	} else {
		lcd.setCursor(3, 0);
		lcd.print("<Running>");
	}
	lcd.setCursor(5, 1);
	print_time();
}

void lcd_reset()
{
	lcd.clear();
	lcd.setCursor(0, 0);
}

static void lcd_backlight_set(int enable)
{
	ioport.digitalWrite(13, enable);
}


int check_feeding_slot(const tm *t, int slot)
{
	if (!feeding_slots[slot].enable || feeding_slots[slot].has_been_fed == 1)
		return 0;
	
	if (feeding_slots[slot].hour != t->tm_hour ||
		feeding_slots[slot].min != t->tm_min)
		return 0;

	feeding_slots[slot].has_been_fed = 1;
	//~ feed(feeding_slots[slot].qty);

	return 1;
}


static int last_day = -1;

static void check_feeding()
{
	int i;
	time_t now = time(nullptr);
	const tm *t = gmtime(&now);

	/* Reset the slots if new day */
	if (last_day != t->tm_yday) {
		last_day = t->tm_yday;
		for (i = 0; i < FEEDING_SLOT_COUNT; i++) {
			feeding_slots[i].has_been_fed = 0;
		}
	}

	for (i = 0; i < FEEDING_SLOT_COUNT; i++) {
		if (check_feeding_slot(t, i))
			break;
	}
}


/**
 * Menu handling
 */

void display_lcd_menu(struct menu *cur_menu, int menu_cur_sel)
{

	lcd_reset();
	lcd.print("<");
	lcd.print(cur_menu->name);
	lcd.print(">");

	lcd.setCursor(0, 1);
	if (cur_menu->entry_count == 1)
		lcd.print(" ");
	else if (menu_cur_sel == 0)
		lcd.write(CHAR_DOWN);
	else if (menu_cur_sel == cur_menu->entry_count - 1)
		lcd.write(CHAR_UP);
	else
		lcd.write(CHAR_UPDOWN);

	lcd.print(cur_menu->entries[menu_cur_sel].name);
}


typedef enum {
	MENU_DISABLED,
	MENU_HANDLE,
	MENU_ACTION,
} menu_state_t;

static menu_state_t menu_state;

int menu_handle_action(struct menu *cur_menu, int menu_cur_sel)
{
	int button = BUTTON_NONE;
	unsigned long last_press_time;
	
	lcd_reset();
	lcd.print("<");
	lcd.print(cur_menu->entries[menu_cur_sel].name);
	lcd.print(">");

  	do {
		ESP.wdtFeed();
		lcd.setCursor(0, 1);
		lcd.print("		");
		lcd.setCursor(0, 1);
		cur_menu->entries[menu_cur_sel].display(cur_menu->data) ;
		delay(50);
		last_press_time = millis();
		do {
			ESP.wdtFeed();
			button = button_pressed();
			if ((millis() - last_press_time) > MENU_TIMEOUT)
				return 1;
		} while(button == BUTTON_NONE);
		if (cur_menu->entries[menu_cur_sel].do_action)
			cur_menu->entries[menu_cur_sel].do_action(cur_menu->data, button);
;
	} while (button != BUTTON_OK && button != BUTTON_LEFT);
	
	return 0;
}

void handle_menu(struct menu *cur_menu)
{
	int button;
	int menu_cur_sel = 0;
	unsigned long last_press_time;
	
	do {
		ESP.wdtFeed();
		display_lcd_menu(cur_menu, menu_cur_sel);
		delay(100);
		last_press_time = millis();
		do {
			ESP.wdtFeed();
			button = button_pressed();
			if ((millis() - last_press_time) > MENU_TIMEOUT)
				return;
		} while(button == BUTTON_NONE);

		switch (button) {
			case BUTTON_OK:
				if (cur_menu->entries[menu_cur_sel].do_action) {
					if (menu_handle_action(cur_menu, menu_cur_sel)) {
						return;
					}
				} else if (cur_menu->entries[menu_cur_sel].sub_menu) {
					handle_menu(cur_menu->entries[menu_cur_sel].sub_menu);
				}
				break;
			case BUTTON_LEFT:
				return;
			case BUTTON_UP:
				if (menu_cur_sel > 0)
					menu_cur_sel--;
				break;
			case BUTTON_DOWN:
				menu_cur_sel++;
				if (menu_cur_sel == cur_menu->entry_count)
					menu_cur_sel = cur_menu->entry_count - 1;
				break;
		}
	} while(1);
}

void disp_handle_menu()
{
	
	lcd_backlight_set(1);
	handle_menu(&main_menu);
	lcd_backlight_set(0);

	disp_running();
	/* Content of eeprom might have been modified during menu handling */
	EEPROM.commit();
}

/**
 *  Init
 */
static void eeprom_init()
{
	int i;

	Serial.println("Init EEPROM");

	EEPROM.begin(EEPROM_TOTAL_SIZE);

	if (EEPROM.read(EEPROM_VERSION_ADDR) != CATFEEDER_VERSION) {
		EEPROM.write(EEPROM_VERSION_ADDR, CATFEEDER_VERSION);

		eeprom_write_total_qty();
		eeprom_write_gram_per_portion();

		for (i = 0; i < FEEDING_SLOT_COUNT; i++)
			eeprom_write_slot(i);
	} else {
		
		eeprom_read_total_qty();
		eeprom_read_gram_per_portion();

		for (i = 0; i < FEEDING_SLOT_COUNT; i++)
			eeprom_read_slot(i);
	}
	Serial.println("Init EEPROM Done");
}

/**
 * Setup and loop
 */
void setup()
{	
	Serial.begin(UART_SPEED);
	Serial.println("Starting");

	/* Run connection */
	WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
	
	/* Setup beeper */
	pinMode(BEEPER_PIN, OUTPUT);
	digitalWrite(BEEPER_PIN, 0);

	/* Setup analog in */
	pinMode(IR_SENSOR_PIN, INPUT);

	stepper.begin(MOTOR_SPEED_RPM, MOTOR_MICROSTEPS);
	Wire.begin();

	/* Setup IO expander */
	for (uint8_t i = 0; i < 5; i++)
		ioport.pinMode(i, INPUT);

	/* Setup backlight pin */
	ioport.pinMode(13, OUTPUT);

	lcd_backlight_set(1);

	lcd.init();
	lcd.blink_on();
	lcd.clear();
	lcd.setContrast(0);
	lcd.setCursor(0, 0);
	lcd.print("Booting...");

	lcd.load_custom_character(CHAR_UP, up);
	lcd.load_custom_character(CHAR_UPDOWN, updown);
	lcd.load_custom_character(CHAR_DOWN, down);

	lcd_reset();
	lcd.print("Connecting...");
	while(WiFi.status() != WL_CONNECTED){
		delay(500);
	}

	lcd.setCursor(0, 0);
	lcd.print("Fetching time...");
	configTime(UTC_TIME_OFFSET, UTC_DAYLIGHT_OFFSET, "pool.ntp.org", "time.nist.gov");
	while (time(nullptr) <= 100000) {
		Serial.print(".");
		delay(500);
	}

	eeprom_init();
	lcd_reset();
	lcd.print("Starting server ");
	server_init();
  
	disp_running();
	Serial.println("Setup done\n");
	delay(1000);
	lcd_backlight_set(0);
}

static unsigned long last_disp_millis = 0;

void loop()
{
	server.handleClient();
	MDNS.update();

	if (button_pressed() == BUTTON_OK) {
		disp_handle_menu();
	}

	if ((millis() - last_disp_millis) > TIME_CHECKING) {
		last_disp_millis = millis();

		check_feeding();

		disp_running();
	}
}
