#include <mbuspico.h>

#if MBUSPICO_HTTP_ENABLED

#include <mongoose.h>
#include <pico/cyw43_arch.h>
#include <string.h>

#if defined(MBUSPICO_HTTP_AUTH_USER) || defined(MBUSPICO_HTTP_AUTH_PWD)
# define MBUSPICO_HTTP_AUTH_ENABLED 1

# ifndef MBUSPICO_HTTP_AUTH_USER
#  define MBUSPICO_HTTP_AUTH_USER ""
# endif
# ifndef MBUSPICO_HTTP_AUTH_PWD
#  define MBUSPICO_HTTP_AUTH_PWD ""
# endif

# define RESPONSE_UNAUTH_HEADER \
	"WWW-Authenticate: Basic realm=" PROJECT_NAME "\r\n"
#else
# define MBUSPICO_HTTP_AUTH_ENABLED 0
#endif

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

#if MBUSPICO_HTTP_AUTH_ENABLED
static int check_auth(struct mg_http_message *hm) {
	struct mg_str *s = mg_http_get_header(hm, "Authorization");
	if (s == NULL) {
		MBUSPICO_LOG_D(LOG_TAG_HTTP, "no auth header");
		return 0;
	}

	static const char auth_user[] = MBUSPICO_HTTP_AUTH_USER;
	static const size_t auth_user_len = sizeof(auth_user);
	static const char auth_pwd[] = MBUSPICO_HTTP_AUTH_PWD;
	static const size_t auth_pwd_len = sizeof(auth_pwd);

	char user[auth_user_len];
	memset(user, 0, auth_user_len);
	char pwd[auth_pwd_len];
	memset(pwd, 0, auth_pwd_len);

	mg_http_creds(hm, user, auth_user_len, pwd, auth_pwd_len);
	MBUSPICO_LOG_D(LOG_TAG_HTTP, "checking auth header: user: '%s' ==' %s', pwd: '%s' == '%s'", user, auth_user, pwd, auth_pwd);

	if (strncmp(auth_user, user, auth_user_len) == 0 && strncmp(auth_pwd, pwd, auth_pwd_len) == 0) {
		return 1;
	}

	MBUSPICO_LOG_E(LOG_TAG_HTTP, "rejected auth attempt: user: '%s', pwd: '%s'", user, pwd);
	return 0;
}
#endif

static void http_serve_fn(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {
  	if (ev == MG_EV_HTTP_MSG) {
    	struct mg_http_message *hm = (struct mg_http_message *) ev_data;
		if (strncmp(hm->method.ptr, "GET", hm->method.len) == 0) {
			// METER DATA
			if (mg_http_match_uri(hm, "/")) {
			#if MBUSPICO_HTTP_AUTH_ENABLED
				if (!check_auth(hm)) {
					mg_http_reply(c, 401, RESPONSE_HEADERS_HTML RESPONSE_UNAUTH_HEADER, "%s", "Unauthorized\n");
					return;
				}
			#endif
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
			#if MBUSPICO_HTTP_AUTH_ENABLED
				if (!check_auth(hm)) {
					mg_http_reply(c, 401, RESPONSE_HEADERS_HTML RESPONSE_UNAUTH_HEADER, "%s", "Unauthorized\n");
					return;
				}
			#endif
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
			#if MBUSPICO_HTTP_AUTH_ENABLED
				if (!check_auth(hm)) {
					mg_http_reply(c, 401, RESPONSE_HEADERS_HTML RESPONSE_UNAUTH_HEADER, "%s", "Unauthorized\n");
					return;
				}
			#endif
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
