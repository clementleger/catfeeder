#include <Servo.h>
#include <LiquidCrystal.h>

#define PIN_SENSOR  	A0
#define PIN_ENTER	A1
#define PIN_CANCEL	A2
#define PIN_UP		A3
#define PIN_DOWN	A4

#define PIN_SERVO   9
#define SERVO_FIX_VALUE		189
#define SERVO_MOVE_VALUE	150

#define GRAMS_PER_PORTIONS	5
#define HOURS_PER_DAY		24
#define MINUTES_PER_HOUR	60
#define MINUTES_PER_DAY		(HOURS_PER_DAY * MINUTES_PER_HOUR)

#define GRAM_PER_DAY		80
#define PORTIONS_PER_DAY	6

enum {
	KEY_NONE = 0
	KEY_UP,
	KEY_DOWN,
	KEY_ENTER,
	KEY_CANCEL,
};

struct menu;

struct menu_entry {
	const char *name;
	void (* do_action)(void);
	struct menu *sub_menu;
};

struct menu {
	char *name;
	struct menu_entry *entries;
	int entry_count;
	struct menu *prev_menu;
};

extern struct menu main_menu;

/**
 *  Configuration
 */
struct menu_entry configure_entries[] = {
	{
		"Set interval",
		NULL,
		NULL,
	},
	{
		"Set quantity",
		NULL,
		NULL,
	},
};

struct menu configure_menu = {
	"Configuration",
	configure_entries,
	2,
	&main_menu,
};

/**
 *  Main menu
 */
struct menu_entry main_entries[] = {
	{
		"Configure",
		NULL,
		&configure_menu,
	},
};

struct menu main_menu = {
	(char *) "Main",
	main_entries,
	1,
	NULL,
};

Servo feeder;
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

int interval_in_minutes;
int gram_per_day = GRAM_PER_DAY;
int part_per_portion = 2;

struct menu *cur_menu = &main_menu;
int cur_sel = 0;

void setup()
{
	pinMode(PIN_SENSOR, INPUT);
	pinMode(PIN_ENTER, INPUT);
	pinMode(PIN_CANCEL, INPUT);
	pinMode(PIN_UP, INPUT);
	pinMode(PIN_DOWN, INPUT);

	feeder.attach(PIN_SERVO);
	feeder.write(SERVO_FIX_VALUE);
	
	lcd.begin(16, 2);
}

void wait_sensor_state(int state)
{
	while(analogRead(PIN_SENSOR) != state) {
		//do nothing
	}
}

void wait_trigger()
{
	/* Wait for the sensor to be low (since it might be already high) */
	wait_sensor_state(LOW);
	/* An then wait for it to be high */
	wait_sensor_state(HIGH);
}

void feed(int part)
{

	feeder.write(SERVO_MOVE_VALUE);
	
	/* Wait for the parts to be delivered */ 
	while(part--) {
		wait_trigger();
	}

	feeder.write(SERVO_FIX_VALUE);
}

int is_feed_time()
{
	return 0;
}

#define up_pressed() digitalRead(PIN_UP)
#define down_pressed() digitalRead(PIN_DOWN)
#define cancel_pressed() digitalRead(PIN_CANCEL)
#define enter_pressed() digitalRead(PIN_ENTER)

int button_pressed()
{
	if (enter_pressed())
		return KEY_ENTER;
	if (cancel_pressed())
		return KEY_CANCEL;
	if (up_pressed())
		return KEY_UP;
	if (down_pressed()
		return KEY_DOWN;

	return KEY_NONE;
}

void display_menu()
{
	int button;

	do {

		lcd.setCursor(0, 0);
		lcd.print(cur_menu->name);
	
		lcd.setCursor(0, 1);
		lcd.print(cur_menu->entries[cur_sel].name);

		do {
			button = button_pressed();
		} while(!button);
		
		switch (button) {
			case KEY_ENTER:
				if (cur_menu->entries[cur_sel].do_action)
					lcd.print("prout");
				else if (cur_menu->entries[cur_sel].sub_menu)
					cur_menu = cur_menu->entries[cur_sel].sub_menu;
			case KEY_UP:
				cur_sel++;
				cur_sel %= cur_menu->entry_count;
				break;
			case KEY_DOWN:
				cur_sel--;
				if (cur_sel < 0)
					cur_sel = cur_menu->entry_count - 1;
				break;
		}
	
	}
}

void loop()
{
	if (is_feed_time())
		feed(part_per_portion);
	
	if (button_pressed())
		display_menu();
	
	delay(50);
}
