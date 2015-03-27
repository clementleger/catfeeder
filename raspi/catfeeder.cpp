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

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include <microhttpd.h>

#include <catfeeder_com.h>

#define PORT    	5454
#define RESP_SIZE  	1024

using namespace std;

// Setup for GPIO 22 CE and CE1 CSN with SPI Speed @ 8Mhz
RF24 radio(RPI_V2_GPIO_P1_22, RPI_V2_GPIO_P1_24, BCM2835_SPI_SPEED_1MHZ);  

static void
rf24_init()
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

static int
rf24_send_data(cf_cmd_req_t *req)
{
	printf("Sending data\n");
	radio.stopListening();
	if (!radio.write(req, sizeof(*req))) {
		printf("Write failed\n");
		return 1;
	}
	radio.startListening();

	return 0;
}


static int
rf24_recv_data(cf_cmd_resp_t *resp)
{
	int i = 10;
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

static int
rf24_send_recv(cf_cmd_req_t *req, cf_cmd_resp_t *resp)
{
	if(rf24_send_data(req))
		return 1;

	if(rf24_recv_data(resp))
		return 1;

	return 0;
}

static int
rf24_slot_feed(uint8_t slotidx)
{	
	cf_cmd_req_t req;

	req.type = CF_SLOT_FEED;
	req.cmd.slotidx = slotidx;

	return rf24_send_data(&req);
}

static int
rf24_send_manual_feed(uint8_t qty)
{	
	cf_cmd_req_t req;

	req.type = CF_MISC_MANUAL_FEED;
	req.cmd.qty = qty;
	
	printf("Radio: sending manual feeding\n");

	return rf24_send_data(&req);
}

static int
rf24_set_time(uint8_t hour, uint8_t min)
{	
	cf_cmd_req_t req;

	req.type = CF_SET_TIME;
	req.cmd.time.hour = hour;
	req.cmd.time.min = min;
	
	printf("Radio: sending set time command\n");

	return rf24_send_data(&req);
}

static int
rf24_get_cal(char *resp_str)
{	
	cf_cmd_req_t req;
	cf_cmd_resp_t resp;

	req.type = CF_CAL_VALUE_GET;

	if (rf24_send_recv(&req, &resp))
		return 1;

	sprintf(resp_str, "{ \"cal_value\": \"%.02f\" }", resp.cmd.cal_value);

	return 0;
}

static int
rf24_get_stat(char *resp_str)
{	
	cf_cmd_req_t req;
	cf_cmd_resp_t resp;

	req.type = CF_STAT_GET;

	if (rf24_send_recv(&req, &resp))
		return 1;

	sprintf(resp_str, "{ \"total_feed\": \"%.02f\" }", resp.cmd.stat_total);

	return 0;
}

static int
rf24_get_slot_count(char *resp_str)
{	
	cf_cmd_req_t req;
	cf_cmd_resp_t resp;

	req.type = CF_SLOT_GET_COUNT;

	if (rf24_send_recv(&req, &resp))
		return 1;

	sprintf(resp_str, "{ \"slot_count\": \"%d\" }", resp.cmd.slot_count);

	return 0;
}

static int
rf24_get_slot(uint8_t slot_idx, char *resp_str)
{	
	cf_cmd_req_t req;
	cf_cmd_resp_t resp;
	req.type = CF_SLOT_GET;
	req.cmd.slotidx = slot_idx;

	if (rf24_send_recv(&req, &resp))
		return 1;

	sprintf(resp_str, "{ \"hour\": \"%d\", "
				     "\"min\": \"%d\", "
				     "\"qty\": \"%d\", "
				     "\"enable\": \"%d\" }"
					, resp.cmd.slot.hour
					, resp.cmd.slot.min
					, resp.cmd.slot.qty
					, resp.cmd.slot.enable);

	return 0;
}

static int
rf24_set_slot(uint8_t idx, uint8_t hour, uint8_t min, uint8_t qty, uint8_t enable)
{	
	cf_cmd_req_t req;

	req.type = CF_SLOT_SET;
	req.cmd.slot.idx = idx;
	req.cmd.slot.hour = hour;
	req.cmd.slot.min = min;
	req.cmd.slot.qty = qty;
	req.cmd.slot.enable = enable;

	if (rf24_send_data(&req))
		return 1;

	return 0;
}


/**
 *  Connection stuff
 */

static int
get_param_value(struct MHD_Connection *connection, const char *key, int *value)
{
	const char * str_value = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, key);
	if (str_value == NULL || str_value[0] == '\0')
		return 1;
		
	*value = atoi(str_value);
	return 0;
}

