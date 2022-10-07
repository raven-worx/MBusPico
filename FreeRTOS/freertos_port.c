/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "FreeRTOS_Sockets.h"

void * mbuspi_platform_calloc( size_t nmemb, size_t size )
{
	size_t totalSize = nmemb * size;
	void * pBuffer = NULL;
	/* Check that neither nmemb nor size were 0. */
	if( totalSize > 0 ) {
		/* Overflow check. */
		if( ( totalSize / size ) == nmemb ) {
			pBuffer = pvPortMalloc( totalSize );
			if( pBuffer != NULL ) {
				( void ) memset( pBuffer, 0x00, totalSize );
			}
		}
	}
	return pBuffer;
}

void mbuspi_platform_free( void * ptr )
{
	vPortFree( ptr );
}

/*-----------------------------------------------------------*/

void mbuspi_platform_mutex_init( mbuspi_threading_mutex_t * pMutex )
{
	configASSERT( pMutex != NULL );
	/* Create a statically-allocated FreeRTOS mutex. This should never fail as storage is provided. */
	pMutex->mutexHandle = xSemaphoreCreateMutexStatic( &( pMutex->mutexStorage ) );
	configASSERT( pMutex->mutexHandle != NULL );
}

void mbuspi_platform_mutex_free( mbuspi_threading_mutex_t * pMutex )
{
	/* Nothing needs to be done to free a statically-allocated FreeRTOS mutex. */
	( void ) pMutex;
}

int mbuspi_platform_mutex_lock( mbuspi_threading_mutex_t * pMutex )
{
	BaseType_t mutexStatus = 0;
	configASSERT( pMutex != NULL );
	/* mutexStatus is not used if asserts are disabled. */
	( void ) mutexStatus;
	/* This function should never fail if the mutex is initialized. */
	mutexStatus = xSemaphoreTake( pMutex->mutexHandle, portMAX_DELAY );
	configASSERT( mutexStatus == pdTRUE );
	return 0;
}

int mbuspi_platform_mutex_unlock( mbuspi_threading_mutex_t * pMutex )
{
	BaseType_t mutexStatus = 0;
	configASSERT( pMutex != NULL );
	/* mutexStatus is not used if asserts are disabled. */
	( void ) mutexStatus;
	/* This function should never fail if the mutex is initialized. */
	mutexStatus = xSemaphoreGive( pMutex->mutexHandle );
	configASSERT( mutexStatus == pdTRUE );
	return 0;
}
