#include <mbuspico.h>

#if MBUSPICO_UDP_ENABLED

#include <pico/cyw43_arch.h>
#include <lwip/pbuf.h>
#include <lwip/tcp.h>
#include <lwip/apps/lwiperf.h>
#include <lwip/ip4_addr.h>
#include <lwip/netif.h>
#include <lwip/sockets.h>

#define DATA_BUFFER_SIZE 	512

static uint64_t last_send = 0;

static const send_interval = 1000*
#ifdef MBUSPICO_UDP_INTERVAL_S
  MBUSPICO_UDP_INTERVAL_S
#else
  30
#endif
;

static void mbuspico_send_udp() {
	if (last_send > 0 && ((mbuspico_time_ms() - last_send) < send_interval)) {
		return;
	}

	char data_buffer[DATA_BUFFER_SIZE] = {0};
	size_t data_len = mbuspico_get_meterdata_json(data_buffer, DATA_BUFFER_SIZE);

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
		MBUSPICO_LOG_E(LOG_TAG_UDP, "Unable to create socket: errno %d", errno);
		return;
	}

	MBUSPICO_LOG_I(LOG_TAG_UDP, "Socket created, sending to %s:%d", MBUSPICO_UDP_RECEIVER_HOST, port);
	MBUSPICO_LOG_D(LOG_TAG_UDP, "  data length: %d", data_len);

	int err = lwip_sendto(sock, data_buffer, data_len, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
	if (err < 0) {
		MBUSPICO_LOG_E(LOG_TAG_UDP, "Error occurred during sending: errno %d", errno);
		return;
	} else {
		MBUSPICO_LOG_D(LOG_TAG_UDP, "Message sent: %d", err);
		last_send = mbuspico_time_ms();
	}
	
	if (sock != -1) {
		MBUSPICO_LOG_D(LOG_TAG_UDP, "Shutting down socket");
		lwip_shutdown(sock, SHUT_RDWR);
		lwip_close(sock);
	}
}

static void mbuspico_udp_init() {
	MBUSPICO_LOG_D(LOG_TAG_UDP, "mbuspico_udp_init()");

#ifndef MBUSPICO_UDP_RECEIVER_HOST
	#error "MBUSPICO_UDP_RECEIVER_HOST not defined. Not specified via options?"
#endif
#ifndef MBUSPICO_UDP_RECEIVER_PORT
	#error "MBUSPICO_UDP_RECEIVER_PORT not defined. Not specified via options?"
#endif
}

void mbuspico_udp_task(void* arg) {
	MBUSPICO_LOG_D(LOG_TAG_UDP, "mbuspico_udp_task()");
	
	mbuspico_udp_init();
	
	for(;;) {
		if (mbuspico_wifi_get_state() == CYW43_LINK_UP) { // connected and assigned IP
			mbuspico_send_udp();
		}
		vTaskDelay(100/portTICK_PERIOD_MS);
	}
}

#endif // MBUSPICO_UDP_ENABLED
