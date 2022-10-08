#include <mbuspico.h>

#if MBUSPICO_WIFI_ENABLED

#include <pico/cyw43_arch.h>
#include <lwip/pbuf.h>
#include <lwip/tcp.h>

#define TAG "WIFI"

static int mbuspico_wifi_state = CYW43_LINK_DOWN;

static int mbuspico_wifi_init() {
	MBUSPICO_LOG_D(TAG, "mbuspi_wifi_init() called");
	// see pico-sdk/lib/cyw43-driver/src/cyw43_country.h
	// cyw43_arch_init_with_country(CYW43_COUNTRY_AUSTRIA) // CYW43_COUNTRY('A', 'T', 0)
	if (cyw43_arch_init()) {
		MBUSPICO_LOG_E(TAG, "failed to initialize Wifi");
		return 1;
	}
	MBUSPICO_LOG_I(TAG, "Wifi initialized");
	
#ifdef MBUSPICO_WIFI_HOSTNAME
	const char hostname[] = MBUSPICO_WIFI_HOSTNAME;
#else
	const char hostname[] = PROJECT_NAME;
#endif
	if (strlen(hostname) > 0) {
		netif_set_hostname(&cyw43_state.netif[0], hostname);
		MBUSPICO_LOG_I(TAG, "Hostname set to '%s'", hostname);
	}
	
	cyw43_arch_enable_sta_mode();
	MBUSPICO_LOG_I(TAG, "Wifi STA mode enabled");
	
	return 0;
}

int mbuspico_wifi_get_state() {
	return mbuspico_wifi_state;
}

void mbuspico_wifi_task(void* arg) {
	MBUSPICO_LOG_D(TAG, "mbuspico_wifi_task()");
	
	mbuspico_wifi_init();
	
	int lastState = 0;
	for (;;) {
		int state = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA); //cyw43_wifi_link_status
		//MBUSPICO_LOG_D(TAG, "Wifi state: %d", state);
		bool doConnect = (state <= 0);
		if (state != mbuspico_wifi_state) {
			mbuspico_wifi_state = state;
			switch (state) {
				case CYW43_LINK_DOWN:	// 0: Wifi down
					MBUSPICO_LOG_I(TAG, "Wifi state: link down / disconnected");
					break;
				case CYW43_LINK_JOIN:	// 1: Connecting to wifi
					MBUSPICO_LOG_I(TAG, "Wifi state: connecting");
					break;
				case CYW43_LINK_NOIP:	// 2: Connected to wifi, but no IP address
					MBUSPICO_LOG_I(TAG, "Wifi state: connected, no ip address yet");
					break;
				case CYW43_LINK_UP:		// 3: Connected to wifi, with IP address
					MBUSPICO_LOG_I(TAG, "Wifi state: connected, with ip address: %s", ipaddr_ntoa(&cyw43_state.netif[0].ip_addr)); //cyw43_state.netif[0].ip_addr.addr
					break;
				case CYW43_LINK_FAIL:	// -1: Connection failed
					MBUSPICO_LOG_I(TAG, "Wifi state: connection failed");
					break;
				case CYW43_LINK_NONET:	// -2: No matching SSID found (could be out of range, or down)
					MBUSPICO_LOG_I(TAG, "Wifi state: SSID not found");
					break;
				case CYW43_LINK_BADAUTH: // -3: Authentication failure
					MBUSPICO_LOG_I(TAG, "Wifi state: authentication failed");
					break;
				default:
					MBUSPICO_LOG_I(TAG, "Wifi state: unknown state: %d", state);
					break;
			}
		}
		
		if (doConnect) {
			MBUSPICO_LOG_I(TAG, "Connecting to Wifi '%s'", MBUSPICO_WIFI_SSID);
			if (cyw43_arch_wifi_connect_timeout_ms(MBUSPICO_WIFI_SSID, MBUSPICO_WIFI_PWD, CYW43_AUTH_WPA2_AES_PSK, 10000)) {
				MBUSPICO_LOG_E(TAG, "failed to connect Wifi");
			}
			else {
				mbuspico_wifi_state = CYW43_LINK_UP;
				MBUSPICO_LOG_I(TAG, "Wifi connected, IP address: %s", ipaddr_ntoa(&cyw43_state.netif[0].ip_addr));
			}
		}

		vTaskDelay(10000/portTICK_PERIOD_MS); // retry / check again in 10 secs
	}
}

#endif // MBUSPICO_WIFI_ENABLED
