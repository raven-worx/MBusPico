#include <mbuspico.h>

#if MBUSPICO_HTTP_ENABLED

#include <string.h>
#include <stdlib.h>
#include <pico/stdlib.h>
#include <pico/cyw43_arch.h>
#include <lwip/pbuf.h>
#include <lwip/tcp.h>
#include <lwip/apps/lwiperf.h>
#include <lwip/ip4_addr.h>
#include <lwip/netif.h>
#include <lwip/sockets.h>

#include <FreeRTOS.h>
#include <task.h>

#define TAG "HTTP"

// #define HTTP_TASK_PRIORITY (tskIDLE_PRIORITY + 2UL)

#define REQUEST_BUFFER_SIZE 	768
#define RESPONSE_BUFFER_SIZE 	768

static int handle_request(int conn_sock)
{
	char buffer[REQUEST_BUFFER_SIZE] = {0};
	size_t buffer_len = 0;

	while (buffer_len < REQUEST_BUFFER_SIZE)
	{
		int done_now = recv(conn_sock, buffer + buffer_len, REQUEST_BUFFER_SIZE - buffer_len, 0);
		if (done_now <= 0) {
			return -1;
		}
		buffer_len += done_now;
		char *end = strnstr(buffer, "\r\n\r\n", buffer_len);
		if (!end) {
			continue;
		}
		*end = 0;

		// parse HTTP request
		MBUSPICO_LOG_D(TAG, "Request: [%d]\n%s", buffer_len, buffer);
		
		int status_code = 0;
		
		if (strstr(buffer,"GET ") == buffer) { // starts with GET
			if (strstr(buffer,"GET / ") == buffer) { // only support root path for now
				status_code = 200;
			}
			else {
				status_code = 404;
			}
		}
		else {
			status_code = 405;
		}
		
		return status_code;
	}

	return 0;
}

static int send_response(int socket, int statusCode)
{
	char buffer[RESPONSE_BUFFER_SIZE] = {0};
	size_t buffer_size = 0;
	
	MBUSPICO_LOG_D(TAG, "HTTP: sending response status code %d", statusCode);
	
	// generate raw HTTP response
	switch (statusCode) {
		case 200:
			{
				const char* response_tmpl = 
					"HTTP/1.1 200 OK" "\r\n"
					"Server: " PROJECT_NAME "/" PROJECT_VERSION "\r\n"
					"Content-Length: %d" "\r\n"
					"Content-Type: application/json" "\r\n"
					"Connection: close" "\r\n"
					"\r\n"
					"%s";
				char data_buffer[DATA_BUFFER_SIZE] = {0};
				size_t data_size = mbuspico_get_values_json(data_buffer);
				buffer_size = snprintf(buffer, RESPONSE_BUFFER_SIZE, response_tmpl, data_size, data_buffer);
			}
			break;
		case 400:
			{
				const char* response_tmpl = 
					"HTTP/1.1 400 Bad Request" "\r\n"
					"Server: " PROJECT_NAME "/" PROJECT_VERSION "\r\n"
					"Content-Length: 0" "\r\n"
					"Connection: close" "\r\n"
					;
				buffer_size = snprintf(buffer, RESPONSE_BUFFER_SIZE, response_tmpl);
			}
			break;
		case 404:
			{
				const char* response_tmpl = 
					"HTTP/1.1 404 Not Found" "\r\n"
					"Server: " PROJECT_NAME "/" PROJECT_VERSION "\r\n"
					"Content-Length: 0" "\r\n"
					"Connection: close" "\r\n"
					;
				buffer_size = snprintf(buffer, RESPONSE_BUFFER_SIZE, response_tmpl);
			}
			break;
		case 405:
			{
				const char* response_tmpl = 
					"HTTP/1.1 405 Not Allowed" "\r\n"
					"Server: " PROJECT_NAME "/" PROJECT_VERSION "\r\n"
					"Content-Length: 0" "\r\n"
					"Connection: close" "\r\n"
					;
				buffer_size = snprintf(buffer, RESPONSE_BUFFER_SIZE, response_tmpl);
			}
			break;
		case 406:
			{
				const char* response_tmpl = 
					"HTTP/1.1 406 Not Acceptable" "\r\n"
					"Server: " PROJECT_NAME "/" PROJECT_VERSION "\r\n"
					"Content-Length: 0" "\r\n"
					"Connection: close" "\r\n"
					;
				buffer_size = snprintf(buffer, RESPONSE_BUFFER_SIZE, response_tmpl);
			}
			break;
		case 413:
			{
				const char* response_tmpl = 
					"HTTP/1.1 413 Payload Too Large" "\r\n"
					"Server: " PROJECT_NAME "/" PROJECT_VERSION "\r\n"
					"Content-Length: 0" "\r\n"
					"Connection: close" "\r\n"
					;
				buffer_size = snprintf(buffer, RESPONSE_BUFFER_SIZE, response_tmpl);
			}
			break;
		default:
			return -1;
	}

	int len = strlen(buffer);
	int done = 0;
	while (done < len)
	{
		int done_now = send(socket, buffer + done, len - done, 0);
		if (done_now <= 0) {
			return -2;
		}
		done += done_now;
	}

	return 0;
}


static void handle_connection(int conn_sock)
{
	int statusCode;
	do {
		statusCode = handle_request(conn_sock);
	} while(statusCode <= 0);

	send_response(conn_sock,statusCode);

	closesocket(conn_sock);
}

static void run_server()
{
#ifdef MBUSPICO_HTTP_SERVER_PORT
	uint16_t port = MBUSPICO_HTTP_SERVER_PORT;
#else
	uint16_t port = 80;
#endif
	int server_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
	struct sockaddr_in listen_addr =
		{
			.sin_len = sizeof(struct sockaddr_in),
			.sin_family = AF_INET,
			.sin_port = htons(port),
			.sin_addr = 0,
		};

	if (server_sock < 0)
	{
		MBUSPICO_LOG_E(TAG, "Unable to create socket: error %d", errno);
		return;
	}

	if (bind(server_sock, (struct sockaddr *)&listen_addr, sizeof(listen_addr)) < 0)
	{
		MBUSPICO_LOG_E(TAG, "Unable to bind socket: error %d", errno);
		return;
	}

	if (listen(server_sock, 1) < 0)
	{
		MBUSPICO_LOG_E(TAG, "Unable to listen on socket: error %d\n", errno);
		return;
	}

	MBUSPICO_LOG_I(TAG, "Starting server at %s on port %u", ip4addr_ntoa(netif_ip4_addr(netif_list)), ntohs(listen_addr.sin_port));

	while (true)
	{
		struct sockaddr_storage remote_addr;
		socklen_t len = sizeof(remote_addr);
		int conn_sock = accept(server_sock, (struct sockaddr *)&remote_addr, &len);
		if (conn_sock < 0)
		{
			MBUSPICO_LOG_E(TAG, "Unable to accept incoming connection: error %d", errno);
			return;
		}
		handle_connection(conn_sock);
	}
}

void mbuspico_http_task(void* arg) {
	MBUSPICO_LOG_D(TAG, "mpuspi_http_task()");

	for(;;) {
		MBUSPICO_LOG_D(TAG, "checking wifi state");
		if (mbuspico_wifi_get_state() == CYW43_LINK_UP) { // connected and assigned IP
			run_server();
		}
		vTaskDelay(5000/portTICK_PERIOD_MS); // retry / check again in 5 secs
	}
}

#endif // MBUSPICO_HTTP_ENABLED
