#include <mbuspi.h>

#define TAG "DEVICE"

const char* mbuspi_device_name() {
	return "Kaifa_MA309M";
}

static void mbuspi_device_init() {
}

void mbuspi_device_task(void* arg) {
	mbuspi_device_init();
	
	xMBusData_t data;
	for (;;) {
		LOG_D(TAG, "Device: Check device queue...");
		if (xQueueReceive(xDeviceEventQueue, &data, portMAX_DELAY) == pdPASS) 
		{ 
			
		}
		//vTaskDelay(1000/portTICK_PERIOD_MS);
	}
}