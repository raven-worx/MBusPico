#include <mbuspico.h>

#if MBUSPICO_UDP_ENABLED

#include <mongoose.h>
#include <pico/cyw43_arch.h>

#define DATA_BUFFER_SIZE 	512

#define UDP_CONNECT_TIMEOUT 2000

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

void mbuspico_udp_task(void* arg) {
	MBUSPICO_LOG_D(LOG_TAG_UDP, "mbuspico_udp_task()");

	int udp_enabled = 0;
	mbuspico_read_config_num(MBUSPICO_CONFIG_UDP_ENABLED, &udp_enabled);

	int udp_port = -1;
	mbuspico_read_config_num(MBUSPICO_CONFIG_UDP_PORT, &udp_port);

	char udp_host[MBUSPICO_CONFIG_SIZE_STR] = {0};
	mbuspico_read_config(MBUSPICO_CONFIG_UDP_RECEIVER, udp_host, sizeof(udp_host));

	int send_interval = 0;
	mbuspico_read_config_num(MBUSPICO_CONFIG_UDP_INTERVAL, &send_interval);
	if (send_interval <= 0) {
		send_interval = 30;
	}
	send_interval *= 1000; // to ms

	if (udp_enabled <= 0 || udp_port <= 0 || strlen(udp_host) == 0) {
		MBUSPICO_LOG_D(LOG_TAG_UDP, "Not executing UDP: Settings: enabled: %d, udp://%s:%d, interval: %d", udp_enabled, udp_host, udp_port, send_interval);
		vTaskDelete(NULL);
		return;
	}

	char receiver_addr[128] = {0};
	snprintf(receiver_addr, sizeof(receiver_addr), "udp://%s:%d", udp_host, (uint16_t)udp_port);
	
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
