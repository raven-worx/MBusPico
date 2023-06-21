import sys
import config

if sys.implementation.name == "micropython":
	import uasyncio as asyncio
	import utime as time
else:
	import asyncio
	import time

_SERIAL = None

# MICROPYTHON
if sys.implementation.name == "micropython":
	from machine import UART, Pin
	
	def _uart_init():
		global _SERIAL
		_SERIAL = UART(1,
			tx=Pin(4), rx=Pin(5),
			baudrate=2400,
			parity=0,
			stop=1,
			bits=8,
			timeout=0
		)
	
	def _uart_read():
		global _SERIAL
		d = bytes()
		while _SERIAL.any() > 0:
			d += _SERIAL.read(1)
		return d
	
	def _get_time():
		return time.ticks_ms()
	
	def _time_diff(t1,t2):
		return time.ticks_diff(t1,t2)

# PYTHON
else:
	import serial
	import os
	from datetime import datetime
	
	#
	# UART
	#
	def _uart_init():
		global _SERIAL
		_SERIAL = serial.Serial(
			port=config.MBUSPICO_SERIAL_PORT,
			baudrate=2400,
			parity=serial.PARITY_EVEN,
			stopbits=serial.STOPBITS_ONE,
			bytesize=serial.EIGHTBITS,
			timeout=0
		)
	
	def _uart_read():
		global _SERIAL
		return _SERIAL.read()
	
	def _get_time():
		return round(time.time() * 1000)
	
	def _time_diff(t1,t2):
		return t1-t2


async def uart_init():
	_uart_init()
	print("initialized UART")

async def uart_read():
	lastRead = _get_time()
	data = bytes()
	while _time_diff(_get_time(),lastRead) < 1500:
		chunk = _uart_read()
		if len(chunk) > 0:
			lastRead = _get_time()
			data += chunk
		await asyncio.sleep(0.1) # [s]
	print("uart_read():", len(data))
	return data

