#include <mbus-rpi.h>
#include "semphr.h"

QueueHandle_t MBusPi::xDeviceEventQueue = nullptr;

static SemaphoreHandle_t xValueMutex = nullptr;

void MBusPi::Init() {
	LOG_D("MBusPi::Init() called")
	xValueMutex =  xSemaphoreCreateBinary();
	xSemaphoreGive(xValueMutex);
	
	xDeviceEventQueue = xQueueCreate(5, sizeof(xMBusData_t));
	LOG_D("MBus data queue created")
}

void MBusPi::GetValue(MBusPi::Value, void*) {
	LOG_D("MBusPi::GetValue() called")
	if (xSemaphoreTake(xValueMutex, portMAX_DELAY) == pdTRUE) {
		// TODO
		xSemaphoreGive(xValueMutex);
	}
}

void MBusPi::SetValue(MBusPi::Value, void*) {
	LOG_D("MBusPi::SetValue() called")
	if (xSemaphoreTake(xValueMutex, portMAX_DELAY) == pdTRUE) {
		// TODO
		xSemaphoreGive(xValueMutex);
	}
}
