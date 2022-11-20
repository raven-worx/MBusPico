#include <mbuspico.h>
#include <stdio.h>
#include <semphr.h>
#include <string.h>
#include <hardware/watchdog.h>
#include <hardware/regs/rosc.h>
#include <hardware/regs/addressmap.h>
#include <mbedtls_config.h>

QueueHandle_t g_DeviceEventQueue = NULL;

static SemaphoreHandle_t g_ValueMutex = NULL;
static SemaphoreHandle_t g_LogMutex = NULL;
static MeterData_t g_MeterData;

void mbuspico_print_meterdata(void) {
	if (xSemaphoreTake(g_ValueMutex, portMAX_DELAY) == pdTRUE) {
		MBUSPICO_LOG_D(LOG_TAG_MAIN,
				"\n"
				"Timestamp: %s" "\n"
				"Meter #: %s" "\n"
				"activePowerPlus: %.2f [W]" "\n"
				"activePowerMinus: %.2f [W]" "\n"
				"activeEnergyPlus: %.2f [Wh]" "\n"
				"activeEnergyMinus: %.2f [Wh]" "\n"
				//"reactiveEnergyPlus: %.2f" "\n"
				//"reactiveEnergyMinus: %.2f" "\n"
				"voltageL1: %.2f [V]" "\n"
				"voltageL2: %.2f [V]" "\n"
				"voltageL3: %.2f [V]" "\n"
				"currentL1: %.2f [A]" "\n"
				"currentL2: %.2f [A]" "\n"
				"currentL3: %.2f [A]" "\n"
				"powerFactor: %.2f" "\n"
				"================" "\n",
				g_MeterData.timestamp,
				g_MeterData.meterNumber,
				g_MeterData.activePowerPlus,
				g_MeterData.activePowerMinus,
				g_MeterData.activeEnergyPlus,
				g_MeterData.activeEnergyMinus,
				//g_MeterData.reactiveEnergyPlus,
				//g_MeterData.reactiveEnergyMinus,
				g_MeterData.voltageL1,
				g_MeterData.voltageL2,
				g_MeterData.voltageL3,
				g_MeterData.currentL1,
				g_MeterData.currentL2,
				g_MeterData.currentL3,
				g_MeterData.powerFactor
			);
		xSemaphoreGive(g_ValueMutex);
	}
}

size_t mbuspico_get_meterdata_json(char* data_buffer, size_t buffer_size) {
	size_t data_len = 0;
	MBUSPICO_LOG_D(LOG_TAG_MAIN, "mbuspico_get_meterdata_json() called");
	if (xSemaphoreTake(g_ValueMutex, portMAX_DELAY) == pdTRUE) {
		data_len = snprintf(
				data_buffer, buffer_size,
				"{" "\n"
				"\"timestamp\": \"%s\"," "\n"
				"\"meterNumber\": \"%s\"," "\n"
				"\"activePowerPlus\": %.2f," "\n"
				"\"activePowerMinus\": %.2f," "\n"
				"\"activeEnergyPlus\": %.2f," "\n"
				"\"activeEnergyMinus\": %.2f," "\n"
				"\"voltageL1\": %.2f," "\n"
				"\"voltageL2\": %.2f," "\n"
				"\"voltageL3\": %.2f," "\n"
				"\"currentL1\": %.2f," "\n"
				"\"currentL2\": %.2f," "\n"
				"\"currentL3\": %.2f," "\n"
				"\"powerFactor\": %.2f" "\n"
				"}",
				g_MeterData.timestamp,
				g_MeterData.meterNumber,
				g_MeterData.activePowerPlus,
				g_MeterData.activePowerMinus,
				g_MeterData.activeEnergyPlus,
				g_MeterData.activeEnergyMinus,
				g_MeterData.voltageL1,
				g_MeterData.voltageL2,
				g_MeterData.voltageL3,
				g_MeterData.currentL1,
				g_MeterData.currentL2,
				g_MeterData.currentL3,
				g_MeterData.powerFactor
			);
		xSemaphoreGive(g_ValueMutex);
	}
	else {
		MBUSPICO_LOG_E(LOG_TAG_MAIN, "failed to aquire mutex in mbuspico_get_meterdata_json()");
	}
	return data_len;
}

