#pragma once

#include <mbus-rpi.conf.h>
#include <stdio.h>
#include <stdarg.h>
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>

#define MAX_QUEUE_ITEM_SIZE 128

namespace MBusPi {

	typedef struct {
		size_t len;
		char data[MAX_QUEUE_ITEM_SIZE];
	} xMBusData_t;
	
	enum class Value {
		
	};

	void Init();
	void GetValue(Value, void*);
	void SetValue(Value, void*);
	
	extern QueueHandle_t xDeviceEventQueue;

#ifdef WIFI_ENABLED
	namespace Wifi {
		int Init();
		void RunTask(void*);
	}
#endif

	namespace Device {
		void Init();
		void RunTask(void*);
	}

	namespace UART {
		void Init();
		void RunTask(void*);
	}

#ifdef HTTP_ENABLED
	namespace HTTP {
		void Init();
		void RunTask(void*);
	}
#endif

#ifdef UDP_ENABLED
	namespace UDP {
		void Init();
		void RunTask(void*);
	}
#endif
}

// Logging

#define LOG_NONE  0
#define LOG_ERROR 1
#define LOG_INFO  2
#define LOG_DEBUG 3

#ifndef LOG_LEVEL
# define LOG_LEVEL LOG_DEBUG
#endif

void LOG_E(const char* format, ...);
void LOG_I(const char* format, ...);
void LOG_D(const char* format, ...);
