#include <mbuspi.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/irq.h"

#define TAG "UART"

#define UART_ID 	uart0
#define BAUD_RATE 	2400
#define DATA_BITS 	8
#define STOP_BITS 	1
#define PARITY		UART_PARITY_EVEN

// We are using pins 0 and 1, but see the GPIO function select table in the
// datasheet for information on which other pins can be used.
#define UART_TX_PIN 0
#define UART_RX_PIN 1

// RX interrupt handler
static void on_uart_rx() {
	//LOG_D(TAG, "data ready to read");
	xMBusData_t d = {0};
	while (uart_is_readable(UART_ID)) {
		uint8_t ch = uart_getc(UART_ID);
		d.data[d.len] = ch;
		d.len++;
		if (d.len == MAX_QUEUE_ITEM_SIZE) {
			xQueueSend(xDeviceEventQueue, &d, 0);
			d.len = 0;
		}
	}
	if (d.len > 0) {
		xQueueSend(xDeviceEventQueue, &d, 0); 
	}
}

static void mbuspi_uart_init() {
	LOG_D(TAG, "mbuspi_uart_init()");
	
	// Set up UART with a basic baud rate
	uart_init(UART_ID, 2400);
	
	// Set the TX and RX pins by using the function select on the GPIO
	gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART); // actually not used
	gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
	
	// The call will return the actual baud rate selected, which will be as close as possible to the requested one
	int __unused actual = uart_set_baudrate(UART_ID, BAUD_RATE);
	
	// Set UART flow control CTS/RTS, we don't want these, so turn them off
	uart_set_hw_flow(UART_ID, false, false);
	
	// Set our data format
	uart_set_format(UART_ID, DATA_BITS, STOP_BITS, PARITY);
	
	// Turn off FIFO's - we want to do this character by character
	uart_set_fifo_enabled(UART_ID, false);
	
	// Set up a RX interrupt
	// We need to set up the handler first
	// Select correct interrupt for the UART we are using
	int UART_IRQ = UART_ID == uart0 ? UART0_IRQ : UART1_IRQ;
	
	// And set up and enable the interrupt handlers
	irq_set_exclusive_handler(UART_IRQ, on_uart_rx);
	irq_set_enabled(UART_IRQ, true);
	
	// Now enable the UART to send interrupts - RX only
	uart_set_irq_enables(UART_ID, true, false);
	
	LOG_D(TAG, "UART initialized");
}

void mbuspi_uart_task(void* arg) {
	LOG_D(TAG, "mbuspi_uart_task()");
	
	mbuspi_uart_init();
	
	for (;;) {
		vTaskDelay(5000/portTICK_PERIOD_MS);
	}
}