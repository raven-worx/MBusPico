#include <mbuspico.h>

// FreeRTOS debug hooks
#if configCHECK_FOR_STACK_OVERFLOW
void vApplicationStackOverflowHook(TaskHandle_t xTask, char* pcTaskName) {
	printf("OVERFLOW of task '%s'\n", pcTaskName);
}
#endif

#if configUSE_MALLOC_FAILED_HOOK
void vApplicationMallocFailedHook(void) {
	printf("MALLOC failed\n");
}
#endif

int main() {
	stdio_init_all();
	
	BaseType_t result;
	
	mbuspico_init();
	
#if MBUSPICO_WIFI_ENABLED
	TaskHandle_t handleWifiTask;
	result = xTaskCreate(mbuspico_wifi_task, "WIFI_Task", configMINIMAL_STACK_SIZE*8, NULL, tskIDLE_PRIORITY+1, &handleWifiTask);
	#if MBUSPICO_LOG_LEVEL >= LOG_DEBUG
	printf("WIFI task creation: %s\n", result == pdPASS ? "SUCCESS" : "FAILED" );
	#endif
	// we must bind the wifi task to one core (well at least while the init is called)
	// (note we only do this in NO_SYS mode, because cyw43_arch_freertos takes care of it otherwise)
	vTaskCoreAffinitySet(handleWifiTask, 1);
#endif
	
	result = xTaskCreate(mbuspico_device_task, "Device_Task", configMINIMAL_STACK_SIZE*8, NULL, tskIDLE_PRIORITY+1, NULL);
	#if MBUSPICO_LOG_LEVEL >= LOG_DEBUG
	printf("Device task creation: %s\n", result == pdPASS ? "SUCCESS" : "FAILED" );
	#endif

#if 0
	result = xTaskCreate(mbuspico_uart_task, "UART_Task", configMINIMAL_STACK_SIZE*8, NULL, tskIDLE_PRIORITY+1, NULL);
	#if MBUSPICO_LOG_LEVEL >= LOG_DEBUG
	printf("UART task creation: %s\n", result == pdPASS ? "SUCCESS" : "FAILED" );
	#endif
#endif

#if MBUSPICO_HTTP_ENABLED
	result = xTaskCreate(mbuspico_http_task, "HTTP_Task", configMINIMAL_STACK_SIZE*8, NULL, tskIDLE_PRIORITY+1, NULL);
	#if MBUSPICO_LOG_LEVEL >= LOG_DEBUG
	printf("HTTP task creation: %s\n", result == pdPASS ? "SUCCESS" : "FAILED" );
	#endif
#endif

#if MBUSPICO_UDP_ENABLED
	result = xTaskCreate(mbuspico_udp_task, "UDP_Task", configMINIMAL_STACK_SIZE*8, NULL, tskIDLE_PRIORITY+1, NULL);
	#if MBUSPICO_LOG_LEVEL >= LOG_DEBUG
	printf("UDP task creation: %s\n", result == pdPASS ? "SUCCESS" : "FAILED" );
	#endif
#endif

	vTaskStartScheduler();

#if MBUSPICO_LOG_LEVEL >= LOG_DEBUG
	printf("=== EXIT ===\n");
#endif
	return 0;
}