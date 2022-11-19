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

xSemaphoreHandle g_HttpConnectionSemaphore;

#define REQUEST_BUFFER_SIZE 	1024
#define RESPONSE_BUFFER_SIZE 	512

static int handle_request(int conn_sock)
{
	char buffer[REQUEST_BUFFER_SIZE] = {0};
	size_t buffer_len = 0;

	uint64_t lastRead = mbuspico_time_ms();
	while (buffer_len < REQUEST_BUFFER_SIZE)
	{
		int done_now = lwip_recv(conn_sock, buffer + buffer_len, REQUEST_BUFFER_SIZE - buffer_len, 0);
		MBUSPICO_LOG_D(LOG_TAG_HTTP, "done_now: %d\n%s", done_now, buffer);
		if (done_now < 0 || (done_now == 0 && ((mbuspico_time_ms()-lastRead)>1000))) {
			return -1;
		}

		lastRead = mbuspico_time_ms();

		buffer_len += done_now;
		char *end = strnstr(buffer, "\r\n\r\n", buffer_len);
		if (!end) {
			continue;
		}
		*end = 0;

		// parse HTTP request
		MBUSPICO_LOG_D(LOG_TAG_HTTP, "Request: [%d]\n%s", buffer_len, buffer);
		
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

	return 413;
}

static int send_response(int socket, int statusCode)
{
	char buffer[RESPONSE_BUFFER_SIZE] = {0};
	size_t buffer_size = 0;
	
	MBUSPICO_LOG_D(LOG_TAG_HTTP, "HTTP: sending response status code %d", statusCode);
	
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
				char data_buffer[RESPONSE_BUFFER_SIZE] = {0};
				size_t data_size = mbuspico_get_meterdata_json(data_buffer, RESPONSE_BUFFER_SIZE);
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

static void do_handle_connection(void* arg)
{
    int conn_sock = (int)arg;

#if configUSE_TRACE_FACILITY && MBUSPICO_LOG_LEVEL >= LOG_DEBUG
	TaskStatus_t xTaskDetails;
	vTaskGetInfo(NULL, &xTaskDetails, pdFALSE, eRunning);
	MBUSPICO_LOG_D(LOG_TAG_HTTP, "START connection task: %s, socket: %d", xTaskDetails.pcTaskName, conn_sock);
#endif

	int statusCode = handle_request(conn_sock);
	if (statusCode > 0) {
		send_response(conn_sock,statusCode);
	}

	lwip_shutdown(conn_sock, SHUT_RDWR);
	lwip_close(conn_sock);

	xSemaphoreGive(g_HttpConnectionSemaphore);
    vTaskDelete(NULL);

#if configUSE_TRACE_FACILITY && MBUSPICO_LOG_LEVEL >= LOG_DEBUG
	MBUSPICO_LOG_D(LOG_TAG_HTTP, "EXIT connection task: %s, socket: %d", xTaskDetails.pcTaskName, conn_sock);
#endif
}

static void handle_connection(int conn_sock)
{
	TaskHandle_t task;
	xSemaphoreTake(g_HttpConnectionSemaphore, portMAX_DELAY);

	static uint32_t connectionCounter = 0;
	char taskName[15];
	snprintf(taskName, sizeof(taskName)-1, "Http#%lu", ++connectionCounter);
	MBUSPICO_LOG_D(LOG_TAG_HTTP, "Creating task for new HTTP connection: %s", taskName);
	xTaskCreate(do_handle_connection, taskName, configMINIMAL_STACK_SIZE, (void *)conn_sock, tskIDLE_PRIORITY+1, &task);
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
		MBUSPICO_LOG_E(LOG_TAG_HTTP, "Unable to create socket: error %d", errno);
		return;
	}

	if (bind(server_sock, (struct sockaddr *)&listen_addr, sizeof(listen_addr)) < 0)
	{
		MBUSPICO_LOG_E(LOG_TAG_HTTP, "Unable to bind socket: error %d", errno);
		return;
	}

	if (listen(server_sock, HTTP_MAX_CONNECTION_COUNT*2) < 0)
	{
		MBUSPICO_LOG_E(LOG_TAG_HTTP, "Unable to listen on socket: error %d\n", errno);
		return;
	}

	MBUSPICO_LOG_I(LOG_TAG_HTTP, "Starting server at %s on port %u", ip4addr_ntoa(netif_ip4_addr(netif_list)), ntohs(listen_addr.sin_port));

	while (true)
	{
		struct sockaddr_storage remote_addr;
		socklen_t len = sizeof(remote_addr);
		int conn_sock = accept(server_sock, (struct sockaddr *)&remote_addr, &len);
		if (conn_sock < 0)
		{
			MBUSPICO_LOG_E(LOG_TAG_HTTP, "Unable to accept incoming connection: error %d", errno);
			return;
		}
		handle_connection(conn_sock);
	}
}

void mbuspico_http_task(void* arg) {
	MBUSPICO_LOG_D(LOG_TAG_HTTP, "mpuspi_http_task()");

	for(;;) {
		int state = mbuspico_wifi_get_state();
		MBUSPICO_LOG_D(LOG_TAG_HTTP, "checking wifi state -> %d", state);
		if (state == CYW43_LINK_UP) { // connected and assigned IP
			run_server();
		}
		vTaskDelay(3000/portTICK_PERIOD_MS); // retry / check again in 3 secs
	}
}

#endif // MBUSPICO_HTTP_ENABLED
