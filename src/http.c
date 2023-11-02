#include <mbuspico.h>

#if MBUSPICO_HTTP_ENABLED

#include <mongoose.h>
#include <pico/cyw43_arch.h>
#include <string.h>

#include "web/config_html.raw.h"
#include "web/bootstrap_css.raw.h"

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

#define NO_CACHE_HEADERS \
	"Cache-Control: no-cache, no-store, must-revalidate" "\r\n" \
	"Pragma: no-cache" "\r\n" \
	"Expires: 0" "\r\n"

#define RESPONSE_HEADERS \
	"Server: " PROJECT_NAME "/" PROJECT_VERSION "\r\n" \
	"Connection: close" "\r\n" \
	NO_CACHE_HEADERS

#define RESPONSE_HEADERS_JSON \
	RESPONSE_HEADERS \
	"Content-Type: application/json" "\r\n"

#define RESPONSE_HEADERS_HTML \
	RESPONSE_HEADERS \
	"Content-Type: text/html" "\r\n"

#define RESPONSE_HEADERS_CSS \
	RESPONSE_HEADERS \
	"Content-Type: text/css" "\r\n"

#define RESPONSE_HEADERS_GZIP \
	"Content-Encoding: gzip" "\r\n"

#define RESPONSE_HEADERS_CHUNKED \
	"Transfer-Encoding: chunked" "\r\n"

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

// -------------------------------------------------

#define TRANSFER_CHUNK_SIZE		768
#define MAX_ACTIVE_TRANSFERS 	10

typedef struct {
	void* conn; 		// connection ptr
	int idx; 			// last sent index in data ptr
	const byte* data; 	// data ptr
	size_t len; 		// total size of data to send
} chunked_transfer_info_t;
static chunked_transfer_info_t _active_transfers[MAX_ACTIVE_TRANSFERS];


static void _http_serve_chunked_data_end(struct mg_connection *c) {
	for (int i = 0; c && i < MAX_ACTIVE_TRANSFERS; ++i) {
		if (_active_transfers[i].conn == c) {
			memset(_active_transfers+i, 0, sizeof(chunked_transfer_info_t));
			break;
		}
	}
}

static void _http_serve_chunked_data_continue(struct mg_connection *c) {
	for (int i = 0; c != NULL && i < MAX_ACTIVE_TRANSFERS; ++i) {
		if (_active_transfers[i].conn != c) {
			continue;
		}

		chunked_transfer_info_t* t = _active_transfers+i;

		// send "full" chunk
		if ((t->idx+TRANSFER_CHUNK_SIZE) < t->len) {
			mg_http_write_chunk(c, t->data + t->idx, TRANSFER_CHUNK_SIZE);
			t->idx += TRANSFER_CHUNK_SIZE;
			return;
		}

		// send remaining chunk
		int remaining = t->len - t->idx;
		if (remaining > 0) {
			mg_http_write_chunk(c, t->data + t->idx, remaining);
		}

		// send terminating chunk -> finished
		mg_http_printf_chunk(c, "");

		_http_serve_chunked_data_end(c);

		break;
	}
}

static void _http_serve_chunked_data_start(struct mg_connection *c, const char* headers, const byte* data, size_t data_len) {
	// find next free slot for transfer
	int s = -1;
	for (int i = 0; c != NULL && i < MAX_ACTIVE_TRANSFERS; ++i) {
		if (_active_transfers[i].conn == NULL) {
			s = i;
			break;
		}
	}
	if (s < 0) {
		mg_http_reply(c, 507, RESPONSE_HEADERS_HTML, "%s", "Insufficient Storage");
		return;
	}
	
	// start chunked transfer
	mg_printf(c, "HTTP/1.1 200 OK\r\n%s" RESPONSE_HEADERS_CHUNKED "\r\n", headers);
	// insert into transfer slot
	_active_transfers[s].conn = c;
	_active_transfers[s].idx = 0;
	_active_transfers[s].data = data;
	_active_transfers[s].len = data_len;
	// send first chunk
	_http_serve_chunked_data_continue(c);
}

