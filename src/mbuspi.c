#include <mbuspi.h>
#include "semphr.h"

#define TAG ""

QueueHandle_t xDeviceEventQueue = NULL;

static SemaphoreHandle_t xValueMutex = NULL;
static SemaphoreHandle_t xLogMutex = NULL;

void mbuspi_init() {
	xValueMutex =  xSemaphoreCreateBinary();
	xSemaphoreGive(xValueMutex);
	
	xLogMutex =  xSemaphoreCreateBinary();
	xSemaphoreGive(xLogMutex);
	
	xDeviceEventQueue = xQueueCreate(5, sizeof(xMBusData_t));
}

size_t mbuspi_get_values_json(char* data_buffer) {
	size_t data_len = 0;
	LOG_D(TAG, "mbuspi_get_values_json() called");
	if (xSemaphoreTake(xValueMutex, portMAX_DELAY) == pdTRUE) {
		static const char* data_tmpl = 
			"{" "\n"
			"\"value1\": %d," "\n"
			"\"value2\": %d," "\n"
			"\"value3\": %d," "\n"
			"\"value4\": %d," "\n"
			"\"value5\": %d" "\n"
			"}"
		;
		data_len = snprintf(data_buffer, DATA_BUFFER_SIZE, data_tmpl, 1, 2, 3, 4, 5);
		xSemaphoreGive(xValueMutex);
	}
	return data_len;
}

// Logging

#if LOG_LEVEL >= LOG_ERROR
void LOG_E(const char* tag, const char* format, ...) {
	if (xSemaphoreTake(xLogMutex, portMAX_DELAY) == pdTRUE) {
		va_list args;
		va_start (args, format);
		printf("[ERROR]\t");
		printf("%s\t", tag);
		vprintf(format, args);
		printf("\n");
		va_end (args);
		xSemaphoreGive(xLogMutex);
	}
}
#endif

#if LOG_LEVEL >= LOG_INFO
void LOG_I(const char* tag, const char* format, ...) {
	if (xSemaphoreTake(xLogMutex, portMAX_DELAY) == pdTRUE) {
		va_list args;
		va_start (args, format);
		printf("[INFO]\t");
		printf("%s\t", tag);
		vprintf(format, args);
		printf("\n");
		va_end (args);
		xSemaphoreGive(xLogMutex);
	}
}
#endif

#if LOG_LEVEL >= LOG_DEBUG
void LOG_D(const char* tag, const char* format, ...) {
	if (xSemaphoreTake(xLogMutex, portMAX_DELAY) == pdTRUE) {
		va_list args;
		va_start (args, format);
		printf("[DEBUG]\t");
		printf("%s\t", tag);
		vprintf(format, args);
		printf("\n");
		va_end (args);
		xSemaphoreGive(xLogMutex);
	}
}
#endif
