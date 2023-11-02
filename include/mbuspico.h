#pragma once

#include <mbuspico.conf.h>

#include <stdarg.h>
#include <pico/stdlib.h>
#include <hardware/regs/rosc.h>
#include <hardware/regs/addressmap.h>

#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>

// ####################################
// INTERFACES
// ####################################

typedef struct {
	float activePowerPlus;		// [W]
	float activePowerMinus;		// [W]
	float activeEnergyPlus;		// [Wh]
	float activeEnergyMinus;	// [Wh]
	float reactiveEnergyPlus;
	float reactiveEnergyMinus;
	float voltageL1;			// [V]
	float voltageL2;			// [V]
	float voltageL3;			// [V]
	float currentL1;			// [A]
	float currentL2;			// [A]
	float currentL3;			// [A]
	float powerFactor;
	char timestamp[21];			// 0000-00-00T00:00:00Z
	uint64_t lxTimestamp;		// Loxone timestamp (seconds since 1.1.2009)
	char meterNumber[13];		// 123456789012
} MeterData_t;

void mbuspico_print_meterdata();
size_t mbuspico_get_meterdata_json(char* data_buffer, size_t buffer_size);
void mbuspico_set_meterdata(MeterData_t* data);

// global interface
void mbuspico_init();
int mbuspico_schedule_reboot(uint32_t ms);
int mbuspico_schedule_reboot_usb(uint32_t ms);
uint64_t mbuspico_time_ms();
void mbuspico_hex_to_bin(const char* in, size_t len, unsigned char* out);
void mbuspico_reboot();
void mbuspico_reboot_into_bootloader();

// UART data queue
typedef unsigned char byte;

#define MAX_QUEUE_ITEM_SIZE 128

typedef struct {
	size_t len;
	byte data[MAX_QUEUE_ITEM_SIZE];
} xMBusData_t;

extern QueueHandle_t g_DeviceEventQueue;

#if MBUSPICO_WIFI_ENABLED

#include "lwip/ip_addr.h"

// wifi interface
int mbuspico_wifi_get_state();
int mbuspico_wifi_get_mode();
void mbuspico_wifi_task(void*);

#define MBUSPICO_WIFI_MODE_NOMODE	-1
#define MBUSPICO_WIFI_MODE_STA 		0
#define MBUSPICO_WIFI_MODE_AP		1

typedef struct {
	int link_state;	// wifi link state
	int mode;		// current wifi mode
    ip_addr_t gw;	// DHCP & DNS gateway ip
	ip4_addr_t mask; // DHCP net mask
} mbuspico_wifi_state_t;
#endif

// device interface
const char* mbuspico_device_name();
void mbuspico_device_task(void*);

// uart interface
void mbuspico_uart_task(void*);

#if MBUSPICO_HTTP_ENABLED
// http webserver interface
void mbuspico_http_task(void*);
#endif

#if MBUSPICO_UDP_ENABLED
// udp sender interface
void mbuspico_udp_task(void*);
#endif

// ####################################
// CONFIG
// ####################################

#define MBUSPICO_CONFIG_SIZE_STR	128
#define MBUSPICO_CONFIG_SIZE_STR_XL	256

typedef enum {
	MBUSPICO_CONFIG_ENCRYPTION_KEY,
	MBUSPICO_CONFIG_WIFI_SSID,
	MBUSPICO_CONFIG_WIFI_PWD,
	MBUSPICO_CONFIG_WIFI_HOSTNAME,
	MBUSPICO_CONFIG_HTTP_PORT,
	MBUSPICO_CONFIG_HTTP_AUTH_USER,
	MBUSPICO_CONFIG_HTTP_AUTH_PWD,
	MBUSPICO_CONFIG_UDP_ENABLED,
	MBUSPICO_CONFIG_UDP_RECEIVER,
	MBUSPICO_CONFIG_UDP_PORT,
	MBUSPICO_CONFIG_UDP_INTERVAL
} mbuspico_config_t;

int mbuspico_read_config(mbuspico_config_t config, byte* data, uint32_t data_size);
int mbuspico_write_config(mbuspico_config_t config, byte* data, uint32_t data_size);

// ####################################
// LOGGING
// ####################################

// log tags/ids
#define LOG_TAG_MAIN	(1<<0) 
#define LOG_TAG_WIFI	(1<<1)
#define LOG_TAG_DEVICE	(1<<2)
#define LOG_TAG_UART	(1<<3)
#define LOG_TAG_HTTP	(1<<4)
#define LOG_TAG_UDP		(1<<5)
#define LOG_TAG_MG		(1<<6)
#define LOG_TAG_FS		(1<<7)

#define LOG_TAG_ALL		(0xFFFF)

// log filter
#define LOG_FILTER		LOG_TAG_ALL

// log levels
#define LOG_NONE  0
#define LOG_ERROR 1
#define LOG_INFO  2
#define LOG_DEBUG 3

#ifndef MBUSPICO_LOG_LEVEL
# define MBUSPICO_LOG_LEVEL LOG_ERROR
#endif

#if MBUSPICO_LOG_LEVEL >= LOG_ERROR
void MBUSPICO_LOG_E(uint16_t id, const char* format, ...);
#else
#define MBUSPICO_LOG_E(...)
#endif

#if MBUSPICO_LOG_LEVEL >= LOG_INFO
void MBUSPICO_LOG_I(uint16_t id, const char* format, ...);
#else
#define MBUSPICO_LOG_I(...)
#endif

#if MBUSPICO_LOG_LEVEL >= LOG_DEBUG
void MBUSPICO_LOG_D(uint16_t id, const char* format, ...);
#else
#define MBUSPICO_LOG_D(...)
#endif