// -------------------------------------------------

static void http_serve_fn(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {
  	switch (ev) {
		case MG_EV_ERROR:
		{
			MBUSPICO_LOG_E(LOG_TAG_HTTP, (char*)ev_data);
		}
		break;
		case MG_EV_CLOSE:
		{
			_http_serve_chunked_data_end(c);
		}
		break;
		case MG_EV_WRITE:
		{
			_http_serve_chunked_data_continue(c);
		}
		break;
		case MG_EV_HTTP_MSG:
		{
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
			// CONFIG DATA
			else if (mg_http_match_uri(hm, "/config/data")) {
			#if MBUSPICO_HTTP_AUTH_ENABLED
				if (!check_auth(hm)) {
					mg_http_reply(c, 401, RESPONSE_HEADERS_HTML RESPONSE_UNAUTH_HEADER, "%s", "Unauthorized\n");
					return;
				}
			#endif
				char encryption_key[MBUSPICO_CONFIG_SIZE_STR] = {0};
				mbuspico_read_config(MBUSPICO_CONFIG_ENCRYPTION_KEY, encryption_key, sizeof(encryption_key));
				char wifi_ssid[MBUSPICO_CONFIG_SIZE_STR] = {0};
				mbuspico_read_config(MBUSPICO_CONFIG_WIFI_SSID, wifi_ssid, sizeof(wifi_ssid));
				//char wifi_pwd[MBUSPICO_CONFIG_SIZE_STR] = {0};
				//mbuspico_read_config(MBUSPICO_CONFIG_WIFI_PWD, wifi_pwd, sizeof(wifi_pwd));
				char wifi_hostname[MBUSPICO_CONFIG_SIZE_STR] = {0};
				mbuspico_read_config(MBUSPICO_CONFIG_WIFI_HOSTNAME, wifi_hostname, sizeof(wifi_hostname));
				int http_port = -1;
				mbuspico_read_config(MBUSPICO_CONFIG_HTTP_PORT, &http_port, sizeof(int));
				char http_auth_user[MBUSPICO_CONFIG_SIZE_STR] = {0};
				mbuspico_read_config(MBUSPICO_CONFIG_HTTP_AUTH_USER, http_auth_user, sizeof(http_auth_user));
				char http_auth_pwd[MBUSPICO_CONFIG_SIZE_STR] = {0};
				mbuspico_read_config(MBUSPICO_CONFIG_HTTP_AUTH_PWD, http_auth_pwd, sizeof(http_auth_pwd));
				int udp_enabled = 0;
				mbuspico_read_config(MBUSPICO_CONFIG_UDP_ENABLED, &udp_enabled, sizeof(int));
				char udp_receiver[MBUSPICO_CONFIG_SIZE_STR] = {0};
				mbuspico_read_config(MBUSPICO_CONFIG_UDP_RECEIVER, udp_receiver, sizeof(udp_receiver));
				int udp_port = -1;
				mbuspico_read_config(MBUSPICO_CONFIG_UDP_PORT, &udp_port, sizeof(int));
				int udp_interval = -1;
				mbuspico_read_config(MBUSPICO_CONFIG_UDP_INTERVAL, &udp_interval, sizeof(int));

				mg_http_reply(c, 200, RESPONSE_HEADERS_JSON,
					"{\"version\": %m,"
					"\"meter\": {"
					"\"key\": %m"
					"},"
					"\"wifi\": {"
					"\"ssid\": %m,"
					"\"hostname\": %m"
					"},"
					"\"http\": {"
					"\"port\": %d,"
					"\"user\": %m,"
					"\"pwd\": %m"
					"},"
					"\"udp\": {"
					"\"enabled\": %s,"
					"\"receiver\": %m,"
					"\"port\": %d,"
					"\"interval\": %d"
					"}"
					"}",
					MG_ESC(PROJECT_VERSION),
					MG_ESC(encryption_key),
					MG_ESC(wifi_ssid), MG_ESC(wifi_hostname),
					http_port, MG_ESC(http_auth_user), MG_ESC(http_auth_pwd),
					(udp_enabled ? "true" : "false"), MG_ESC(udp_receiver), udp_port, udp_interval
				);
			}
			// CONFIG
			else if (mg_http_match_uri(hm, "/config")) {
			#if MBUSPICO_HTTP_AUTH_ENABLED
				if (!check_auth(hm)) {
					mg_http_reply(c, 401, RESPONSE_HEADERS_HTML RESPONSE_UNAUTH_HEADER, "%s", "Unauthorized\n");
					return;
				}
			#endif
				_http_serve_chunked_data_start(c, RESPONSE_HEADERS_HTML RESPONSE_HEADERS_GZIP, __config_html, __config_html_size);
			}
			// bootstrap.css
			else if (mg_http_match_uri(hm, "/bootstrap.css")) {
			#if MBUSPICO_HTTP_AUTH_ENABLED
				if (!check_auth(hm)) {
					mg_http_reply(c, 401, RESPONSE_HEADERS_HTML RESPONSE_UNAUTH_HEADER, "%s", "Unauthorized\n");
					return;
				}
			#endif
				_http_serve_chunked_data_start(c, RESPONSE_HEADERS_CSS RESPONSE_HEADERS_GZIP, __bootstrap_css, __bootstrap_css_size);
			}
			else {
				mg_http_reply(c, 404, RESPONSE_HEADERS_HTML, "%s", "Not Found\n");
			}
		}
		else if (strncmp(hm->method.ptr, "POST", hm->method.len) == 0) {
			// CONFIG
			if (mg_http_match_uri(hm, "/config/data")) {
			#if MBUSPICO_HTTP_AUTH_ENABLED
				if (!check_auth(hm)) {
					mg_http_reply(c, 401, RESPONSE_HEADERS_HTML RESPONSE_UNAUTH_HEADER, "%s", "Unauthorized\n");
					return;
				}
			#endif
				/////////////////////////////////////////////////////////////////////////////////
				struct mg_http_part part;
				size_t ofs = 0;
				while ((ofs = mg_http_next_multipart(hm->body, ofs, &part)) > 0) {
					MG_INFO(("Chunk name: [%.*s] filename: [%.*s] length: %lu bytes",
							(int) part.name.len, part.name.ptr,
							(int) part.filename.len, part.filename.ptr,
							(unsigned long) part.body.len)
					);
				}
				// TODO: write config
#if 0
				if (savedOk) {
					mbuspico_schedule_reboot(2000)
					char msg[] = "Configuration saved. Device is restarting now...";
					mg_http_reply(c, 202, RESPONSE_HEADERS_HTML, "%s\n", msg);
				}
				else {
					char msg[] = "Failed to save configuration.";
					mg_http_reply(c, 500, RESPONSE_HEADERS_HTML, "%s\n", msg);
				}
#else
				char msg[] = "Configuration saved. Device is restarting now...";
				mg_http_reply(c, 202, RESPONSE_HEADERS_HTML, "%s\n", msg);
#endif
			}
			else {
				mg_http_reply(c, 405, RESPONSE_HEADERS_HTML, "%s", "Not Allowed\n");
			}
		}
		else {
			mg_http_reply(c, 405, RESPONSE_HEADERS_HTML, "%s", "Not Allowed\n");
		}
		}
  	}
}

void mbuspico_http_task(void* arg) {
	MBUSPICO_LOG_D(LOG_TAG_HTTP, "mbuspico_http_task()");
	
	memset(&_active_transfers, 0, sizeof(_active_transfers));

	for(;;) {
		int state = mbuspico_wifi_get_state();
		if (state == CYW43_LINK_UP) { // connected and assigned IP
			struct mg_mgr mgr;                                
			mg_mgr_init(&mgr);
			
			int port = 80;
			mbuspico_read_config(MBUSPICO_CONFIG_HTTP_PORT, &port, sizeof(int));
			
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
