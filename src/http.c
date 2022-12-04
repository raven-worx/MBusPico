#include <mbuspico.h>

#if MBUSPICO_HTTP_ENABLED

#include <mongoose.h>
#include <pico/cyw43_arch.h>

#define RESPONSE_BUFFER_SIZE 	512

#define RESPONSE_HEADERS \
	"Server: " PROJECT_NAME "/" PROJECT_VERSION "\r\n" \
	"Connection: close" "\r\n"

#define RESPONSE_HEADERS_JSON \
	RESPONSE_HEADERS \
	"Content-Type: application/json" "\r\n"

#define RESPONSE_HEADERS_HTML \
	RESPONSE_HEADERS \
	"Content-Type: text/html" "\r\n"

static void http_serve_fn(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {
  	if (ev == MG_EV_HTTP_MSG) {
    	struct mg_http_message *hm = (struct mg_http_message *) ev_data;
		if (strncmp(hm->method.ptr, "GET", hm->method.len) == 0) {
			// METER DATA
			if (mg_http_match_uri(hm, "/")) {
				char data_buffer[RESPONSE_BUFFER_SIZE] = {0};
				size_t data_size = mbuspico_get_meterdata_json(data_buffer, RESPONSE_BUFFER_SIZE);
				if (data_size > 0) {
					mg_http_reply(c, 200, RESPONSE_HEADERS_JSON, "%s\n", data_buffer);
				}
				else {
					MBUSPICO_LOG_E(LOG_TAG_HTTP, "Error retrieving meterdata");
					mg_http_reply(c, 500, RESPONSE_HEADERS_HTML, "%s", "Internal Server Error: Error retrieving meterdata");
				}
			}
			// UPDATE
			else if (mg_http_match_uri(hm, "/update")) {
				if (mbuspico_schedule_reboot_usb(2000)) {
					char msg[] = "Device is restarting into USB mode...";
					mg_http_reply(c, 202, RESPONSE_HEADERS_HTML, "%s\n", msg);
				}
				else {
					char msg[] = "Failed to initiate reboot";
					mg_http_reply(c, 500, RESPONSE_HEADERS_HTML, "%s\n", msg);
				}
			}
			// REBOOT
			else if (mg_http_match_uri(hm, "/reboot")) {
				if (mbuspico_schedule_reboot(2000)) {
					char msg[] = "Device is restarting...";
					mg_http_reply(c, 202, RESPONSE_HEADERS_HTML, "%s\n", msg);
				}
				else {
					char msg[] = "Failed to initiate reboot";
					mg_http_reply(c, 500, RESPONSE_HEADERS_HTML, "%s\n", msg);
				}
			}
			else {
				mg_http_reply(c, 404, RESPONSE_HEADERS_HTML, "%s", "Not Found\n");
			}
		}
		else {
			mg_http_reply(c, 405, RESPONSE_HEADERS_HTML, "%s", "Not Allowed\n");
		}
  	}
}

void mbuspico_http_task(void* arg) {
	MBUSPICO_LOG_D(LOG_TAG_HTTP, "mbuspico_http_task()");
	
	for(;;) {
		int state = mbuspico_wifi_get_state();
		if (state == CYW43_LINK_UP) { // connected and assigned IP
			struct mg_mgr mgr;                                
			mg_mgr_init(&mgr);
			
		#ifdef MBUSPICO_HTTP_SERVER_PORT
			uint16_t port = MBUSPICO_HTTP_SERVER_PORT;
		#else
			uint16_t port = 80;
		#endif
			
			char listen_addr[15] = {0};
			snprintf(listen_addr, sizeof(listen_addr), "0.0.0.0:%d", port);
			mg_http_listen(&mgr, listen_addr, http_serve_fn, NULL);
			
			while ((state = mbuspico_wifi_get_state()) == CYW43_LINK_UP) {
				mg_mgr_poll(&mgr, 1000);
			}
			
			mg_mgr_free(&mgr);
		}
		
		vTaskDelay(pdMS_TO_TICKS(3000)); // retry / check again in 3 secs
	}
}

#endif // MBUSPICO_HTTP_ENABLED
