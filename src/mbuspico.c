#include <mbuspico.h>
#include <semphr.h>
#include <hardware/watchdog.h>
#include <hardware/regs/rosc.h>
#include <hardware/regs/addressmap.h>
#include <mbedtls_config.h>

#define TAG ""

QueueHandle_t xDeviceEventQueue = NULL;

static SemaphoreHandle_t xValueMutex = NULL;
static SemaphoreHandle_t xLogMutex = NULL;

void mbuspico_init() {
	xValueMutex =  xSemaphoreCreateBinary();
	xSemaphoreGive(xValueMutex);
	
	xLogMutex =  xSemaphoreCreateBinary();
	xSemaphoreGive(xLogMutex);
	
	xDeviceEventQueue = xQueueCreate(5, sizeof(xMBusData_t));
}

size_t mbuspico_get_values_json(char* data_buffer) {
	size_t data_len = 0;
	MBUSPICO_LOG_D(TAG, "mbuspico_get_values_json() called");
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

void mbuspico_reset() {
	watchdog_enable(1, 1);
	while(1) {};
}

// ENTROPY RNG
#ifdef MBEDTLS_ENTROPY_HARDWARE_ALT
// based on https://forums.raspberrypi.com/viewtopic.php?t=302960
int mbedtls_hardware_poll (void *data, unsigned char *output, size_t len, size_t *olen) {
	int k, i, rnd = 0;
	volatile uint32_t *rnd_reg=(uint32_t *)(ROSC_BASE + ROSC_RANDOMBIT_OFFSET);
	for (k = 0; k < 32; k++) {
		rnd = rnd << 1;
		rnd = rnd + (0x00000001 & (*rnd_reg));
	}
	for (i = 0; i < len; i++) {
		output[i] = ((unsigned char*)&rnd)[i%sizeof(rnd)];
	}
	*olen = len;
	return 0;
}
#endif

// Logging

#if MBUSPICO_LOG_LEVEL >= MBUSPICO_LOG_ERROR
void MBUSPICO_LOG_E(const char* tag, const char* format, ...) {
	if (xSemaphoreTake(xLogMutex, portMAX_DELAY) == pdTRUE) {
		va_list args;
		va_start (args, format);
		printf("[ERROR]\t%s\t", tag);
		vprintf(format, args);
		printf("\n");
		va_end (args);
		xSemaphoreGive(xLogMutex);
	}
}
#endif

#if MBUSPICO_LOG_LEVEL >= MBUSPICO_LOG_INFO
void MBUSPICO_LOG_I(const char* tag, const char* format, ...) {
	if (xSemaphoreTake(xLogMutex, portMAX_DELAY) == pdTRUE) {
		va_list args;
		va_start (args, format);
		printf("[INFO]\t%s\t", tag);
		vprintf(format, args);
		printf("\n");
		va_end (args);
		xSemaphoreGive(xLogMutex);
	}
}
#endif

#if MBUSPICO_LOG_LEVEL >= MBUSPICO_LOG_DEBUG
void MBUSPICO_LOG_D(const char* tag, const char* format, ...) {
	if (xSemaphoreTake(xLogMutex, portMAX_DELAY) == pdTRUE) {
		va_list args;
		va_start (args, format);
		printf("[DEBUG]\t%s\t", tag);
		vprintf(format, args);
		printf("\n");
		va_end (args);
		xSemaphoreGive(xLogMutex);
	}
}
#endif
