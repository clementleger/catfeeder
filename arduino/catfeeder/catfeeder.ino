#include <Servo.h>
#include <LiquidCrystal.h>
#include <DS1302.h>
#include <EEPROM.h>
#include <avr/eeprom.h>
#include <SPI.h>

#include "catfeeder_com.h"

#define CATFEEDER_VERSION	0x02

/**
 * PINS 
 */

#define PIN_SENSOR  		A0
#define PIN_KEYS		A1
#define PIN_SERVO               A2

#define PIN_CLOCK_CE   		A5
#define PIN_CLOCK_IO 		A4
#define PIN_CLOCK_SCLK		A3

#define PIN_LCD_RS		7
#define PIN_LCD_ENABLE		6
#define PIN_LCD_D4		5
#define PIN_LCD_D5		4
#define PIN_LCD_D6		3
#define PIN_LCD_D7		2


/**
 * Servo
 */

#define SERVO_FIX_VALUE		109
#define SERVO_MOVE_VALUE	1

#define MAX_FEEDING_TIME_MILLI	15000

/** 
 * Button ADC values
 */

#define VALUE_UP		205
#define VALUE_DOWN		120
#define VALUE_ENTER		165
#define VALUE_CANCEL		70

#define VALUE_DELTA		10

#define VALUE_CHECK(__value, __ref)  ((__value > (__ref - VALUE_DELTA)) && (__value < (__ref + VALUE_DELTA)))

/**
 * Configuration
 */
#define UART_SPEED		115200

#define FEEDING_SLOT_COUNT	8

#define HOURS_PER_DAY		24
#define MINUTES_PER_HOUR	60
#define MINUTES_PER_DAY		(HOURS_PER_DAY * MINUTES_PER_HOUR)

#define GRAMS_PER_PORTION	3.6
#define GPP_INCREMENT		0.1

#define SEC_TO_MILLI		1000
#define MENU_TIMEOUT		(15 * SEC_TO_MILLI)
#define TIME_CHECKING		(10 * SEC_TO_MILLI)


/**
 * EEPROM
 */

#define EEPROM_VERSION_ADDR	0x0
#define EEPROM_TOTAL_GRAM_ADDR	0x1
#define EEPROM_GPP_ADDR		(EEPROM_TOTAL_GRAM_ADDR + sizeof(total_feeding_grams))
#define EEPROM_SLOT_CONF_ADDR	(EEPROM_GPP_ADDR + sizeof(grams_per_portion))

#define TIME_INCREMENT	15

enum keys {
	KEY_NONE = 0,
	
	KEY_UP,
	KEY_DOWN,
	KEY_ENTER,
	KEY_CANCEL
};

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

#define xstr(s) str(s)
#define str(s) #s

/**
 * Globals
 */
Servo feeder;
LiquidCrystal lcd(PIN_LCD_RS, PIN_LCD_ENABLE, PIN_LCD_D4, PIN_LCD_D5, PIN_LCD_D6, PIN_LCD_D7);
DS1302 rtc(PIN_CLOCK_CE, PIN_CLOCK_IO, PIN_CLOCK_SCLK);

static float total_feeding_grams = 0;
static float grams_per_portion = GRAMS_PER_PORTION;

static bool feeder_is_blocked = false;

static byte cur_hour = 0, cur_minutes = 0;

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

void clock_display(void *data) ;
void clock_action(void *data, int button);

void calibrate_display(void *data) ;
void calibrate_action(void *data, int button);

void feed_now_action(void *data, int button);
void feed_now_display(void *data) ;

void disp_running(Time t);
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
		"Quantity",	\
		slot_quant_action,	\
		slot_quant_display,	\
		NULL,	\
	},	\
	{	\
		"Feed now",	\
		feed_now_action,	\
		feed_now_display,	\
		NULL,	\
	},	\
	{	\
		"Skip today",	\
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
struct menu slot_menu_ ## __slot = {	\
	"Slot " str(__slot),	\
	slot_entries_ ## __slot,	\
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
	"Configuration",
	configure_entries,
	FEEDING_SLOT_COUNT,
	NULL,
};

/**
 *  Main menu
 */
