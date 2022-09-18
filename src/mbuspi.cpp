#include <mbus-rpi.h>
#include "semphr.h"

QueueHandle_t MBusPi::xDeviceEventQueue = nullptr;

static SemaphoreHandle_t xValueMutex = nullptr;
static SemaphoreHandle_t xLogMutex = nullptr;

void MBusPi::Init() {
	//LOG_D("MBusPi::Init() called");
	xValueMutex =  xSemaphoreCreateBinary();
	xSemaphoreGive(xValueMutex);
	
	xLogMutex =  xSemaphoreCreateBinary();
	xSemaphoreGive(xLogMutex);
	
	xDeviceEventQueue = xQueueCreate(5, sizeof(xMBusData_t));
	//LOG_D("MBus data queue created");
}

void MBusPi::GetValue(MBusPi::Value, void*) {
	LOG_D("MBusPi::GetValue() called");
	if (xSemaphoreTake(xValueMutex, portMAX_DELAY) == pdTRUE) {
		// TODO
		xSemaphoreGive(xValueMutex);
	}
}

void MBusPi::SetValue(MBusPi::Value, void*) {
	LOG_D("MBusPi::SetValue() called");
	if (xSemaphoreTake(xValueMutex, portMAX_DELAY) == pdTRUE) {
		// TODO
		xSemaphoreGive(xValueMutex);
	}
}

// Logging

void LOG_E(const char* format, ...) {
#if LOG_LEVEL >= LOG_ERROR
	if (xSemaphoreTake(xLogMutex, portMAX_DELAY) == pdTRUE) {
		va_list args;
		va_start (args, format);
		printf("[ERROR]\t");
		vprintf(format, args);
		printf("\n");
		va_end (args);
		xSemaphoreGive(xLogMutex);
	}
#endif
}

void LOG_I(const char* format, ...) {
#if LOG_LEVEL >= LOG_INFO
	if (xSemaphoreTake(xLogMutex, portMAX_DELAY) == pdTRUE) {
		va_list args;
		va_start (args, format);
		printf("[INFO]\t");
		vprintf(format, args);
		printf("\n");
		va_end (args);
		xSemaphoreGive(xLogMutex);
	}
#endif
}

void LOG_D(const char* format, ...) {
#if LOG_LEVEL >= LOG_DEBUG
	if (xSemaphoreTake(xLogMutex, portMAX_DELAY) == pdTRUE) {
		va_list args;
		va_start (args, format);
		printf("[DEBUG]\t");
		vprintf(format, args);
		printf("\n");
		va_end (args);
		xSemaphoreGive(xLogMutex);
	}
#endif
}

