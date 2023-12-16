#include <mbuspico.h>

#if MBUSPICO_HTTP_ENABLED

#include <web_res.h>

#include <mongoose.h>
#include <pico/cyw43_arch.h>
#include <string.h>

QueueHandle_t g_WebsocketDataQueue = NULL;

#define RESPONSE_BUFFER_SIZE 	512

#define RESPONSE_UNAUTH_HEADER \
	"WWW-Authenticate: Basic realm=" PROJECT_NAME "\r\n"

#define NO_CACHE_HEADERS \
	"Cache-Control: no-cache, no-store, must-revalidate" "\r\n" \
	"Pragma: no-cache" "\r\n" \
	"Expires: 0" "\r\n"

#define RESPONSE_HEADERS \
	"Server: " PROJECT_NAME "/" PROJECT_VERSION "\r\n" \
	"Connection: close" "\r\n" \
	NO_CACHE_HEADERS

#define RESPONSE_HEADER_CONTENT_TYPE(T) \
	RESPONSE_HEADERS \
	"Content-Type: " T "\r\n"

#define RESPONSE_HEADERS_JSON \
	RESPONSE_HEADER_CONTENT_TYPE("application/json")

#define RESPONSE_HEADERS_HTML \
	RESPONSE_HEADER_CONTENT_TYPE("text/html")

#define RESPONSE_HEADERS_CSS \
	RESPONSE_HEADER_CONTENT_TYPE("text/css")

#define RESPONSE_HEADERS_JS \
	RESPONSE_HEADER_CONTENT_TYPE("text/javascript")

#define RESPONSE_HEADERS_GZIP \
	"Content-Encoding: gzip" "\r\n"

#define RESPONSE_HEADERS_CHUNKED \
	"Transfer-Encoding: chunked" "\r\n"

static int _http_check_auth(struct mg_http_message *hm) {
	char http_auth_user[MBUSPICO_CONFIG_SIZE_STR] = {0};
	mbuspico_read_config(MBUSPICO_CONFIG_HTTP_AUTH_USER, http_auth_user, sizeof(http_auth_user));
	char http_auth_pwd[MBUSPICO_CONFIG_SIZE_STR] = {0};
	mbuspico_read_config(MBUSPICO_CONFIG_HTTP_AUTH_PWD, http_auth_pwd, sizeof(http_auth_pwd));

	if (strlen(http_auth_user) == 0 && strlen(http_auth_pwd) == 0) {
		return 1; // no auth -> always grant access
	}

	struct mg_str *s = mg_http_get_header(hm, "Authorization");
	if (s == NULL) {
		MBUSPICO_LOG_D(LOG_TAG_HTTP, "no auth header");
		return 0;
	}

	char user[64];
	memset(user, 0, sizeof(user));
	char pwd[64];
	memset(pwd, 0, sizeof(pwd));

	mg_http_creds(hm, user, sizeof(user), pwd, sizeof(pwd));
	MBUSPICO_LOG_D(LOG_TAG_HTTP, "checking auth header: user: '%s' ==' %s', pwd: '%s' == '%s'", user, http_auth_user, pwd, http_auth_pwd);

	if (strlen(http_auth_user) != strlen(user) && strlen(http_auth_pwd) != strlen(pwd)
		&& strncmp(http_auth_user, user, sizeof(user)) == 0 && strncmp(http_auth_pwd, pwd, sizeof(pwd)) == 0)
	{
		return 1;
	}

	MBUSPICO_LOG_E(LOG_TAG_HTTP, "rejected auth attempt: user: '%s', pwd: '%s'", user, pwd);
	return 0;
}

// -------------------------------------------------

static const char* _mime_type_from_filename(const char* filename) {
  	const char* suffix = strrchr(filename, '.');
	if (suffix) {
    	if (strcmp(suffix,".html") == 0) {
			static const char* html = "text/html";
			return html;
		}
		else if (strcmp(suffix,".css") == 0) {
			static const char* css = "text/css";
			return css;
		}
		else if (strcmp(suffix,".js") == 0) {
			static const char* js = "text/javascript";
			return js;
		}
	}
	static const char* text = "text/plain";
  	return text;
}

