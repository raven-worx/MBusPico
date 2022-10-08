#include <mbuspico.h>
#include <mbedtls/gcm.h>

#define TAG "DEVICE"

const char* mbuspico_device_name() {
	return "Kaifa_MA309M";
}

static void mbuspico_device_init() {
}

void mbuspico_device_task(void* arg) {
	mbuspico_device_init();
	
	xMBusData_t data;
	for (;;) {
		MBUSPICO_LOG_D(TAG, "Device: Check device queue...");
		if (xQueueReceive(xDeviceEventQueue, &data, portMAX_DELAY) == pdPASS) 
		{ 
			
		}
		//vTaskDelay(1000/portTICK_PERIOD_MS);
	}
}