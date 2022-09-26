#include <mbuspi.h>
#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"

#ifdef WIFI_ENABLED

#define TAG "WIFI"

static int mbuspi_wifi_state = CYW43_LINK_DOWN;

static int mbuspi_wifi_init() {
	LOG_D(TAG, "mbuspi_wifi_init() called");
	// see pico-sdk/lib/cyw43-driver/src/cyw43_country.h
	// cyw43_arch_init_with_country(CYW43_COUNTRY_AUSTRIA) // CYW43_COUNTRY('A', 'T', 0)
	if (cyw43_arch_init()) {
		LOG_E(TAG, "failed to initialize Wifi");
		return 1;
	}
	LOG_I(TAG, "Wifi initialized");
	
#ifdef WIFI_HOSTNAME
	netif_set_hostname(&cyw43_state.netif[0], WIFI_HOSTNAME);
	LOG_I(TAG, "Hostname set to '%s'", WIFI_HOSTNAME);
#endif
	
	cyw43_arch_enable_sta_mode();
	LOG_I(TAG, "Wifi STA mode enabled");
	
	return 0;
}

int mbuspi_wifi_get_state() {
	return mbuspi_wifi_state;
}

void mbuspi_wifi_task(void* arg) {
	LOG_D(TAG, "mbuspi_wifi_task()");
	
	mbuspi_wifi_init();
	
	int lastState = 0;
	for (;;) {
		int state = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA); //cyw43_wifi_link_status
		//LOG_D(TAG, "Wifi state: %d", state);
		bool doConnect = (state <= 0);
		if (state != mbuspi_wifi_state) {
			mbuspi_wifi_state = state;
			switch (state) {
				case CYW43_LINK_DOWN:	// 0: Wifi down
					LOG_I(TAG, "Wifi state: link down / disconnected");
					break;
				case CYW43_LINK_JOIN:	// 1: Connecting to wifi
					LOG_I(TAG, "Wifi state: connecting");
					break;
				case CYW43_LINK_NOIP:	// 2: Connected to wifi, but no IP address
					LOG_I(TAG, "Wifi state: connected, no ip address yet");
					break;
				case CYW43_LINK_UP:		// 3: Connected to wifi, with IP address
					LOG_I(TAG, "Wifi state: connected, with ip address: %s", ipaddr_ntoa(&cyw43_state.netif[0].ip_addr)); //cyw43_state.netif[0].ip_addr.addr
					break;
				case CYW43_LINK_FAIL:	// -1: Connection failed
					LOG_I(TAG, "Wifi state: connection failed");
					break;
				case CYW43_LINK_NONET:	// -2: No matching SSID found (could be out of range, or down)
					LOG_I(TAG, "Wifi state: SSID not found");
					break;
				case CYW43_LINK_BADAUTH: // -3: Authentication failure
					LOG_I(TAG, "Wifi state: authentication failed");
					break;
				default:
					LOG_I(TAG, "Wifi state: unknown state: %d", state);
					break;
			}
		}
		
		if (doConnect) {
			LOG_I(TAG, "Connecting to Wifi '%s'", WIFI_SSID);
			if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PWD, CYW43_AUTH_WPA2_AES_PSK, 10000)) {
				LOG_E(TAG, "failed to connect Wifi");
			}
			else {
				mbuspi_wifi_state = CYW43_LINK_UP;
				LOG_I(TAG, "Wifi connected, IP address: %s", ipaddr_ntoa(&cyw43_state.netif[0].ip_addr));
			}
		}

		vTaskDelay(10000/portTICK_PERIOD_MS); // retry / check again in 10 secs
	}
}

#endif // WIFI_ENABLED