static int
handle_client_command(const char *url, struct MHD_Connection *connection)
{
	int http_ret = MHD_HTTP_NOT_FOUND, ret;
	char resp_buffer[RESP_SIZE] = {0};
	struct MHD_Response *response;
	
	printf("Handling url %s\n", url);

	if (strncmp(url, "feed", 4) == 0) {
		int qty;
		if (get_param_value(connection, "qty", &qty) != 0) {
			printf("Missing parameter qty\n");
			goto out;
		}

		printf("Receive manual feed %d\n", qty);
		if (rf24_send_manual_feed(qty))
			goto out;

	} else if (strncmp(url, "slotfeed", 8) == 0) {
		int id;
		if (get_param_value(connection, "id", &id) != 0) {	
			printf("Missing parameter id\n");
			goto out;
		}

		printf("Receive slot feed %d\n", id);

		if (rf24_slot_feed(id))
			goto out;

	}else if (strncmp(url, "setslot", 8) == 0) {
		int id, hour, min, qty, enable;
		if (get_param_value(connection, "id", &id) != 0) {	
			printf("Missing parameter id\n");
			goto out;
		}
		if (get_param_value(connection, "hour", &hour) != 0) {	
			printf("Missing parameter hour\n");
			goto out;
		}
		if (get_param_value(connection, "min", &min) != 0) {	
			printf("Missing parameter min\n");
			goto out;
		}
		if (get_param_value(connection, "qty", &qty) != 0) {	
			printf("Missing parameter qty\n");
			goto out;
		}
		if (get_param_value(connection, "enable", &enable) != 0) {	
			printf("Missing parameter enable\n");
			goto out;
		}

		printf("Receive set slot %d, %d:%d, %d parts, enabled: %d\n", id, hour, min, qty, enable);

		if (rf24_set_slot(id, hour, min, qty, enable))
			goto out;

	} else if (strncmp(url, "getslotcount", 12) == 0) {
		if (rf24_get_slot_count(resp_buffer))
			goto out;

	} else if (strncmp(url, "getslot", 7) == 0) {
		int id;
		if (get_param_value(connection, "id", &id) != 0) {	
			printf("Missing parameter id\n");
			goto out;
		}
		if (rf24_get_slot(id, resp_buffer))
			goto out;

	} else if (strncmp(url, "getcal", 6) == 0) {
		if (rf24_get_cal(resp_buffer))
			goto out;

	} else if (strncmp(url, "getstat", 7) == 0) {
		if (rf24_get_stat(resp_buffer))
			goto out;

	} else if (strncmp(url, "settime", 8) == 0) {
		int hour, min;
		if (get_param_value(connection, "hour", &hour) != 0) {	
			printf("Missing parameter id\n");
			goto out;
		}
		if (get_param_value(connection, "min", &min) != 0) {	
			printf("Missing parameter hour\n");
			goto out;
		}
		if (rf24_set_time(hour, min))
			goto out;
	} else {
		printf("Unknown request: %s\n", url);
		goto out;
	}
	
	http_ret = MHD_HTTP_OK;
	printf("Sending response: %s\n", resp_buffer);
out:
	response = MHD_create_response_from_buffer (strlen (resp_buffer),
					      (void *) resp_buffer,
					      MHD_RESPMEM_MUST_COPY);
	ret = MHD_queue_response (connection, http_ret, response);
	MHD_destroy_response (response);
	
	return ret;
}

static int
ahc_echo (void *cls,
          struct MHD_Connection *connection,
          const char *url,
          const char *method,
          const char *version,
          const char *upload_data, size_t *upload_data_size, void **ptr)
{
	static int aptr;

	/* Only handle GET request */
	if (0 != strcmp (method, "GET"))
		return MHD_NO;

	if (&aptr != *ptr) {
		/* do never respond on first call */
		*ptr = &aptr;
		return MHD_YES;
	}

	/* Skip the / */
	*ptr = NULL;
	return handle_client_command(url+1, connection);
}

int
main (int argc, char *const *argv)
{
	struct MHD_Daemon *d;
	  struct sockaddr_in daemon_ip_addr;


	memset (&daemon_ip_addr, 0, sizeof (struct sockaddr_in));
	daemon_ip_addr.sin_family = AF_INET;
	daemon_ip_addr.sin_port = htons(PORT);
	daemon_ip_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	
  
	if (open("/dev/mem", O_RDONLY) < 0) {
		printf("root rights required\n");
		return 1;
	}
	rf24_init();

	d = MHD_start_daemon ( MHD_USE_SELECT_INTERNALLY | MHD_USE_DEBUG,
                        PORT,
                        NULL, NULL, &ahc_echo, NULL,
			MHD_OPTION_CONNECTION_TIMEOUT, (unsigned int) 120,
			MHD_OPTION_SOCK_ADDR, &daemon_ip_addr,
			MHD_OPTION_END);
	if (d == NULL)
		return 1;

	while(1);

	return 0;
}
