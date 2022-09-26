#include <mbuspi.h>
#include <stdio.h>
#include <pico/stdlib.h>

int main() {
	stdio_init_all();
	
	BaseType_t result;
	
	mbuspi_init();
	
#ifdef WIFI_ENABLED
	TaskHandle_t handleWifiTask;
	result = xTaskCreate(mbuspi_wifi_task, "WIFI_Task", configMINIMAL_STACK_SIZE*8, NULL, tskIDLE_PRIORITY+1, &handleWifiTask);
	printf("WIFI task creation: %s\n", result == pdPASS ? "SUCCESS" : "FAILED" );
	// we must bind the wifi task to one core (well at least while the init is called)
	// (note we only do this in NO_SYS mode, because cyw43_arch_freertos takes care of it otherwise)
	vTaskCoreAffinitySet(handleWifiTask, 1);
#endif
	
	//xTaskCreate(mbuspi_device_task, "Device_Task", configMINIMAL_STACK_SIZE*8, NULL, tskIDLE_PRIORITY+1, NULL);

	//xTaskCreate(mbuspi_uart_task, "UART_Task", configMINIMAL_STACK_SIZE*8, NULL, tskIDLE_PRIORITY+1, NULL);

#ifdef HTTP_ENABLED
	result = xTaskCreate(mbuspi_http_task, "HTTP_Task", configMINIMAL_STACK_SIZE*8, NULL, tskIDLE_PRIORITY+1, NULL);
	printf("HTTP task creation: %s\n", result == pdPASS ? "SUCCESS" : "FAILED" );
#endif

#ifdef UDP_ENABLED
	result = xTaskCreate(mbuspi_udp_task, "UDP_Task", configMINIMAL_STACK_SIZE*8, NULL, tskIDLE_PRIORITY+1, NULL);
	printf("UDP task creation: %s\n", result == pdPASS ? "SUCCESS" : "FAILED" );
#endif

	vTaskStartScheduler();

#if LOG_LEVEL >= LOG_DEBUG
	printf("=== EXIT ===\n");
#endif
	return 0;
}