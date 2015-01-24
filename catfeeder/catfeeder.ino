#include <Servo.h>
#include <LiquidCrystal.h>
#include <DS1302.h>

#define PIN_SENSOR  	A0
#define PIN_KEYS	A1

#define VALUE_UP       205
#define VALUE_DOWN      120
#define VALUE_ENTER     165
#define VALUE_CANCEL     70

#define VALUE_DELTA      10

#define VALUE_CHECK(__value, __ref)  ((__value > (__ref - VALUE_DELTA)) && (__value < (__ref + VALUE_DELTA)))

#define PIN_SERVO               13
#define SERVO_FIX_VALUE		109
#define SERVO_MOVE_VALUE	50

#define FEEDING_SLOT_COUNT	4

#define HOURS_PER_DAY		24
#define MINUTES_PER_HOUR	60
#define MINUTES_PER_DAY		(HOURS_PER_DAY * MINUTES_PER_HOUR)

#define PORTIONS_PER_DAY	6

#define SEC_TO_MILLI		1000
#define MENU_TIMEOUT		(10 * SEC_TO_MILLI)
#define TIME_CHECKING		(1 * SEC_TO_MILLI)

#define CE_PIN   	8
#define IO_PIN 		9
#define SCLK_PIN	10

/* Create a DS1302 object */
DS1302 rtc(CE_PIN, IO_PIN, SCLK_PIN);


enum keys {
	KEY_NONE,
	
	KEY_UP,
	KEY_DOWN,
	KEY_ENTER,
	KEY_CANCEL
};

#define CHAR_UP	0
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

void force_feed_display();
void force_feed_action(int button);

void enable_display();
void enable_action(int button);

void slot_quant_display();
void slot_quant_action(int button);

void time_display();
void time_action(int button);

struct menu;

struct feeding_slot {
	int hour;
	int min;
	int enable;
	int parts;
};

struct feeding_slot feeding_slots[FEEDING_SLOT_COUNT] =
{
	{
		7, 0, 1, 6,
	},
	{
		12, 0, 1, 6,
	},
	{
		18, 0, 1, 6,
	},
	{
		23, 0, 1, 6,
	},
};

struct menu_entry {
	const char *name;
	void (* do_action)(int key);
	void (* display)();
	struct menu *sub_menu;
};

struct menu {
	const char *name;
	struct menu_entry *entries;
	int entry_count;
	struct menu *prev_menu;
	void *data;
};

extern struct menu main_menu;
extern struct menu configure_menu;

#define slot_menu(__slot)		\
struct menu_entry slot_entries_ ##__slot[3] = 	\
{	\
	{	\
		"Enable",	\
		enable_action,	\
		enable_display,	\
		NULL,	\
	},	\
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
};	\
	\
struct menu slot_menu_ ## __slot = {	\
	"Slot " str(__slot),	\
	slot_entries_ ## __slot,	\
	3,	\
	&configure_menu,	\
	(void *) __slot,		\
}

slot_menu(1);
slot_menu(2);
slot_menu(3);
slot_menu(4);

/**
 *  Configuration
 */
struct menu_entry configure_entries[FEEDING_SLOT_COUNT] =
{
	{
		"Slot 1",
		NULL,
		NULL,
		&slot_menu_1,
	},
	{
		"Slot 2",
		NULL,
		NULL,
		&slot_menu_2,
	},
	{
		"Slot 3",
		NULL,
		NULL,
		&slot_menu_3,
	},
	{
		"Slot 4",
		NULL,
		NULL,
		&slot_menu_4,
	},
};

struct menu configure_menu = {
	"Configuration",
	configure_entries,
	FEEDING_SLOT_COUNT,
	&main_menu,
	NULL,
};

/**
 *  Main menu
 */
struct menu_entry main_entries[] = {
	{
		"Force feed",
		force_feed_action,
		force_feed_display,
		NULL,
	},
	{
		"Configure",
		NULL,
		NULL,
		&configure_menu,
	},
};

