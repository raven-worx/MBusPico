#include <mbuspico.h>

#if MBUSPICO_WIFI_ENABLED

#include <pico/cyw43_arch.h>
#include <lwip/pbuf.h>
#include <lwip/tcp.h>
#include "net/dhcpserver.h"
#include "net/dnsserver.h"

#define MAX_FAILED_CONNECT_ATTEMPTS 5

static int failed_attempts = 0;
static mbuspico_wifi_state_t mbuspico_wifi_state;

int mbuspico_wifi_get_state() {
	return mbuspico_wifi_state.link_state;
}

int mbuspico_wifi_get_mode() {
	return mbuspico_wifi_state.mode;
}

static void _mbuspico_wifi_sta_start() {
	cyw43_arch_enable_sta_mode();
	MBUSPICO_LOG_I(LOG_TAG_WIFI, "Wifi STA mode enabled");
	mbuspico_wifi_state.mode = MBUSPICO_WIFI_MODE_STA;
	failed_attempts = 0;
}

static void _mbuspico_wifi_sta_stop() {
	cyw43_arch_disable_sta_mode();
	mbuspico_wifi_state.link_state = CYW43_LINK_DOWN;
	mbuspico_wifi_state.mode = MBUSPICO_WIFI_MODE_NOMODE;
	MBUSPICO_LOG_I(LOG_TAG_WIFI, "Wifi STA mode disabled");
}

static void _mbuspico_wifi_ap_start() {
	cyw43_arch_enable_ap_mode("MBusPico", "mbuspico", CYW43_AUTH_WPA_TKIP_PSK);
	MBUSPICO_LOG_I(LOG_TAG_WIFI, "Wifi AP mode enabled");
	mbuspico_wifi_state.mode = MBUSPICO_WIFI_MODE_AP;

	BaseType_t result;
	
	result = xTaskCreate(dhcp_server_task, "DHCPServer_Task", configMINIMAL_STACK_SIZE, &mbuspico_wifi_state, tskIDLE_PRIORITY+1, NULL);
	if (result != pdPASS) {
		MBUSPICO_LOG_E(LOG_TAG_WIFI, "Failed to create DHCPServer task!");
	}
}

static void _mbuspico_wifi_ap_stop() {
    dhcp_server_deinit(&mbuspico_wifi_state.dhcp_server);	// Stop the DHCP server
	cyw43_arch_disable_ap_mode();
	mbuspico_wifi_state.link_state = CYW43_LINK_DOWN;
	mbuspico_wifi_state.mode = MBUSPICO_WIFI_MODE_NOMODE;
	MBUSPICO_LOG_I(LOG_TAG_WIFI, "Wifi AP mode disabled");
}

static int mbuspico_wifi_init() {
#ifndef MBUSPICO_WIFI_SSID
	#error "MBUSPICO_WIFI_SSID not defined. Not specified via options?"
#endif
#ifndef MBUSPICO_WIFI_PWD
	#error "MBUSPICO_WIFI_PWD not defined. Not specified via options?"
#endif

	MBUSPICO_LOG_D(LOG_TAG_WIFI, "mbuspi_wifi_init() called");

	mbuspico_wifi_state.link_state = CYW43_LINK_DOWN;
	mbuspico_wifi_state.mode = MBUSPICO_WIFI_MODE_NOMODE;
	failed_attempts = 0;
	IP4_ADDR(ip_2_ip4(&mbuspico_wifi_state.gw), 192, 168, 4, 1);		// IP: 192.168.4.1
    IP4_ADDR(ip_2_ip4(&mbuspico_wifi_state.mask), 255, 255, 255, 0);	// Subnet mask: 255.255.255.0

	// see pico-sdk/lib/cyw43-driver/src/cyw43_country.h
	// cyw43_arch_init_with_country(CYW43_COUNTRY_AUSTRIA) // CYW43_COUNTRY('A', 'T', 0)
	if (cyw43_arch_init()) {
		MBUSPICO_LOG_E(LOG_TAG_WIFI, "failed to initialize Wifi");
		return 1;
	}
	MBUSPICO_LOG_I(LOG_TAG_WIFI, "Wifi initialized");

	MBUSPICO_LOG_I(LOG_TAG_WIFI, "Hostname set to '%s'", CYW43_HOST_NAME);
	
	return 0;
}

void mbuspico_wifi_task(void* arg) {
	MBUSPICO_LOG_D(LOG_TAG_WIFI, "mbuspico_wifi_task()");
	
	mbuspico_wifi_init();

	// TODO: check settings if to start in STA mode
	_mbuspico_wifi_sta_start();

	for (;;) {
		switch (mbuspico_wifi_state.mode) {
			case MBUSPICO_WIFI_MODE_STA:
			{
				int state = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA); //cyw43_wifi_link_status
				//MBUSPICO_LOG_D(LOG_TAG_WIFI, "Wifi state: %d", state);
				bool doConnect = (state <= 0) || (mbuspico_wifi_state.link_state <= 0);
				if (state != mbuspico_wifi_state.link_state) {
					mbuspico_wifi_state.link_state = state;
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
					if (cyw43_arch_wifi_connect_timeout_ms(MBUSPICO_WIFI_SSID, MBUSPICO_WIFI_PWD, CYW43_AUTH_WPA2_AES_PSK, 10000)) {
						MBUSPICO_LOG_E(LOG_TAG_WIFI, "failed to connect Wifi");
						mbuspico_wifi_state.link_state = CYW43_LINK_FAIL; // try again
						failed_attempts += 1;
					}
					else {
						mbuspico_wifi_state.link_state = CYW43_LINK_UP;
						MBUSPICO_LOG_I(LOG_TAG_WIFI, "Wifi connected, IP address: %s", ipaddr_ntoa(&cyw43_state.netif[0].ip_addr));
					}
				}

				if (failed_attempts >= MAX_FAILED_CONNECT_ATTEMPTS) {
					_mbuspico_wifi_sta_stop();
					break;
				}

				vTaskDelay(pdMS_TO_TICKS(2000)); // retry / check again in 2 secs
			}
			break;
			case MBUSPICO_WIFI_MODE_NOMODE:
			{
				vTaskDelay(pdMS_TO_TICKS(1000));
				_mbuspico_wifi_ap_start();
			}
			case MBUSPICO_WIFI_MODE_AP:
			{
				int state = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_AP); //cyw43_wifi_link_status
				//MBUSPICO_LOG_D(LOG_TAG_WIFI, "Wifi AP Link state: %d", state);
				if (state != mbuspico_wifi_state.link_state) {
					mbuspico_wifi_state.link_state = state;
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

				vTaskDelay(pdMS_TO_TICKS(10));
			}
			break;
			default:
				break;
		}
	}
}

#endif // MBUSPICO_WIFI_ENABLED
