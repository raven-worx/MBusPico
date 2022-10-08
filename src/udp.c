#include <mbuspico.h>

#if MBUSPICO_UDP_ENABLED

#include <pico/cyw43_arch.h>
#include <lwip/pbuf.h>
#include <lwip/tcp.h>
#include <lwip/apps/lwiperf.h>
#include <lwip/ip4_addr.h>
#include <lwip/netif.h>
#include <lwip/sockets.h>

#define TAG "UDP"

static void mbuspico_send_udp() {
	//if( xQueueReceive(eventQueue, &i, portMAX_DELAY) == pdPASS ) 
	{
		char data_buffer[DATA_BUFFER_SIZE] = {0};
		size_t data_len = mbuspico_get_values_json(data_buffer);

		char host_ip[] = MBUSPICO_UDP_RECEIVER_HOST;
		uint16_t port = MBUSPICO_UDP_RECEIVER_PORT;
		int addr_family = 0;
		int ip_protocol = 0;
		
		struct sockaddr_in dest_addr = {
			.sin_addr.s_addr = inet_addr(MBUSPICO_UDP_RECEIVER_HOST),
			.sin_family = AF_INET,
			.sin_port = htons(port)
		};
		
		int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
		if (sock < 0) {
			MBUSPICO_LOG_E(TAG, "Unable to create socket: errno %d", errno);
			return;
		}

		MBUSPICO_LOG_I(TAG, "Socket created, sending to %s:%d", MBUSPICO_UDP_RECEIVER_HOST, port);
		MBUSPICO_LOG_D(TAG, "  data length: %d", data_len);

		int err = sendto(sock, data_buffer, data_len, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
		if (err < 0) {
			MBUSPICO_LOG_E(TAG, "Error occurred during sending: errno %d", errno);
			return;
		} else {
			MBUSPICO_LOG_D(TAG, "Message sent: %d", err);
		}
		
		if (sock != -1) {
			MBUSPICO_LOG_D(TAG, "Shutting down socket");
			shutdown(sock, 0);
			close(sock);
		}
	}
}

static void mbuspico_udp_init() {
	MBUSPICO_LOG_D(TAG, "mbuspico_udp_init()");
}

void mbuspico_udp_task(void* arg) {
	MBUSPICO_LOG_D(TAG, "mbuspico_udp_task()");
	
	mbuspico_udp_init();
	
	for(;;) {
		MBUSPICO_LOG_D(TAG, "checking wifi state");
		if (mbuspico_wifi_get_state() == CYW43_LINK_UP) { // connected and assigned IP
			mbuspico_send_udp();
		}
		vTaskDelay(5000/portTICK_PERIOD_MS);
	}
}

#endif // MBUSPICO_UDP_ENABLED
