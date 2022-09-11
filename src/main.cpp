#include <conf.h>
#include <stdio.h>
#include <pico/stdlib.h>
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>

QueueHandle_t eventQueue = nullptr;

int main() {
	stdio_init_all();
	
	eventQueue = xQueueCreate(5, sizeof(uint8_t) );
	
	MBusDevice::Init();
	
	xTaskCreate(MBusDevice::DoWork, "MBus_Task", 256, NULL, 1, NULL);
	//xTaskCreate(http_task, "HTTP_Task", 256, NULL, 1, NULL);
	//xTaskCreate(udp_task, "UDP_Task", 256, NULL, 1, NULL);
	
	vTaskStartScheduler();
	
	return 0;
}