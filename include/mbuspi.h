#pragma once

#include <mbuspi.conf.h>
#include <stdio.h>
#include <stdarg.h>
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

void mbuspi_init();
size_t mbuspi_get_values_json(char* data_buffer);
void mbuspi_reset();
	
extern QueueHandle_t xDeviceEventQueue;

#ifdef WIFI_ENABLED
// wifi interface
int mbuspi_wifi_get_state();
void mbuspi_wifi_task(void*);
#endif

// device interface
const char* mbuspi_device_name();
void mbuspi_device_task(void*);

// uart interface
void mbuspi_uart_task(void*);

#ifdef HTTP_ENABLED
// http webserver interface
void mbuspi_http_task(void*);
#endif

#ifdef UDP_ENABLED
// udp sender interface
void mbuspi_udp_task(void*);
#endif

// LOGGING

#define LOG_NONE  0
#define LOG_ERROR 1
#define LOG_INFO  2
#define LOG_DEBUG 3

#ifndef LOG_LEVEL
# define LOG_LEVEL LOG_DEBUG
#endif

#if LOG_LEVEL >= LOG_ERROR
void LOG_E(const char* tag, const char* format, ...);
#else
#define LOG_E(...)
#endif

#if LOG_LEVEL >= LOG_INFO
void LOG_I(const char* tag, const char* format, ...);
#else
#define LOG_I(...)
#endif

#if LOG_LEVEL >= LOG_DEBUG
void LOG_D(const char* tag, const char* format, ...);
#else
#define LOG_D(...)
#endif