struct menu main_menu = {
	(char *) "Main",
	main_entries,
	2,
	NULL,
	NULL,
};

Servo feeder;
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

static struct menu *cur_menu = &main_menu;
static int cur_sel = 0;

void setup()
{
	pinMode(PIN_SENSOR, INPUT);
	pinMode(PIN_KEYS, INPUT_PULLUP);

	feeder.attach(PIN_SERVO);
	feeder.write(SERVO_FIX_VALUE);
	
	lcd.begin(16, 2);
	lcd.setCursor(0, 0);
        lcd.print("Running");

        lcd.createChar(CHAR_UP, up);
        lcd.createChar(CHAR_UPDOWN, updown);
        lcd.createChar(CHAR_DOWN, down);

	rtc.halt(false);
}

void lcd_reset()
{
	lcd.clear();
	lcd.setCursor(0, 0);
}

void wait_sensor_state(int state)
{
	while(digitalRead(PIN_SENSOR) != state) {
		//do nothing
	}
}

void wait_trigger()
{
	delay(70);
	/* Wait for the sensor to be low (since it might be already high) */
	wait_sensor_state(LOW);
	delay(70);
	/* An then wait for it to be high */
	wait_sensor_state(HIGH);
}

void feed(int part)
{
	if (part == 0)	
		return;

	feeder.write(SERVO_MOVE_VALUE);
	delay(300); 
	lcd_reset();
	lcd.print("Feeding...");

	/* Wait for the parts to be delivered */
	while(part-- > 0) {
		lcd.setCursor(0, 1);
		lcd.print(part + 1);
		wait_trigger();
	}

	feeder.write(SERVO_FIX_VALUE);
	lcd_reset();
}