struct menu_entry main_entries[] = {
	{
		"Manual feed",
		manual_feed_action,
		manual_feed_display,
		NULL,
	},
  {
    "Statistics",
    stat_action,
    stat_display,
    NULL,
  },
  {
    "Clock",
    clock_action,
    clock_display,
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

/**
 *  EEPROM
 */
void eeprom_write_total_qty()
{	
	eeprom_write_block(&total_feeding_grams, (void *) (EEPROM_TOTAL_GRAM_ADDR), sizeof(float));
}

void eeprom_read_total_qty()
{
	eeprom_read_block(&total_feeding_grams, (void *) (EEPROM_TOTAL_GRAM_ADDR), sizeof (float));
}

void eeprom_write_gram_per_portion()
{
	eeprom_write_block(&grams_per_portion, (void *) (EEPROM_GPP_ADDR), sizeof(float));
}

void eeprom_read_gram_per_portion()
{
	eeprom_read_block(&grams_per_portion, (void *) (EEPROM_GPP_ADDR), sizeof(float));
}


void eeprom_write_slot(int slot)
{
	byte tmp;
	unsigned long addr = EEPROM_SLOT_CONF_ADDR + slot * sizeof(struct feeding_slot);

	if (EEPROM.read(addr) != feeding_slots[slot].hour)
		EEPROM.write(addr, feeding_slots[slot].hour);
	addr++;
	if (EEPROM.read(addr) != feeding_slots[slot].min)
		EEPROM.write(addr, feeding_slots[slot].min);
	addr++;
	if (EEPROM.read(addr) != feeding_slots[slot].enable)
		EEPROM.write(addr, feeding_slots[slot].enable);
	addr++;
	if (EEPROM.read(addr) != feeding_slots[slot].qty)
		EEPROM.write(addr, feeding_slots[slot].qty);
}

void eeprom_read_slot(int slot)
{
	unsigned long addr = EEPROM_SLOT_CONF_ADDR + slot * sizeof(struct feeding_slot);

	feeding_slots[slot].hour = EEPROM.read(addr++);
	feeding_slots[slot].min = EEPROM.read(addr++);
	feeding_slots[slot].enable = EEPROM.read(addr++);
	feeding_slots[slot].qty = EEPROM.read(addr);
	feeding_slots[slot].has_been_fed = 0; 
}

void print_time(Time t)
{	
	if (t.hr < 10)
		lcd.print("0");
	lcd.print(t.hr);
	lcd.print(":");
	if (t.min < 10)
		lcd.print("0");
	lcd.print(t.min);
}
/**
 *  Init
 */
void eeprom_init()
{
	int i;

	//Serial.println("Init EEPROM");
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
	//Serial.println("Init EEPROM Done");
}

void setup()
{

	Serial.begin(UART_SPEED);
	Serial.print("Cat feeder started\n");
	pinMode(PIN_SENSOR, INPUT);
	pinMode(PIN_KEYS, INPUT_PULLUP);

	
	lcd.begin(16, 2);

        lcd.createChar(CHAR_UP, up);
        lcd.createChar(CHAR_UPDOWN, updown);
        lcd.createChar(CHAR_DOWN, down);

	rtc.writeProtect(false);
	rtc.halt(false);

	eeprom_init();
	Time t = rtc.time();
	cur_hour = t.hr;
	cur_minutes = t.min;
	disp_running(t);
}

void lcd_reset()
{
	lcd.clear();
	lcd.setCursor(0, 0);
}

int wait_sensor_state(int state)
{
	int ret = 0;
	unsigned long start_time = millis();

	do {
		if (digitalRead(PIN_SENSOR) == state)
			break;

		if ((millis() - start_time) > MAX_FEEDING_TIME_MILLI) {
			feeder_is_blocked = true;
			ret = 1;
			break;
		}

		if (button_pressed()) {
			lcd.setCursor(0, 1);
			lcd.print("Feed cancelled");
			ret = 1;
		}
	} while(1);

	return ret;
}

int wait_trigger()
{
	int ret;
	delay(70);
	/* Wait for the sensor to be low (since it might be already high) */
	if (wait_sensor_state(HIGH))
		return 1;
	delay(70);
	/* An then wait for it to be high */
	if (wait_sensor_state(LOW))
		return 1;
	
	return 0;
}

void feed(int part)
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
	
	feeder.attach(PIN_SERVO);
	feeder.write(SERVO_MOVE_VALUE);
	delay(1000); 

	/* Wait for the qty to be delivered */
	while(part-- > 0) {
		lcd.setCursor(0, 1);
		lcd.print((part + 1) * grams_per_portion, 1);
		lcd.print(" grams ");
		if (wait_trigger() != 0)
			goto out;
	}

	total_feeding_grams += orig_part * grams_per_portion;
out:
	eeprom_write_total_qty();

	feeder.write(SERVO_FIX_VALUE);
	feeder.detach();
	lcd_reset();
}

int button_pressed()
{
        int value = analogRead(PIN_KEYS);
        int key = KEY_NONE;
        delay(70);
 
        if (VALUE_CHECK(value, VALUE_UP))
                key = KEY_UP;
        else if (VALUE_CHECK(value, VALUE_DOWN))
                key = KEY_DOWN;
        else if (VALUE_CHECK(value, VALUE_CANCEL))
                key = KEY_CANCEL;
        else if (VALUE_CHECK(value, VALUE_ENTER))
                key = KEY_ENTER;
        
	return key;
}

/**
 *  Actions and display
 */
 
void quantity_action(int button, byte *qty)
{
	byte new_parts = *qty;
	switch (button) {
	case KEY_UP:
		new_parts++;
		break;
	case KEY_DOWN:
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

void calibrate_display(void *data) 
{
	if (grams_per_portion == 0)
		lcd.write(CHAR_UP);
	else
		lcd.write(CHAR_UPDOWN);

	lcd.print("Qty: ");
	lcd.print(grams_per_portion, 1);
	lcd.print(" grams");
}

void calibrate_action(void *data, int button)
{
	switch (button) {
	case KEY_UP:
		grams_per_portion += GPP_INCREMENT;
		break;
	case KEY_DOWN:
		if (grams_per_portion > GPP_INCREMENT)
			grams_per_portion -= GPP_INCREMENT;
		break;
	}
	if (button == KEY_ENTER || button == KEY_CANCEL)
		eeprom_write_gram_per_portion();
}

static byte manual_feed_parts = 0;

void manual_feed_display(void *data) 
{
	quantity_display(&manual_feed_parts);
}

void manual_feed_action(void *data, int button)
{
	if (button == KEY_ENTER) {
		feed(manual_feed_parts);
		manual_feed_parts = 0;
	} else if (button == KEY_CANCEL) {
		manual_feed_parts = 0;
	} else {
		quantity_action(button, &manual_feed_parts);
	}
}

void feed_now_action(void *data, int button)
{
	int slot_idx = (int) data;
	
	if (button == KEY_ENTER) {
		feeding_slots[slot_idx].has_been_fed = 1;
		feed(feeding_slots[slot_idx].qty);
	}
}

void feed_now_display(void *data) 
{
	lcd.print("Press enter");
}

void slot_quant_display(void *data) 
{
	int slot_idx = (int) data;

	quantity_display(&feeding_slots[slot_idx].qty);
}

void slot_quant_action(void *data, int button)
{
	int slot_idx = (int) data;

	quantity_action(button, &feeding_slots[slot_idx].qty);

	if (button == KEY_ENTER || button == KEY_CANCEL)
		eeprom_write_slot(slot_idx);
}

enum stat {
	STAT_TOTAL_QTY = 0,
	STAT_GRAMS_PER_DAY = 1,
	STAT_BLOCKED = 2,
	STAT_COUNT = 3,
};

static byte cur_stat = 0;

void stat_display(void *data) 
{
	float tmp = 0;
	if (cur_stat == 0)
		lcd.write(CHAR_UP);
	else if (cur_stat == (STAT_COUNT - 1))
		lcd.write(CHAR_DOWN);
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
		case STAT_BLOCKED:
			lcd.print("Blocked: ");
			lcd.print(feeder_is_blocked ? "Yes" : "No");
			break;
		default:
			break;
	}
}

void stat_action(void *data, int button)
{
	if (button == KEY_UP) {
		if (cur_stat != (STAT_COUNT - 1))
			cur_stat++;
	} else if (button == KEY_DOWN) {
		if (cur_stat != 0)
			cur_stat--;
	} else if (button == KEY_ENTER) {
		if (cur_stat == STAT_BLOCKED)
			feeder_is_blocked = false;
	}
		
}

void clock_display(void *data)
{
  lcd.print("Set: ");

  if (cur_hour < 10)
    lcd.print("0");
  lcd.print(cur_hour);
  lcd.print(":");

  if (cur_minutes < 10)
    lcd.print("0");
  lcd.print(cur_minutes);
}

bool inc_byte(byte *b, byte max_val) {
  *b = *b + 1;
  if (*b > max_val) {
    *b = 0;
    return true;
 }
 return false;
}

bool dec_byte(byte *b, byte max_val) {
  if (*b == 0) {
    *b = max_val;
    return true;
  } else {
    *b = *b - 1;
    return false;
  }
}

void clock_action(void *data, int button)
{
  bool ret;
  if (button == KEY_UP) {
      ret = inc_byte(&cur_minutes, 59);
      if (ret)
        inc_byte(&cur_hour, 23);
  } else if (button == KEY_DOWN) {   
      ret = dec_byte(&cur_minutes, 59);
      if (ret)
        dec_byte(&cur_hour, 23);
  } else if (button == KEY_ENTER) {
      time_set(cur_hour, cur_minutes);
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
	case KEY_UP:
		*value = 1;
		break;
	case KEY_DOWN:
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

	if (button == KEY_ENTER || button == KEY_CANCEL)
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

/**
 *  Menu
 */

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
	case KEY_UP:
		slot->min += TIME_INCREMENT;
		if (slot->min == 60) {
			slot->min = 0;
			slot->hour++;
			if (slot->hour == 24) {
				slot->hour = 0;
			}
		}
		break;
	case KEY_DOWN:
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
	case KEY_ENTER:
		eeprom_write_slot(slot_idx);
		break;
	}
}

void display_lcd_menu(struct menu *cur_menu, int menu_cur_sel)
{

        lcd_reset();
	lcd.setCursor(0, 0);
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

int menu_handle_action(struct menu *cur_menu, int menu_cur_sel)
{
	int button = KEY_NONE;
	unsigned long last_press_time;
	
        lcd_reset();
  	lcd.setCursor(0, 0);
        lcd.print("<");
	lcd.print(cur_menu->entries[menu_cur_sel].name);
        lcd.print(">");

  	do {
		lcd.setCursor(0, 1);
		lcd.print("                ");
		lcd.setCursor(0, 1);
		cur_menu->entries[menu_cur_sel].display(cur_menu->data) ;
                delay(50);
		last_press_time = millis();
		do {
			button = button_pressed();
			if ((millis() - last_press_time) > MENU_TIMEOUT)
				return 1;
		} while(button == KEY_NONE);
		if (cur_menu->entries[menu_cur_sel].do_action)
			cur_menu->entries[menu_cur_sel].do_action(cur_menu->data, button);

	} while (button != KEY_ENTER && button != KEY_CANCEL);
	
	return 0;
}

void handle_menu(struct menu *cur_menu)
{
	int button;
	int menu_cur_sel = 0;
	unsigned long last_press_time;

	do {
                display_lcd_menu(cur_menu, menu_cur_sel);
                delay(100);
		last_press_time = millis();
		do {
			button = button_pressed();
			if ((millis() - last_press_time) > MENU_TIMEOUT)
				goto out;
		} while(button == KEY_NONE);

		switch (button) {
			case KEY_ENTER:
				if (cur_menu->entries[menu_cur_sel].do_action) {
					if (menu_handle_action(cur_menu, menu_cur_sel)) {
						goto out;
					}
				} else if (cur_menu->entries[menu_cur_sel].sub_menu) {
					handle_menu(cur_menu->entries[menu_cur_sel].sub_menu);
				}
				break;
                        case KEY_CANCEL:
				return;
			case KEY_UP:
				if (menu_cur_sel > 0)
					menu_cur_sel--;
				break;
			case KEY_DOWN:
				menu_cur_sel++;
                                if (menu_cur_sel == cur_menu->entry_count)
                                        menu_cur_sel = cur_menu->entry_count - 1;
				break;
		}
	} while(1);
out:
        return;
}

int check_feeding_slot(Time t, int slot)
{
	if (!feeding_slots[slot].enable || feeding_slots[slot].has_been_fed == 1)
		return 0;
	
	if (feeding_slots[slot].hour != t.hr ||
		feeding_slots[slot].min != t.min)
		return 0;

	feeding_slots[slot].has_been_fed = 1;
	feed(feeding_slots[slot].qty);

	return 1;
}

void check_feeding(Time t)
{
	int i;

	for (i = 0; i < FEEDING_SLOT_COUNT; i++) {
		if (check_feeding_slot(t, i))
			break;
	}

}

void disp_running(Time t)
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
	print_time(t);
}


void time_set(byte hour, byte min)
{
	Time t = rtc.time();
	t.min = min;
	t.hr = hour;

	rtc.time(t);
}


/**
 *  Main loop
 */
static int last_day = -1;

static unsigned long last_millis = 0;

void loop()
{	
	Time t(0,0,0,0,0,0, Time::kSunday);
	int i;

	if ((millis() - last_millis) > TIME_CHECKING) {
		last_millis = millis();
	
		t = rtc.time();
		cur_hour = t.hr;
		cur_minutes = t.min;
		/* Reset the slots */
		if (last_day != t.day) {
			last_day = t.day;
			for (i = 0; i < FEEDING_SLOT_COUNT; i++) {
				feeding_slots[i].has_been_fed = 0;
			}
		}

		disp_running(t);
		check_feeding(t);
	}

	if (button_pressed()) {
		handle_menu(&main_menu);
		t = rtc.time();
		disp_running(t);
	}

	delay(100);
}
