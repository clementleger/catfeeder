#ifndef __CATFEEDER_H__
#define __CATFEEDER_H__

#define __packed__ __attribute__ ((packed))

// RF24_1MBPS for 1Mbps, or RF24_2MBPS for 2Mbps
#define CATFEEDER_RF24_SPEED	RF24_250KBPS

static const uint64_t catfeeder_to_host_pipe = 0xF0F0F0F0D1LL; 
static const uint64_t host_to_catfeeder_pipe = 0xF0F0F0F0D2LL; 

enum cf_command_type {
	CD_ERROR = 0,
	CF_MISC_FORCE_FEED,
	CF_CAL_VALUE_GET,
	CF_SLOT_GET_COUNT,
	CF_SLOT_GET,
	CF_SLOT_SET,
	CF_SLOT_FEED,
};

/* Request */

typedef struct cf_cmd_req {
	uint8_t type;
	union {
		uint8_t qty;
		uint8_t slotidx;
		struct {
			uint8_t idx;
			uint8_t hour;
			uint8_t min;
			uint8_t qty;
			uint8_t enable;
		} slot;
	} cmd;
} __packed__ cf_cmd_req_t;

/* Response */

typedef struct cf_cmd_resp {
	uint8_t type;
	union {
		float cal_value;
		uint8_t slot_count;
		struct {
			uint8_t hour;
			uint8_t min;
			uint8_t qty;
			uint8_t enable;
		} slot;
	} cmd;
} __packed__ cf_cmd_resp_t;

#endif
