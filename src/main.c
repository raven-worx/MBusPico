#include <mbuspico.h>
#include <stdio.h>
#include <string.h>
#include "hardware/watchdog.h"

// task handles
TaskHandle_t hWDTask = NULL;
TaskHandle_t hWifiTask = NULL;
TaskHandle_t hDeviceTask = NULL;
TaskHandle_t hUartTask = NULL;
TaskHandle_t hHttpTask = NULL;
TaskHandle_t hUdpTask = NULL;

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

void printTaskInfo(TaskHandle_t hTask) {
#if configUSE_TRACE_FACILITY && MBUSPICO_LOG_LEVEL >= LOG_DEBUG
	if (hTask) {
		TaskStatus_t status;
		vTaskGetInfo(hTask, &status, pdTRUE, eInvalid );
		char stateStr[12] = {0};
		switch (status.eCurrentState) {
			case eRunning:	strcpy(stateStr,"eRunning"); break;
    		case eReady:	strcpy(stateStr,"eReady"); break;
			case eBlocked:	strcpy(stateStr,"eBlocked"); break;
			case eSuspended:strcpy(stateStr,"eSuspended"); break;
			case eDeleted:	strcpy(stateStr,"eDeleted"); break;
			case eInvalid:	strcpy(stateStr,"eInvalid"); break;
		}
		printf("###  Task info [%s]:\tstate: %s \tstack-free: %lu\n", status.pcTaskName, stateStr, status.usStackHighWaterMark);
	}
#endif
}

void mbuspico_wd_task(void* arg) {
	#ifndef _DEBUG
	// enable HW watchdog
	watchdog_enable(10000, 1);
	#endif

	for (;;) {
	#if 0
		printTaskInfo(hWifiTask);
		printTaskInfo(hDeviceTask);
		printTaskInfo(hUartTask);
		printTaskInfo(hHttpTask);
		printTaskInfo(hUdpTask);
	#endif

	#ifndef _DEBUG
		watchdog_update();
	#endif

		vTaskDelay(1000/portTICK_PERIOD_MS);
	}
}


int main() {
	stdio_init_all();
	
	if (watchdog_caused_reboot()) {
	#if MBUSPICO_LOG_LEVEL >= LOG_ERROR
        printf("Rebooted by Watchdog!\n");
	#endif
	}

	BaseType_t result;
	
	mbuspico_init();

	result = xTaskCreate(mbuspico_wd_task, "WD_Task", configMINIMAL_STACK_SIZE*4, NULL, tskIDLE_PRIORITY+1000, &hWDTask);
	if (result != pdPASS) {
	#if MBUSPICO_LOG_LEVEL >= LOG_ERROR
		printf("Failed to create DBG task!\n");
	#endif
	}
	
#if MBUSPICO_WIFI_ENABLED
	result = xTaskCreate(mbuspico_wifi_task, "WIFI_Task", configMINIMAL_STACK_SIZE*6, NULL, tskIDLE_PRIORITY+1, &hWifiTask);
	if (result != pdPASS) {
	#if MBUSPICO_LOG_LEVEL >= LOG_ERROR
		printf("Failed to create WIFI task!\n");
	#endif
	}
	else {
		// we must bind the wifi task to one core (well at least while the init is called)
		// (note we only do this in NO_SYS mode, because cyw43_arch_freertos takes care of it otherwise)
		vTaskCoreAffinitySet(hWifiTask, 1);
	}
#endif

	result = xTaskCreate(mbuspico_device_task, "Device_Task", configMINIMAL_STACK_SIZE*10, NULL, tskIDLE_PRIORITY+1, &hDeviceTask);
	if (result != pdPASS) {
	#if MBUSPICO_LOG_LEVEL >= LOG_ERROR
		printf("Failed to create DEVICE task!\n");
	#endif
	}

	result = xTaskCreate(mbuspico_uart_task, "UART_Task", configMINIMAL_STACK_SIZE*6, NULL, tskIDLE_PRIORITY+1, &hUartTask);
	if (result != pdPASS) {
	#if MBUSPICO_LOG_LEVEL >= LOG_ERROR
		printf("Failed to create UART task!\n");
	#endif
	}
	else {
		vTaskCoreAffinitySet(hUartTask, 2);
	}

#if MBUSPICO_HTTP_ENABLED
	result = xTaskCreate(mbuspico_http_task, "HTTP_Task", configMINIMAL_STACK_SIZE*8, NULL, tskIDLE_PRIORITY+1, &hHttpTask);
	if (result != pdPASS) {
	#if MBUSPICO_LOG_LEVEL >= LOG_ERROR
		printf("Failed to create HTTP task!\n");
	#endif
	}
#endif

#if MBUSPICO_UDP_ENABLED
	result = xTaskCreate(mbuspico_udp_task, "UDP_Task", configMINIMAL_STACK_SIZE*8, NULL, tskIDLE_PRIORITY+1, &hUdpTask);
	if (result != pdPASS) {
	#if MBUSPICO_LOG_LEVEL >= LOG_ERROR
		printf("Failed to create UDP task!\n");
	#endif
	}
#endif
#endif

	vTaskStartScheduler();

	// unreachable
#if MBUSPICO_LOG_LEVEL >= LOG_DEBUG
	printf("=== EXIT ===\n");
#endif
	return 0;
}