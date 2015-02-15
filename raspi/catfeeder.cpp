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
#include <cmdline.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <catfeeder_com.h>

using namespace std;

// Setup for GPIO 22 CE and CE1 CSN with SPI Speed @ 8Mhz
RF24 radio(RPI_V2_GPIO_P1_22, RPI_V2_GPIO_P1_24, BCM2835_SPI_SPEED_1MHZ);  

void rf24_init()
{

	// Refer to RF24.h or nRF24L01 DS for settings
	radio.begin();
	radio.enableDynamicPayloads();
	radio.setAutoAck(1);
	radio.setRetries(15,15);
	radio.setDataRate(CATFEEDER_RF24_SPEED);
	radio.setPALevel(RF24_PA_MAX);
	radio.setChannel(70);
	radio.setCRCLength(RF24_CRC_8);

        radio.openWritingPipe(host_to_catfeeder_pipe);
        radio.openReadingPipe(1, catfeeder_to_host_pipe);
}

void rf24_send_force_feed(uint8_t qty) {
	
	struct cf_cmd_req req;
	
	req.type = CF_MISC_FORCE_FEED;
	req.cmd.force_feed.byte = qty;

	printf("Feeding %d parts\n", qty);
	
	radio.stopListening();
	radio.write(&req, sizeof(req));
	radio.startListening();
}


int main(int argc, char** argv) 
{
	struct gengetopt_args_info args_info;
	
	cmdline_parser_init(&args_info);

	if (cmdline_parser(argc, argv, &args_info) != 0)
		return 1;

	if (open("/dev/mem", O_RDONLY) < 0) {
		printf("root rights required\n");
		return 1;
	}

	rf24_init();
	
	if (args_info.feed_given) {
		rf24_send_force_feed(args_info.feed_arg);
	}
	
	return 0;
}

