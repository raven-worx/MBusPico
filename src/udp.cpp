#include <mbus-rpi.h>

using namespace MBusPi;

#ifdef UDP_ENABLED

void UDP::Init() {
	LOG_D("UDP::Init() called")
}

void UDP::RunTask(void*) {
	
	for (;;) {
		LOG_D("UDP::RunTask()")
		//if( xQueueReceive(eventQueue, &i, portMAX_DELAY) == pdPASS ) 
		{ 
			
		}
		vTaskDelay(5000);
	}
}

#endif // UDP_ENABLED
