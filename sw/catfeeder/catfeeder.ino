#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <clsPCA9555.h>
#include <ST7036.h>
#include <Wire.h>
#include <time.h>
#include "wifi_params.h"

#define LCD_WIDTH	16
#define LCD_HEIGHT 2

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

PCA9555 ioport(0x20);
ST7036 lcd(LCD_HEIGHT, LCD_WIDTH, 0x3E);

typedef enum {
	BUTTON_NONE = 0,
	BUTTON_UP,
	BUTTON_OK,
	BUTTON_RIGHT,
	BUTTON_LEFT,
	BUTTON_DOWN,
} button_t;


byte button_pressed()
{	for (uint8_t i = 0; i < 5; i++){
		if (!ioport.digitalRead(i)) {
			delay(200);
			return i + 1;
		}
	}

	return BUTTON_NONE;
}

void setup()
{
	
	Serial.begin(115200);
	Serial.println("TOTO1");
	Wire.begin();
	WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

	for (uint8_t i = 0; i < 5; i++)
		ioport.pinMode(i, INPUT);

	ioport.pinMode(13, OUTPUT);
	
	ioport.digitalWrite(13, 1);
	Serial.println("TOTO");

	lcd.init();
	lcd.blink_on();
	lcd.clear();
	lcd.setContrast(0);
	lcd.setCursor(0,0);
	lcd.print("Catfeeder V2");
	while(WiFi.status() != WL_CONNECTED){
		delay(500);
	}

	configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");
	while (!time(nullptr)) {
		Serial.print(".");
		delay(1000);
	}
	lcd.setCursor(1,0);
	lcd.print("Ok !");

	Serial.println("Setup done\n");
}

unsigned long start_time = 0;

void loop()
{
	if (button_pressed()) {		
		ioport.digitalWrite(13, 1);
		start_time = millis();
	}

	if (millis() - start_time > 4000) {
		ioport.digitalWrite(13, 0);
	}
}
