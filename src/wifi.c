#include <mbuspico.h>

#if MBUSPICO_WIFI_ENABLED

#include <pico/cyw43_arch.h>
#include <lwip/pbuf.h>
#include <lwip/tcp.h>

static int mbuspico_wifi_state = CYW43_LINK_DOWN;

int mbuspico_wifi_get_state() {
	return mbuspico_wifi_state;
}

static int mbuspico_wifi_init() {
#ifndef MBUSPICO_WIFI_SSID
	#error "MBUSPICO_WIFI_SSID not defined. Not specified via options?"
#endif
#ifndef MBUSPICO_WIFI_PWD
	#error "MBUSPICO_WIFI_PWD not defined. Not specified via options?"
#endif

	MBUSPICO_LOG_D(LOG_TAG_WIFI, "mbuspi_wifi_init() called");
	// see pico-sdk/lib/cyw43-driver/src/cyw43_country.h
	// cyw43_arch_init_with_country(CYW43_COUNTRY_AUSTRIA) // CYW43_COUNTRY('A', 'T', 0)
	if (cyw43_arch_init()) {
		MBUSPICO_LOG_E(LOG_TAG_WIFI, "failed to initialize Wifi");
		return 1;
	}
	MBUSPICO_LOG_I(LOG_TAG_WIFI, "Wifi initialized");

	MBUSPICO_LOG_I(LOG_TAG_WIFI, "Hostname set to '%s'", CYW43_HOST_NAME);
	
	cyw43_arch_enable_sta_mode();
	MBUSPICO_LOG_I(LOG_TAG_WIFI, "Wifi STA mode enabled");
	
	return 0;
}

void mbuspico_wifi_task(void* arg) {
	MBUSPICO_LOG_D(LOG_TAG_WIFI, "mbuspico_wifi_task()");
	
	mbuspico_wifi_init();
	
	for (;;) {
		int state = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA); //cyw43_wifi_link_status
		//MBUSPICO_LOG_D(LOG_TAG_WIFI, "Wifi state: %d", state);
		bool doConnect = (state <= 0) || (mbuspico_wifi_state <= 0);
		if (state != mbuspico_wifi_state) {
			mbuspico_wifi_state = state;
			switch (state) {
				case CYW43_LINK_DOWN:	// 0: Wifi down
					MBUSPICO_LOG_I(LOG_TAG_WIFI, "Wifi state: link down / disconnected");
					break;
				case CYW43_LINK_JOIN:	// 1: Connecting to wifi
					MBUSPICO_LOG_I(LOG_TAG_WIFI, "Wifi state: connecting");
					break;
				case CYW43_LINK_NOIP:	// 2: Connected to wifi, but no IP address
					MBUSPICO_LOG_I(LOG_TAG_WIFI, "Wifi state: connected, no ip address yet");
					break;
				case CYW43_LINK_UP:		// 3: Connected to wifi, with IP address
					MBUSPICO_LOG_I(LOG_TAG_WIFI, "Wifi state: connected, with ip address: %s", ipaddr_ntoa(&cyw43_state.netif[0].ip_addr)); //cyw43_state.netif[0].ip_addr.addr
					break;
				case CYW43_LINK_FAIL:	// -1: Connection failed
					MBUSPICO_LOG_I(LOG_TAG_WIFI, "Wifi state: connection failed");
					break;
				case CYW43_LINK_NONET:	// -2: No matching SSID found (could be out of range, or down)
					MBUSPICO_LOG_I(LOG_TAG_WIFI, "Wifi state: SSID not found");
					break;
				case CYW43_LINK_BADAUTH: // -3: Authentication failure
					MBUSPICO_LOG_I(LOG_TAG_WIFI, "Wifi state: authentication failed");
					break;
				default:
					MBUSPICO_LOG_I(LOG_TAG_WIFI, "Wifi state: unknown state: %d", state);
					break;
			}
		}
		
		if (doConnect) {
			MBUSPICO_LOG_I(LOG_TAG_WIFI, "Connecting to Wifi '%s'", MBUSPICO_WIFI_SSID);
			if (cyw43_arch_wifi_connect_timeout_ms(MBUSPICO_WIFI_SSID, MBUSPICO_WIFI_PWD, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
				MBUSPICO_LOG_E(LOG_TAG_WIFI, "failed to connect Wifi");
				mbuspico_wifi_state = CYW43_LINK_FAIL; // try again
			}
			else {
				mbuspico_wifi_state = CYW43_LINK_UP;
				MBUSPICO_LOG_I(LOG_TAG_WIFI, "Wifi connected, IP address: %s", ipaddr_ntoa(&cyw43_state.netif[0].ip_addr));
			}
		}

		vTaskDelay(pdMS_TO_TICKS(3000)); // retry / check again in 3 secs
	}
}

#endif // MBUSPICO_WIFI_ENABLED
