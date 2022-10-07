#ifndef THREADING_ALT_H
#define THREADING_ALT_H

#include "FreeRTOS.h"
#include "semphr.h"

typedef struct mbuspi_threading_mutex {
	SemaphoreHandle_t mutexHandle;
	StaticSemaphore_t mutexStorage;
} mbuspi_threading_mutex_t;

/* mutex functions. */
void mbuspi_platform_mutex_init( mbuspi_threading_mutex_t * pMutex );
void mbuspi_platform_mutex_free( mbuspi_threading_mutex_t * pMutex );
int mbuspi_platform_mutex_lock( mbuspi_threading_mutex_t * pMutex );
int mbuspi_platform_mutex_unlock( mbuspi_threading_mutex_t * pMutex );

#endif // THREADING_ALT_H