// -------------------------------------------------

#define TRANSFER_CHUNK_SIZE			(768)
#define MAX_STORED_CONNECTIONS 		(10)

#define CONNECTION_TYPE_WS					(1)
#define CONNECTION_TYPE_CHUNKED_TRANSFER 	(2)

typedef struct {
	void* conn; 		// connection ptr
	int type;			// connection type (websocket, chunked transfer)
	union {
		struct {
			int idx; 			// last sent index in data ptr
			const byte* data; 	// data ptr
			size_t len; 		// total size of data to send
		} chunked_transfer;
		struct {
		} ws;
	};
} connection_info_t;
static connection_info_t _stored_connections[MAX_STORED_CONNECTIONS];

static int _http_get_free_stored_connection_slot() {
	for (int i = 0; i < MAX_STORED_CONNECTIONS; ++i) {
		if (_stored_connections[i].conn == NULL) {
			return i;
		}
	}
	return -1;
}

static void _http_remove_stored_connection(struct mg_connection *c) {
	for (int i = 0; c && i < MAX_STORED_CONNECTIONS; ++i) {
		if (_stored_connections[i].conn == c) {
			memset(_stored_connections+i, 0, sizeof(connection_info_t));
			break;
		}
	}
}

static void _http_serve_chunked_data_continue(struct mg_connection *c) {
	for (int i = 0; c != NULL && i < MAX_STORED_CONNECTIONS; ++i) {
		if (_stored_connections[i].type != CONNECTION_TYPE_CHUNKED_TRANSFER || _stored_connections[i].conn != c) {
			continue;
		}

		connection_info_t* t = _stored_connections + i;

		// send "full" chunk
		if ((t->chunked_transfer.idx+TRANSFER_CHUNK_SIZE) < t->chunked_transfer.len) {
			mg_http_write_chunk(c, t->chunked_transfer.data + t->chunked_transfer.idx, TRANSFER_CHUNK_SIZE);
			t->chunked_transfer.idx += TRANSFER_CHUNK_SIZE;
			return;
		}

		// send remaining chunk
		int remaining = t->chunked_transfer.len - t->chunked_transfer.idx;
		if (remaining > 0) {
			mg_http_write_chunk(c, t->chunked_transfer.data + t->chunked_transfer.idx, remaining);
		}

		// send terminating chunk -> finished
		mg_http_printf_chunk(c, "");

		_http_remove_stored_connection(c);

		break;
	}
}

static void _http_serve_chunked_data_start(struct mg_connection *c, const char* content_type, const byte* data, size_t data_len) {
	// find next free slot for transfer
	int s = _http_get_free_stored_connection_slot();
	if (s < 0) {
		mg_http_reply(c, 507, RESPONSE_HEADERS_HTML, "%s", "Insufficient Storage");
		return;
	}
	
	// start chunked transfer
	mg_printf(c, "HTTP/1.1 200 OK\r\n" RESPONSE_HEADER_CONTENT_TYPE("%s") RESPONSE_HEADERS_GZIP RESPONSE_HEADERS_CHUNKED "\r\n", content_type);
	// insert into transfer slot
	_stored_connections[s].conn = c;
	_stored_connections[s].type = CONNECTION_TYPE_CHUNKED_TRANSFER;
	_stored_connections[s].chunked_transfer.idx = 0;
	_stored_connections[s].chunked_transfer.data = data;
	_stored_connections[s].chunked_transfer.len = data_len;
	// send first chunk
	_http_serve_chunked_data_continue(c);
}

// -------------------------------------------------

