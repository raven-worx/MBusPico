#pragma once

#include <mbuspico.conf.h>
#include <stdio.h>
#include <stdarg.h>
#include <pico/stdlib.h>
#include <hardware/regs/rosc.h>
#include <hardware/regs/addressmap.h>
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>

#define DATA_BUFFER_SIZE 	512
#define MAX_QUEUE_ITEM_SIZE 128

typedef struct {
	size_t len;
	char data[MAX_QUEUE_ITEM_SIZE];
} xMBusData_t;

void mbuspico_init();
size_t mbuspico_get_values_json(char* data_buffer);
void mbuspico_reset();
	
extern QueueHandle_t xDeviceEventQueue;

#if MBUSPICO_WIFI_ENABLED
// wifi interface
int mbuspico_wifi_get_state();
void mbuspico_wifi_task(void*);
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

// LOGGING

#define LOG_NONE  0
#define LOG_ERROR 1
#define LOG_INFO  2
#define LOG_DEBUG 3

#ifndef MBUSPICO_LOG_LEVEL
# define MBUSPICO_LOG_LEVEL LOG_ERROR
#endif

#if MBUSPICO_LOG_LEVEL >= LOG_ERROR
void MBUSPICO_LOG_E(const char* tag, const char* format, ...);
#else
#define MBUSPICO_LOG_E(...)
#endif

#if MBUSPICO_LOG_LEVEL >= LOG_INFO
void MBUSPICO_LOG_I(const char* tag, const char* format, ...);
#else
#define MBUSPICO_LOG_I(...)
#endif

#if MBUSPICO_LOG_LEVEL >= LOG_DEBUG
void MBUSPICO_LOG_D(const char* tag, const char* format, ...);
#else
#define MBUSPICO_LOG_D(...)
#endif
