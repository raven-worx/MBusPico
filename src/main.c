#include <mbuspi.h>
#include <stdio.h>
#include <pico/stdlib.h>


#if configCHECK_FOR_STACK_OVERFLOW
void vApplicationStackOverflowHook( TaskHandle_t xTask, char* pcTaskName ) {
	printf("OVERFLOW of task '%s'\n", pcTaskName);
}
#endif

#if configUSE_MALLOC_FAILED_HOOK
void vApplicationMallocFailedHook( void ) {
	printf("MALLOC failed\n");
}
#endif

int main() {
	stdio_init_all();
	
	BaseType_t result;
	
	mbuspi_init();
	
#ifdef WIFI_ENABLED
	TaskHandle_t handleWifiTask;
	result = xTaskCreate(mbuspi_wifi_task, "WIFI_Task", configMINIMAL_STACK_SIZE*8, NULL, tskIDLE_PRIORITY+1, &handleWifiTask);
	#if LOG_LEVEL >= LOG_DEBUG
	printf("WIFI task creation: %s\n", result == pdPASS ? "SUCCESS" : "FAILED" );
	#endif
	// we must bind the wifi task to one core (well at least while the init is called)
	// (note we only do this in NO_SYS mode, because cyw43_arch_freertos takes care of it otherwise)
	vTaskCoreAffinitySet(handleWifiTask, 1);
#endif
	
	result = xTaskCreate(mbuspi_device_task, "Device_Task", configMINIMAL_STACK_SIZE*8, NULL, tskIDLE_PRIORITY+1, NULL);
	#if LOG_LEVEL >= LOG_DEBUG
	printf("Device task creation: %s\n", result == pdPASS ? "SUCCESS" : "FAILED" );
	#endif

#if 0
	result = xTaskCreate(mbuspi_uart_task, "UART_Task", configMINIMAL_STACK_SIZE*8, NULL, tskIDLE_PRIORITY+1, NULL);
	#if LOG_LEVEL >= LOG_DEBUG
	printf("UART task creation: %s\n", result == pdPASS ? "SUCCESS" : "FAILED" );
	#endif
#endif

#ifdef HTTP_ENABLED
	result = xTaskCreate(mbuspi_http_task, "HTTP_Task", configMINIMAL_STACK_SIZE*8, NULL, tskIDLE_PRIORITY+1, NULL);
	#if LOG_LEVEL >= LOG_DEBUG
	printf("HTTP task creation: %s\n", result == pdPASS ? "SUCCESS" : "FAILED" );
	#endif
#endif

#ifdef UDP_ENABLED
	result = xTaskCreate(mbuspi_udp_task, "UDP_Task", configMINIMAL_STACK_SIZE*8, NULL, tskIDLE_PRIORITY+1, NULL);
	#if LOG_LEVEL >= LOG_DEBUG
	printf("UDP task creation: %s\n", result == pdPASS ? "SUCCESS" : "FAILED" );
	#endif
#endif

	vTaskStartScheduler();

#if LOG_LEVEL >= LOG_DEBUG
	printf("=== EXIT ===\n");
#endif
	return 0;
}