static void _send_websocket_message(const xWebsocketData_t* ws_data) {
	for (int i = 0; i < MAX_STORED_CONNECTIONS; ++i) {
		if (_stored_connections[i].type != CONNECTION_TYPE_WS || _stored_connections[i].conn == NULL) {
			continue;
		}

		// TODO: check data type and send JSON - once there are more other types than just log messages
		char type[32] = {0};
		char data[3*MAX_LOG_MSG_SIZE] = {0}; // worst case = 3*MAX_LOG_MSG_SIZE = every character has to be url-encoded 

		switch (ws_data->type) {
			case WEBSOCKET_DATA_TYPE_LOGMSG:
				snprintf(type, sizeof(type), "log");
				mbuspico_urlencode(ws_data->msg, data);
				break;
			default:
				assert(false);
				return;
		}

		char json[256] = {0};
		snprintf(json, sizeof(json), "{\"type\": \"%s\", \"data\": \"%s\"}", type, data); // 24 chars

		connection_info_t* t = _stored_connections + i;
		mg_ws_send(t->conn, json, strlen(json), WEBSOCKET_OP_TEXT);
	}
}

// -------------------------------------------------

static int _http_atoi(struct mg_str* str) {
    int val = 0;
    for (int i = 0; str && i < str->len && str->ptr[i] != '\0'; ++i) {
        val = val * 10 + str->ptr[i] - '0';
	}
    return val;
}

