#include <mbus-rpi.h>
#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"
 
char ssid[] = "MyWifi";
char pass[] = "A Password";

using namespace MBusPi;

int Wifi::Init() {
	LOG_D("Wifi::Init() called")
	// see pico-sdk/lib/cyw43-driver/src/cyw43_country.h
	if (cyw43_arch_init_with_country(CYW43_COUNTRY_AUSTRIA)) {
		LOG_E("failed to initialise Wifi")
		return 1;
	}
	LOG_I("Wifi initialized")
	
	cyw43_arch_enable_sta_mode();
	LOG_I("Wifi STA mode enabled")
	return 0;
}

void Wifi::RunTask(void*) {
	LOG_D("Wifi::RunTask()")
	
	Wifi::Init();
	
	int lastState = 0;
	for (;;) {
		int state = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA); //cyw43_wifi_link_status
		//LOG_D("Wifi state: %d", state);
		bool doConnect = (state <= 0);
		if (state != lastState) {
			lastState = state;
			switch (state) {
				case CYW43_LINK_DOWN:	// 0: Wifi down
					LOG_I("Wifi state: link down / disconnected");
					break;
				case CYW43_LINK_JOIN:	// 1: Connecting to wifi
					LOG_I("Wifi state: connecting");
					break;
				case CYW43_LINK_NOIP:	// 2: Connected to wifi, but no IP address
					LOG_I("Wifi state: connected, no ip address yet");
					break;
				case CYW43_LINK_UP:		// 3: Connected to wifi, with IP address
					LOG_I("Wifi state: connected, with ip address: %s", ipaddr_ntoa(&cyw43_state.netif[0].ip_addr)); //cyw43_state.netif[0].ip_addr.addr
					break;
				case CYW43_LINK_FAIL:	// -1: Connection failed
					LOG_I("Wifi state: connection failed");
					break;
				case CYW43_LINK_NONET:	// -2: No matching SSID found (could be out of range, or down)
					LOG_I("Wifi state: SSID not found");
					break;
				case CYW43_LINK_BADAUTH: // -3: Authentication failure
					LOG_I("Wifi state: authentication failed");
					break;
			}
		}
		
		if (doConnect) {
			LOG_I("Connecting to Wifi '%s'",ssid)
			if (cyw43_arch_wifi_connect_timeout_ms(ssid, pass, CYW43_AUTH_WPA2_AES_PSK, 10000)) {
				LOG_E("failed to connect Wifi")
			}
			else {
				LOG_I("Wifi connected")
			}
		}
		else {
			vTaskDelay(10000/portTICK_PERIOD_MS); // retry / check again in 10 secs
		}
	}
}
