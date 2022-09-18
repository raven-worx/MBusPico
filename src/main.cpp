#include <mbus-rpi.h>
#include <stdio.h>
#include <pico/stdlib.h>

using namespace MBusPi;

int main() {
	stdio_init_all();
	
	MBusPi::Init();
	
	TaskHandle_t hWifiTask;
	xTaskCreate(Wifi::RunTask, "Wifi_Task", 1024, NULL, tskIDLE_PRIORITY+1, &hWifiTask);
	// we must bind the wifi task to one core (well at least while the init is called)
	// (note we only do this in NO_SYS mode, because cyw43_arch_freertos takes care of it otherwise)
	vTaskCoreAffinitySet(hWifiTask, 1);
	
	xTaskCreate(Device::RunTask, "Device_Task", 1024, NULL, tskIDLE_PRIORITY+1, NULL);

	xTaskCreate(UART::RunTask, "UART_Task", 1024, NULL, tskIDLE_PRIORITY+1, NULL);

#ifdef HTTP_ENABLED
	xTaskCreate(HTTP::RunTask, "HTTP_Task", 1024, NULL, tskIDLE_PRIORITY+1, NULL);
#endif

#ifdef UDP_ENABLED
	xTaskCreate(UDP::RunTask, "UDP_Task", 1024, NULL, tskIDLE_PRIORITY+1, NULL);
#endif

	vTaskStartScheduler();

#if LOG_LEVEL >= LOG_DEBUG
	printf("=== EXIT ===\n");
#endif
	return 0;
}