static void http_serve_fn(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {
  	switch (ev) {
		case MG_EV_ERROR:
		{
			MBUSPICO_LOG_E(LOG_TAG_HTTP, (char*)ev_data);
		}
		break;
		case MG_EV_CLOSE:
		{
			_http_remove_stored_connection(c);
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
				if (!_http_check_auth(hm)) {
					mg_http_reply(c, 401, RESPONSE_HEADERS_HTML RESPONSE_UNAUTH_HEADER, "%s", "Unauthorized\n");
					return;
				}
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
			else if (mg_http_match_uri(hm, "/stream")) {
				// find next free slot for transfer
				int s = _http_get_free_stored_connection_slot();
				if (s < 0) {
					mg_http_reply(c, 507, RESPONSE_HEADERS_HTML, "%s", "Insufficient Storage");
					return;
				}
				
				// upgrade websocket connection
				mg_ws_upgrade(c, hm, NULL);
				// insert into transfer slot
				_stored_connections[s].conn = c;
				_stored_connections[s].type = CONNECTION_TYPE_WS;
			}
			// UPDATE
			else if (mg_http_match_uri(hm, "/update")) {
				if (!_http_check_auth(hm)) {
					mg_http_reply(c, 401, RESPONSE_HEADERS_HTML RESPONSE_UNAUTH_HEADER, "%s", "Unauthorized\n");
					return;
				}
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
				if (!_http_check_auth(hm)) {
					mg_http_reply(c, 401, RESPONSE_HEADERS_HTML RESPONSE_UNAUTH_HEADER, "%s", "Unauthorized\n");
					return;
				}
				if (mbuspico_schedule_reboot(2000)) {
					char msg[] = "Device is restarting...";
					mg_http_reply(c, 202, RESPONSE_HEADERS_HTML, "%s\n", msg);
				}
				else {
					char msg[] = "Failed to initiate reboot";
					mg_http_reply(c, 500, RESPONSE_HEADERS_HTML, "%s\n", msg);
				}
			}
			// READ CONFIG DATA
			else if (mg_http_match_uri(hm, "/config/data")) {
				if (!_http_check_auth(hm)) {
					mg_http_reply(c, 401, RESPONSE_HEADERS_HTML RESPONSE_UNAUTH_HEADER, "%s", "Unauthorized\n");
					return;
				}
				char encryption_key[MBUSPICO_CONFIG_SIZE_STR] = {0};
				mbuspico_read_config(MBUSPICO_CONFIG_ENCRYPTION_KEY, encryption_key, sizeof(encryption_key));
				char wifi_ssid[MBUSPICO_CONFIG_SIZE_STR] = {0};
				mbuspico_read_config(MBUSPICO_CONFIG_WIFI_SSID, wifi_ssid, sizeof(wifi_ssid));
				//char wifi_pwd[MBUSPICO_CONFIG_SIZE_STR] = {0};
				//mbuspico_read_config(MBUSPICO_CONFIG_WIFI_PWD, wifi_pwd, sizeof(wifi_pwd));
				char wifi_hostname[MBUSPICO_CONFIG_SIZE_STR] = {0};
				mbuspico_read_config(MBUSPICO_CONFIG_WIFI_HOSTNAME, wifi_hostname, sizeof(wifi_hostname));
				int http_port = -1;
				mbuspico_read_config_num(MBUSPICO_CONFIG_HTTP_PORT, &http_port);
				char http_auth_user[MBUSPICO_CONFIG_SIZE_STR] = {0};
				mbuspico_read_config(MBUSPICO_CONFIG_HTTP_AUTH_USER, http_auth_user, sizeof(http_auth_user));
				char http_auth_pwd[MBUSPICO_CONFIG_SIZE_STR] = {0};
				mbuspico_read_config(MBUSPICO_CONFIG_HTTP_AUTH_PWD, http_auth_pwd, sizeof(http_auth_pwd));
				int udp_enabled = 0;
				mbuspico_read_config_num(MBUSPICO_CONFIG_UDP_ENABLED, &udp_enabled);
				char udp_receiver[MBUSPICO_CONFIG_SIZE_STR] = {0};
				mbuspico_read_config(MBUSPICO_CONFIG_UDP_RECEIVER, udp_receiver, sizeof(udp_receiver));
				int udp_port = -1;
				mbuspico_read_config_num(MBUSPICO_CONFIG_UDP_PORT, &udp_port);
				int udp_interval = -1;
				mbuspico_read_config_num(MBUSPICO_CONFIG_UDP_INTERVAL, &udp_interval);

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
			else {
				// static web resource files
				for (int i = 0; i < sizeof(g_web_resources)/sizeof(struct web_res); ++i) {
					if (mg_http_match_uri(hm, g_web_resources[i].url)) {
						if (!_http_check_auth(hm)) {
							mg_http_reply(c, 401, RESPONSE_HEADERS_HTML RESPONSE_UNAUTH_HEADER, "%s", "Unauthorized\n");
							return;
						}
						_http_serve_chunked_data_start(c, _mime_type_from_filename(g_web_resources[i].url), g_web_resources[i].data, g_web_resources[i].size);
						return;
					}
				}

				// not found
				mg_http_reply(c, 404, RESPONSE_HEADERS_HTML, "%s", "Not Found\n");
			}
		}
		else if (strncmp(hm->method.ptr, "POST", hm->method.len) == 0) {
			// WRITE CONFIG DATA
			if (mg_http_match_uri(hm, "/config/data")) {
				if (!_http_check_auth(hm)) {
					mg_http_reply(c, 401, RESPONSE_HEADERS_HTML RESPONSE_UNAUTH_HEADER, "%s", "Unauthorized\n");
					return;
				}
				
				int saveErr = 0;
				struct mg_http_part part;
				size_t ofs = 0;
				while ((ofs = mg_http_next_multipart(hm->body, ofs, &part)) > 0 && saveErr == 0) {
					MG_DEBUG(("Chunk name: [%.*s] filename: [%.*s] length: %lu bytes",
							(int) part.name.len, part.name.ptr,
							(int) part.filename.len, part.filename.ptr,
							(unsigned long) part.body.len)
					);

					mbuspico_config_t config;
					if (mg_strcmp(part.name, mg_str("meter_key")) == 0) {
						saveErr = mbuspico_write_config(MBUSPICO_CONFIG_ENCRYPTION_KEY, part.body.ptr, part.body.len);
					}
					else if (mg_strcmp(part.name, mg_str("wifi_ssid")) == 0) {
						saveErr = mbuspico_write_config(MBUSPICO_CONFIG_WIFI_SSID, part.body.ptr, part.body.len);
					}
					else if (mg_strcmp(part.name, mg_str("wifi_pwd")) == 0) {
						saveErr = mbuspico_write_config(MBUSPICO_CONFIG_WIFI_PWD, part.body.ptr, part.body.len);
					}
					else if (mg_strcmp(part.name, mg_str("wifi_hostname")) == 0) {
						saveErr = mbuspico_write_config(MBUSPICO_CONFIG_WIFI_HOSTNAME, part.body.ptr, part.body.len);
					}
					else if (mg_strcmp(part.name, mg_str("http_port")) == 0) {
						int val = _http_atoi(&part.body);
						saveErr = mbuspico_write_config_num(MBUSPICO_CONFIG_HTTP_PORT, &val);
					}
					else if (mg_strcmp(part.name, mg_str("http_auth_user")) == 0) {
						saveErr = mbuspico_write_config(MBUSPICO_CONFIG_HTTP_AUTH_USER, part.body.ptr, part.body.len);
					}
					else if (mg_strcmp(part.name, mg_str("http_auth_pwd")) == 0) {
						saveErr = mbuspico_write_config(MBUSPICO_CONFIG_HTTP_AUTH_PWD, part.body.ptr, part.body.len);
					}
					else if (mg_strcmp(part.name, mg_str("udp_enabled")) == 0) {
						int val = _http_atoi(&part.body);
						saveErr = mbuspico_write_config_num(MBUSPICO_CONFIG_UDP_ENABLED, &val);
					}
					else if (mg_strcmp(part.name, mg_str("udp_receiver")) == 0) {
						saveErr = mbuspico_write_config(MBUSPICO_CONFIG_UDP_RECEIVER, part.body.ptr, part.body.len);
					}
					else if (mg_strcmp(part.name, mg_str("udp_port")) == 0) {
						int val = _http_atoi(&part.body);
						saveErr = mbuspico_write_config_num(MBUSPICO_CONFIG_UDP_PORT, &val);
					}
					else if (mg_strcmp(part.name, mg_str("udp_interval")) == 0) {
						int val = _http_atoi(&part.body);
						saveErr = mbuspico_write_config_num(MBUSPICO_CONFIG_UDP_INTERVAL, &val);
					}
					else {
						continue;
					}
				}
				
				if (saveErr == 0) {
					mbuspico_schedule_reboot(2000);
					mg_http_reply(c, 202, RESPONSE_HEADERS_HTML, "%s\n", "Configuration saved. Device is restarting now...");
				}
				else {
					mg_http_reply(c, 500, RESPONSE_HEADERS_HTML, "%s\n", "Failed to save configuration");
				}
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
	
	memset(&_stored_connections, 0, sizeof(_stored_connections));
	g_WebsocketDataQueue = xQueueCreate(25, sizeof(xWebsocketData_t));

	for(;;) {
		int state = mbuspico_wifi_get_state();
		if (state == CYW43_LINK_UP) { // connected and assigned IP
			struct mg_mgr mgr;
			mg_mgr_init(&mgr);
			
			int port = 80;
			mbuspico_read_config_num(MBUSPICO_CONFIG_HTTP_PORT, &port);
			
			char listen_addr[15] = {0};
			snprintf(listen_addr, sizeof(listen_addr), "0.0.0.0:%d", port);
			mg_http_listen(&mgr, listen_addr, http_serve_fn, NULL);
			
			while ((state = mbuspico_wifi_get_state()) == CYW43_LINK_UP) {
				mg_mgr_poll(&mgr, 500);
				
				// send out websocket messages
				xWebsocketData_t data;
				if (xQueueReceive(g_WebsocketDataQueue, &data, 0) == pdPASS) {
					_send_websocket_message(&data);
				}
			}
			
			mg_mgr_free(&mgr);
		}
		
		vTaskDelay(pdMS_TO_TICKS(2000)); // retry / check again in 2 secs
	}
}

#endif // MBUSPICO_HTTP_ENABLED
