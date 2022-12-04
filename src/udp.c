#include <mbuspico.h>

#if MBUSPICO_UDP_ENABLED

#include <mongoose.h>
#include <pico/cyw43_arch.h>

#define DATA_BUFFER_SIZE 	512

#define UDP_CONNECT_TIMEOUT 2000

static const int send_interval = 1000*
#ifdef MBUSPICO_UDP_INTERVAL_S
  MBUSPICO_UDP_INTERVAL_S
#else
  30
#endif
;

typedef struct {
	uint8_t done : 1;
	long data_size;
	uint64_t timeout;
} udp_state_t;

static void mbuspico_send_udp(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {
  	switch (ev) {
	case MG_EV_OPEN: // Connection created
		MBUSPICO_LOG_D(LOG_TAG_UDP, "connection event: MG_EV_OPEN");
    	((udp_state_t*)fn_data)->timeout = mg_millis() + UDP_CONNECT_TIMEOUT;
		break;
 	case MG_EV_POLL:
    	if (mg_millis() > ((udp_state_t*)fn_data)->timeout &&  (c->is_connecting || c->is_resolving)) {
			MBUSPICO_LOG_E(LOG_TAG_UDP, "connection timeout");
			((udp_state_t*) fn_data)->done = 1;
    	}
		break;
  	case MG_EV_CONNECT:
		{
			MBUSPICO_LOG_D(LOG_TAG_UDP, "connection event: MG_EV_CONNECT");
			char data_buffer[DATA_BUFFER_SIZE] = {0};
			size_t data_size = mbuspico_get_meterdata_json(data_buffer, DATA_BUFFER_SIZE);
			if (data_size > 0) {
				((udp_state_t*) fn_data)->data_size = data_size;
				mg_send(c, data_buffer, data_size);
			}
			else {
				MBUSPICO_LOG_E(LOG_TAG_UDP, "Error retrieving meterdata");
				((udp_state_t*) fn_data)->done = 1;
			}
		}
		break;
	case MG_EV_WRITE: // Data written to socket
		{
			long* bytes_written = (long*)ev_data;
			MBUSPICO_LOG_D(LOG_TAG_UDP, "connection event: MG_EV_WRITE (%ld)", *bytes_written);
			((udp_state_t*) fn_data)->data_size -= *bytes_written;
			if (((udp_state_t*) fn_data)->data_size <= 0) {
				((udp_state_t*) fn_data)->done = 1;
			}
		}
		break;
  	case MG_EV_ERROR:
		MBUSPICO_LOG_D(LOG_TAG_UDP, "connection event: MG_EV_ERROR");
		MBUSPICO_LOG_E(LOG_TAG_UDP, "send error: %s", (char*)ev_data);
		((udp_state_t*) fn_data)->done = 1;
		break;
	case MG_EV_CLOSE: // Connection closed
		MBUSPICO_LOG_D(LOG_TAG_UDP, "connection event: MG_EV_CLOSE");
    	((udp_state_t*) fn_data)->done = 1;
		break;
  	}
}

static int mbuspico_udp_init() {
	MBUSPICO_LOG_D(LOG_TAG_UDP, "mbuspico_udp_init()");

#ifndef MBUSPICO_UDP_RECEIVER_HOST
	#error "MBUSPICO_UDP_RECEIVER_HOST not defined. Not specified via options?"
#endif
#ifndef MBUSPICO_UDP_RECEIVER_PORT
	#error "MBUSPICO_UDP_RECEIVER_PORT not defined. Not specified via options?"
#else
	if (MBUSPICO_UDP_RECEIVER_PORT <= 0) {
		MBUSPICO_LOG_E(LOG_TAG_UDP, "invalid UDP receiver port provided: '%d'", (uint16_t)MBUSPICO_UDP_RECEIVER_PORT);
		return 1;
	}
#endif
	return 0;
}

void mbuspico_udp_task(void* arg) {
	MBUSPICO_LOG_D(LOG_TAG_UDP, "mbuspico_udp_task()");
	
	if (mbuspico_udp_init()) {
		vTaskDelete(NULL);
		return;
	}

	char receiver_addr[128] = {0};
	snprintf(receiver_addr, sizeof(receiver_addr), "udp://%s:%d", MBUSPICO_UDP_RECEIVER_HOST, (uint16_t)MBUSPICO_UDP_RECEIVER_PORT);
	
	for(;;) {
		while (mbuspico_wifi_get_state() == CYW43_LINK_UP) { // connected and assigned IP
			struct mg_mgr mgr;
			
			udp_state_t state;
			memset(&state, sizeof(udp_state_t), 0);

			mg_mgr_init(&mgr);
  			mg_connect(&mgr, receiver_addr, mbuspico_send_udp, &state);
  			while (!state.done) {
				mg_mgr_poll(&mgr, 50);
			}
  			mg_mgr_free(&mgr);
			MBUSPICO_LOG_D(LOG_TAG_UDP, "done");

			vTaskDelay(pdMS_TO_TICKS(send_interval)); 
		}
		vTaskDelay(pdMS_TO_TICKS(3000)); 
	}
}

#endif // MBUSPICO_UDP_ENABLED