int is_feed_time()
{
	return 0;
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

void quantity_action(int button, int *parts)
{
	switch (button) {
	case KEY_ENTER:
		*parts = 0;
		break;
	case KEY_CANCEL:
		*parts = 0;
		break;
	case KEY_UP:
		(*parts)++;
		break;
	case KEY_DOWN:
		(*parts)--;
		if (*parts < 0)
			*parts = 0;
		break;
	}
}

void quantity_display(int *parts)
{
	if (*parts == 0)
		lcd.write(byte(CHAR_UP));
	else
		lcd.write(CHAR_UPDOWN);
		
	lcd.print("Parts: ");
	lcd.print(*parts);
	lcd.print(" ");
}

static int force_feed_parts = 0;

void force_feed_display()
{
	quantity_display(&force_feed_parts);
}

void force_feed_action(int button)
{
	if (button == KEY_ENTER)
		feed(force_feed_parts);

	quantity_action(button, &force_feed_parts);
}

void slot_quant_display()
{
	int slot_idx = (int) cur_menu->data;

	quantity_display(&feeding_slots[slot_idx].parts);
}

void slot_quant_action(int button)
{
	int slot_idx = (int) cur_menu->data;

	quantity_action(button, &feeding_slots[slot_idx].parts);
}

void enable_display()
{
	int slot_idx = (int) cur_menu->data;

	if (feeding_slots[slot_idx].enable) {
		lcd.write(CHAR_DOWN);
		lcd.print(" On ");
	} else {
		lcd.write(byte(CHAR_UP));
		lcd.print(" Off ");
	}
}

void enable_action(int button)
{
	int slot_idx = (int) cur_menu->data;

	switch (button) {
	case KEY_UP:
		feeding_slots[slot_idx].enable = 1;
		break;
	case KEY_DOWN:
		feeding_slots[slot_idx].enable = 0;
		break;
	}
}

static int cur_field = 0;

void time_display()
{
	int slot_idx = (int) cur_menu->data;
	lcd.print(" ");

	if (feeding_slots[slot_idx].hour < 10)
		lcd.print("0");
	lcd.print(feeding_slots[slot_idx].hour);
	lcd.print(":");

	if (feeding_slots[slot_idx].min < 10)
		lcd.print("0");
	else
		lcd.print(feeding_slots[slot_idx].min);

}

void time_action(int button)
{
	int slot_idx = (int) cur_menu->data;

	switch (button) {
	case KEY_UP:
		feeding_slots[slot_idx].min += 30;
		if (feeding_slots[slot_idx].min == 60) {
			feeding_slots[slot_idx].min = 0;
			feeding_slots[slot_idx].hour++;
			if (feeding_slots[slot_idx].hour == 24) {
				feeding_slots[slot_idx].hour = 0;
			}
		}
		break;
	case KEY_DOWN:	
		feeding_slots[slot_idx].min -= 30;
		if (feeding_slots[slot_idx].min < 0) {
			feeding_slots[slot_idx].min = 30;
			feeding_slots[slot_idx].hour--;
			if (feeding_slots[slot_idx].hour < 0) {
				feeding_slots[slot_idx].hour = 23;
			}
		}
		break;
	}
}

void display_lcd_menu()
{

        lcd_reset();
	lcd.setCursor(0, 0);
        lcd.print("<");
	lcd.print(cur_menu->name);
        lcd.print(">");

	lcd.setCursor(0, 1);
        if (cur_menu->entry_count == 1)
                lcd.print(" ");
        else if (cur_sel == 0)
                lcd.write(CHAR_DOWN);
        else if (cur_sel == cur_menu->entry_count - 1)
                lcd.write(byte(CHAR_UP));
        else
                lcd.write(CHAR_UPDOWN);

	lcd.print(cur_menu->entries[cur_sel].name);
}

int menu_handle_action()
{
	int button = KEY_NONE;
	unsigned long last_press_time;
	
        lcd_reset();
  	lcd.setCursor(0, 0);
        lcd.print("<");
	lcd.print(cur_menu->entries[cur_sel].name);
        lcd.print(">");

  	do {
		lcd.setCursor(0, 1);
		lcd.print("                ");
		lcd.setCursor(0, 1);
		cur_menu->entries[cur_sel].display();
                delay(100);
		last_press_time = millis();
		do {
			button = button_pressed();
			if ((millis() - last_press_time) > MENU_TIMEOUT)
				return 1;
		} while(button == KEY_NONE);

		cur_menu->entries[cur_sel].do_action(button);
	} while (button != KEY_ENTER && button != KEY_CANCEL);
	
	return 0;
}

void handle_menu()
{
	int button;
	unsigned long last_press_time;

	do {
                display_lcd_menu();
                delay(100);
		last_press_time = millis();
		do {
			button = button_pressed();
			if ((millis() - last_press_time) > MENU_TIMEOUT)
				goto out;
		} while(button == KEY_NONE);

		switch (button) {
			case KEY_ENTER:
				if (cur_menu->entries[cur_sel].do_action) {
					if (menu_handle_action()) {
						goto out;
					}
				} else if (cur_menu->entries[cur_sel].sub_menu) {
					cur_menu = cur_menu->entries[cur_sel].sub_menu;
					cur_sel = 0;
				}
				break;
                        case KEY_CANCEL:
				if (cur_menu->prev_menu) {
					cur_sel = 0;
					cur_menu= cur_menu->prev_menu;
				} else {
					goto out;
				}
				break;
			case KEY_UP:
				cur_sel--;
				if (cur_sel < 0)
					cur_sel = 0;
				break;
			case KEY_DOWN:
				cur_sel++;
                                if (cur_sel == cur_menu->entry_count)
                                        cur_sel = cur_menu->entry_count - 1;
				break;
		}
	} while(1);
out:
        cur_sel = 0;
        lcd_reset();
        lcd.print("Running");
        delay(200);

        return;
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
	lcd.print(":");
	if (t.sec < 10)
		lcd.print("0");
	lcd.print(t.sec);
}

static int last_millis = 0;

void loop()
{	
	if ((millis() - last_millis) > TIME_CHECKING) {
		last_millis = millis();
	
		Time t = rtc.time();
		lcd.setCursor(0, 1);
		print_time(t);
	}

	if (button_pressed())
		handle_menu();

	delay(200);
}
