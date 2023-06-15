import sys

if sys.implementation.name == "micropython":
	import uasyncio as asyncio
	import utime as time
else:
	import asyncio
	import time

_SERIAL = None

# MICROPYTHON
if sys.implementation.name == "micropython":
	# https://github.com/raspberrypi/pico-micropython-examples/blob/master/uart/loopback/uart.py
	from machine import UART, Pin
	
	def _uart_init():
		_SERIAL = UART(1,
			tx=Pin(4), rx=Pin(5),
			baudrate=2400,
			parity=0,
			stop=1,
			bits=8,
			timeout=0
		)
	
	def _uart_read():
		d = bytes()
		while _SERIAL.any() > 0:
			d += _SERIAL.read(1)
		return d
# PYTHON
else:
	# https://stackoverflow.com/a/16078029
	import serial
	import os
	from datetime import datetime
	
	#
	# UART
	#
	def _uart_init():
		_SERIAL = serial.Serial(
			port=os.getenv('UART_PORT'), # TODO
			baudrate=2400,
			parity=serial.PARITY_EVEN,
			stopbits=serial.STOPBITS_ONE,
			bytesize=serial.EIGHTBITS,
			timeout=0
		)
	
	def _uart_read():
		return _SERIAL.read()


async def uart_init():
	print("initializing UART")
	_uart_init()

async def uart_read():
	lastRead = time.ticks_ms()
	data = bytes()
	while (lastRead - time.ticks_ms()) < 2000:
		chunk = _uart_read()
		if len(chunk) > 0:
			lastRead = time.ticks_ms()
			data += chunk
	return data
