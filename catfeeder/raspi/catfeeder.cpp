/* 
 *
 *  Filename : rpi-hub.cpp
 *
 *  This program makes the RPi as a hub listening to all six pipes from the remote sensor nodes ( usually Arduino )
 *  and will return the packet back to the sensor on pipe0 so that the sender can calculate the round trip delays
 *  when the payload matches.
 *  
 *  I encounter that at times, it also receive from pipe7 ( or pipe0 ) with content of FFFFFFFFF that I will not sent
 *  back to the sender
 *
 *  Refer to RF24/examples/rpi_hub_arduino/ for the corresponding Arduino sketches to work with this code.
 * 
 *  
 *  CE is not used and CSN is GPIO25 (not pinout)
 *
 *  Refer to RPi docs for GPIO numbers
 *
 *  Author : Stanley Seow
 *  e-mail : stanleyseow@gmail.com
 *  date   : 6th Mar 2013
 *
 * 03/17/2013 : Charles-Henri Hallard (http://hallard.me)
 *              Modified to use with Arduipi board http://hallard.me/arduipi
 *						  Changed to use modified bcm2835 and RF24 library 
 *
 *
 */

#include <cstdlib>
#include <iostream>
#include <RF24/RF24.h>

using namespace std;

// Radio pipe addresses for the 2 nodes to communicate.
// First pipe is for writing, 2nd, 3rd, 4th, 5th & 6th is for reading...
const uint64_t read_pipe = 0xF0F0F0F0D2LL; 

// Setup for GPIO 22 CE and CE1 CSN with SPI Speed @ 8Mhz
RF24 radio(RPI_V2_GPIO_P1_22, RPI_V2_GPIO_P1_24, BCM2835_SPI_SPEED_1MHZ);  

int main(int argc, char** argv) 
{
	uint8_t len;

	// Refer to RF24.h or nRF24L01 DS for settings
	radio.begin();
	radio.enableDynamicPayloads();
	radio.setAutoAck(1);
	radio.setRetries(15,15);
	radio.setDataRate(RF24_250KBPS);
	radio.setPALevel(RF24_PA_MAX);
	radio.setChannel(70);
	radio.setCRCLength(RF24_CRC_8);

	radio.openReadingPipe(1, read_pipe);

	//
	// Start listening
	//
	radio.startListening();

	//
	// Dump the configuration of the rf unit for debugging
	//
	radio.printDetails();
	
	printf("Output below : \n");
	delay(1);
	radio.startListening();
	
	while(1)
	{
		char receivePayload[32];
		
		// Start listening
					
		while ( radio.available() ) 
		{
			len = radio.getDynamicPayloadSize();
			radio.read( receivePayload, len );

			// Display it on screen
			printf("Recv: size=%i payload=%s\n",len,receivePayload);

		}

		delayMicroseconds(20);
	}
	// Send back payload to sender
	radio.stopListening();
	
	return 0;
}

