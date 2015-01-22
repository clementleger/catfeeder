#include <Servo.h>
#include <LiquidCrystal.h>

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

#define FEEDING_SLOT_COUNT	10

#define HOURS_PER_DAY		24
#define MINUTES_PER_HOUR	60
#define MINUTES_PER_DAY		(HOURS_PER_DAY * MINUTES_PER_HOUR)

#define PORTIONS_PER_DAY	6

#define SEC_TO_MILLI		1000
#define MENU_TIMEOUT		(20 * SEC_TO_MILLI)

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

void force_feed_action(int key);
void force_feed_display();

struct menu;

struct feeding_slot {
	char hr;
	char min;
};

struct feeding_slot feeding_slots[FEEDING_SLOT_COUNT] = {0};

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
};

extern struct menu main_menu;

/**
 *  Configuration
 */
struct menu_entry configure_entries[FEEDING_SLOT_COUNT] = {
	{
		"Slot 1",
		NULL,
		NULL,
		NULL,
	},	{
		"Slot 2",
		NULL,
		NULL,
		NULL,
	},	{
		"Slot 3",
		NULL,
		NULL,
		NULL,
	},
	{
		"Add a slot",
		NULL,
		NULL,
		NULL,
	},
};

struct menu configure_menu = {
	"Configuration",
	configure_entries,
	4,
	&main_menu,
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
};

Servo feeder;
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

int part_per_portion = 4;

struct menu *cur_menu = &main_menu;
int cur_sel = 0;

void setup()
{
	pinMode(PIN_SENSOR, INPUT);
	pinMode(PIN_KEYS, INPUT_PULLUP);

	feeder.attach(PIN_SERVO);
	feeder.write(SERVO_FIX_VALUE);
	
	lcd.begin(16, 2);
	lcd.setCursor(0, 0);
	lcd.print("Catfeeder V0.1");
      
        lcd.createChar(CHAR_UP, up);
        lcd.createChar(CHAR_UPDOWN, updown);
        lcd.createChar(CHAR_DOWN, down);
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
	feeder.write(SERVO_MOVE_VALUE);
	delay(300); 
	lcd_reset();
	lcd.print("Feeding...");

	/* Wait for the parts to be delivered */
	while(part-- > 0) {
		lcd.setCursor(0, 1);
		lcd.print(part);
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
static int force_feed_parts = 1;

void force_feed_action(int button)
{
	switch (button) {
	case KEY_ENTER:
		feed(force_feed_parts);
		force_feed_parts = 1;
		break;
	case KEY_CANCEL:
		force_feed_parts = 1;
		break;
	case KEY_UP:
		force_feed_parts++;
		break;
	case KEY_DOWN:
		force_feed_parts--;
		if (force_feed_parts < 1)
			force_feed_parts = 1;
		break;
	}
}

void force_feed_display()
{
	if (force_feed_parts == 1)
		lcd.write(byte(CHAR_UP));
	else
		lcd.write(CHAR_UPDOWN);
		
	lcd.print("Parts: ");
	lcd.print(force_feed_parts);
	lcd.print(" ");
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
        lcd.print("Catfeeder V0.1");
        delay(200);

        return;
}

void loop()
{

	//if (is_feed_time())
        //delay(5000);	
	//feed(part_per_portion);

	if (button_pressed())
		handle_menu();

	//delay(500000);
}
