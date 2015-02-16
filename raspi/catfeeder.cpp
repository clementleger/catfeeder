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
#include <unistd.h>

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

int rf24_send_data(cf_cmd_req_t *req)
{
	radio.stopListening();
	if (!radio.write(req, sizeof(*req))) {
		printf("Write failed");
		return 1;
	}
	radio.startListening();

	return 0;
}


int rf24_recv_data(cf_cmd_resp_t *resp)
{
	int i = 20;
	uint8_t len;

	while(i--) {
		usleep(50000);
		if (radio.available()) {
			len = radio.getDynamicPayloadSize();
			radio.read(resp, len);

			return 0;
		}
	}

	return 1;
}

int rf24_send_recv(cf_cmd_req_t *req, cf_cmd_resp_t *resp)
{
	if(rf24_send_data(req))
		return 1;

	if(rf24_recv_data(resp))
		return 1;

	return 0;
}

int rf24_slot_feed(uint8_t slotidx)
{	
	cf_cmd_req_t req;

	req.type = CF_SLOT_FEED;
	req.cmd.slotidx = slotidx;

	return rf24_send_data(&req);
}

int rf24_send_force_feed(uint8_t qty)
{	
	cf_cmd_req_t req;

	req.type = CF_MISC_FORCE_FEED;
	req.cmd.qty = qty;

	return rf24_send_data(&req);
}

int rf24_get_cal()
{	
	cf_cmd_req_t req;
	cf_cmd_resp_t resp;

	req.type = CF_CAL_VALUE_GET;

	if (rf24_send_recv(&req, &resp))
		return 1;

	printf("Calibration value: %f\n", resp.cmd.cal_value);
	
	return 0;
}

int rf24_get_slot_count()
{	
	cf_cmd_req_t req;
	cf_cmd_resp_t resp;

	req.type = CF_SLOT_GET_COUNT;

	if (rf24_send_recv(&req, &resp))
		return 1;

	printf("Slot count: %d\n", resp.cmd.slot_count);
	
	return 0;
}

int rf24_get_slot(uint8_t slot_idx)
{	
	cf_cmd_req_t req;
	cf_cmd_resp_t resp;

	req.type = CF_SLOT_GET;
	req.cmd.slotidx = slot_idx;

	if (rf24_send_recv(&req, &resp))
		return 1;

	printf("Slot hour: %d\n", resp.cmd.slot.hour);
	printf("Slot min: %d\n", resp.cmd.slot.min);
	printf("Slot qty: %d\n", resp.cmd.slot.qty);
	printf("Slot enabled: %d\n", resp.cmd.slot.enable);

	return 0;
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
	
	if (args_info.force_feed_given) {
		return rf24_send_force_feed(args_info.force_feed_arg);
	}

	if (args_info.get_cal_given) {
		return rf24_get_cal();
	}

	if (args_info.get_slot_count_given) {
		return rf24_get_slot_count();
	}

	if (args_info.get_slot_given) {
		return rf24_get_slot(args_info.get_slot_arg);
	}

	if (args_info.slot_feed_given) {
		return rf24_slot_feed(args_info.slot_feed_arg);
	}

	return 0;
}

