#ifndef __CATFEEDER_H__
#define __CATFEEDER_H__

enum cf_command_type {
	CF_MISC_FORCE_FEED = 0,
	CF_GET_CAL_VALUE,
	CF_GET_SLOT_COUNT,
};

typedef struct cf_cmd_force_feed {
	uint8_t byte;
} cf_cmd_force_feed_t;


struct cf_cmd_req {
	uint8_t type;
	union {
		cf_cmd_force_feed_t force_feed;
	} cmd;
};

// RF24_1MBPS for 1Mbps, or RF24_2MBPS for 2Mbps
#define CATFEEDER_RF24_SPEED	RF24_250KBPS

static const uint64_t catfeeder_to_host_pipe = 0xF0F0F0F0D1LL; 
static const uint64_t host_to_catfeeder_pipe = 0xF0F0F0F0D2LL; 

#endif