void mbuspico_set_meterdata(MeterData_t* data) {
	MBUSPICO_LOG_D(LOG_TAG_MAIN, "mbuspico_set_meterdata() called");
	if (xSemaphoreTake(g_ValueMutex, portMAX_DELAY) == pdTRUE) {
		memcpy(&g_MeterData, data, sizeof(MeterData_t));
		xSemaphoreGive(g_ValueMutex);
#if MBUSPICO_LOG_LEVEL >= MBUSPICO_LOG_DEBUG
		mbuspico_print_meterdata();
#endif
	}
	else {
		MBUSPICO_LOG_E(LOG_TAG_MAIN, "failed to aquire mutex in mbuspico_set_meterdata()");
	}
}

void mbuspico_init(void) {
	g_ValueMutex =  xSemaphoreCreateBinary();
	xSemaphoreGive(g_ValueMutex);
	
	g_LogMutex =  xSemaphoreCreateBinary();
	xSemaphoreGive(g_LogMutex);

	g_DeviceEventQueue = xQueueCreate(15, sizeof(xMBusData_t));

	g_HttpConnectionSemaphore = xSemaphoreCreateCounting(HTTP_MAX_CONNECTION_COUNT, HTTP_MAX_CONNECTION_COUNT);
}

void mbuspico_reset(void) {
	watchdog_enable(1, 1);
	while(1) {};
}

uint64_t mbuspico_time_ms(void) {
	uint64_t us = time_us_64();	
	return us/1000;
}

// taken from https://stackoverflow.com/a/53579348
void mbuspico_hex_to_bin(const char* in, size_t len, byte* out) {
	static const unsigned char TBL[] = {
    	 0,   1,   2,   3,   4,   5,   6,   7,   8,   9,  58,  59,  60,  61,
    	62,  63,  64,  10,  11,  12,  13,  14,  15,  71,  72,  73,  74,  75,
    	76,  77,  78,  79,  80,  81,  82,  83,  84,  85,  86,  87,  88,  89,
    	90,  91,  92,  93,  94,  95,  96,  10,  11,  12,  13,  14,  15
  	};

  	static const unsigned char *LOOKUP = TBL - 48;

  	const char* end = in + len;
  	while(in < end) *(out++) = LOOKUP[*(in++)] << 4 | LOOKUP[*(in++)];
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

static void get_log_tag(uint16_t id, char* tag) {
	switch(id) {
		case LOG_TAG_MAIN:		sprintf(tag, "%s", "MAIN"); break;
		case LOG_TAG_WIFI:		sprintf(tag, "%s", "WIFI"); break;
		case LOG_TAG_DEVICE:	sprintf(tag, "%s", "DEVICE"); break;
		case LOG_TAG_UART:		sprintf(tag, "%s", "UART"); break;
		case LOG_TAG_HTTP:		sprintf(tag, "%s", "HTTP"); break;
		case LOG_TAG_UDP:		sprintf(tag, "%s", "MAIN"); break;
		default:				sprintf(tag, "%s", "???"); break;
	}
}

#if MBUSPICO_LOG_LEVEL >= LOG_ERROR
void MBUSPICO_LOG_E(uint16_t id, const char* format, ...) {
	if (LOG_FILTER & id && xSemaphoreTake(g_LogMutex, portMAX_DELAY) == pdTRUE) {
		char tag[10];
		get_log_tag(id,tag);
		va_list args;
		va_start (args, format);
		printf("[ERROR]\t%s\t", tag);
		vprintf(format, args);
		printf("\n");
		va_end (args);
		xSemaphoreGive(g_LogMutex);
	}
}
#endif

#if MBUSPICO_LOG_LEVEL >= LOG_INFO
void MBUSPICO_LOG_I(uint16_t id, const char* format, ...) {
	if (LOG_FILTER & id && xSemaphoreTake(g_LogMutex, portMAX_DELAY) == pdTRUE) {
		char tag[10];
		get_log_tag(id,tag);
		va_list args;
		va_start (args, format);
		printf("[INFO]\t%s\t", tag);
		vprintf(format, args);
		printf("\n");
		va_end (args);
		xSemaphoreGive(g_LogMutex);
	}
}
#endif

#if MBUSPICO_LOG_LEVEL >= LOG_DEBUG
void MBUSPICO_LOG_D(uint16_t id, const char* format, ...) {
	if (LOG_FILTER & id && xSemaphoreTake(g_LogMutex, portMAX_DELAY) == pdTRUE) {
		char tag[10];
		get_log_tag(id,tag);
		va_list args;
		va_start (args, format);
		printf("[DEBUG]\t%s\t", tag);
		vprintf(format, args);
		printf("\n");
		va_end (args);
		xSemaphoreGive(g_LogMutex);
	}
}
#endif
