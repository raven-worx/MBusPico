#include <mbus-rpi.h>

#define DEV_NAME "Kaifa_MA309M"

using namespace MBusPi;

void Device::Init() {
	LOG_D("Device::Init() called - " DEV_NAME)
}

void Device::RunTask(void*) {
	xMBusData_t data;
	for (;;) {
		LOG_D("Device: Check device queue...")
		if (xQueueReceive(xDeviceEventQueue, &data, portMAX_DELAY) == pdPASS) 
		{ 
			
		}
		//vTaskDelay(1000);
	}
}