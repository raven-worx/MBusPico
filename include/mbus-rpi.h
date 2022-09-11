#pragma once

#include <mbus-rpi.conf.h>
#include <stdio.h>
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

	namespace Wifi {
		int Init();
		void RunTask(void*);
	}

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

#if LOG_LEVEL >= LOG_ERROR
# define LOG_E(MSG, ...) ({printf("[ERROR]\t" MSG "\n", ##__VA_ARGS__); true;});
#else
# define LOG_E(MSG, ...)
#endif

#if LOG_LEVEL >= LOG_INFO
# define LOG_I(MSG, ...) ({printf("[INFO]\t" MSG "\n", ##__VA_ARGS__); true;});
#else
# define LOG_I(MSG, ...)
#endif

#if LOG_LEVEL >= LOG_DEBUG
# define LOG_D(MSG, ...) ({printf("[DEBUG]\t" MSG "\n", ##__VA_ARGS__); true;});
#else
# define LOG_D(MSG, ...)
#endif
