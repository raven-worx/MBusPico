#include <mbus-rpi.h>

using namespace MBusPi;

#ifdef HTTP_ENABLED

// https://github.com/raspberrypi/pico-examples/blob/master/pico_w/tcp_server/picow_tcp_server.c

void HTTP::Init() {
	LOG_D("HTTP::Init() called");
}

void HTTP::RunTask(void*) {
	while(true){
		LOG_D("HTTP::RunTask()")
		vTaskDelay(5000);
	}
}

#endif // HTTP_ENABLED
