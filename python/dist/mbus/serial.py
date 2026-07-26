import sys
import config

if sys.implementation.name == "micropython":
	import uasyncio as asyncio
	import utime as time
else:
	import asyncio
	import time

_SERIAL = None
_RX_BUFFER = bytearray()


def _extract_mbus_frame():
	global _RX_BUFFER
	start = _RX_BUFFER.find(0x68)
	if start < 0:
		if len(_RX_BUFFER) > 0:
			_RX_BUFFER = bytearray()
		return None
	if start > 0:
		del _RX_BUFFER[:start]

	if len(_RX_BUFFER) < 4:
		return None
	if _RX_BUFFER[1] != _RX_BUFFER[2] or _RX_BUFFER[3] != 0x68:
		del _RX_BUFFER[0]
		return None

	frame_length = _RX_BUFFER[1]
	total_length = 4 + frame_length + 2
	if len(_RX_BUFFER) < total_length:
		return None
	if _RX_BUFFER[total_length - 1] != 0x16:
		del _RX_BUFFER[0]
		return None

	frame = bytes(_RX_BUFFER[:total_length])
	del _RX_BUFFER[:total_length]
	return frame

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

	def _get_serial_parity():
		parity_name = str(config.MBUSPICO_SERIAL_PARITY).upper()
		parity_map = {
			"E": serial.PARITY_EVEN,
			"EVEN": serial.PARITY_EVEN,
			"N": serial.PARITY_NONE,
			"NONE": serial.PARITY_NONE,
		}
		if parity_name not in parity_map:
			raise ValueError("Unsupported MBUSPICO_SERIAL_PARITY: " + str(config.MBUSPICO_SERIAL_PARITY))
		return parity_name, parity_map[parity_name]
	
	def _uart_init():
		global _SERIAL
		parity_name, parity = _get_serial_parity()
		_SERIAL = serial.Serial(
			config.MBUSPICO_SERIAL_PORT,
			2400,
			bytesize=serial.EIGHTBITS,
			parity=parity,
			stopbits=serial.STOPBITS_ONE,
		)
		print("configured serial parity:", parity_name)
	
	def _uart_read():
		global _SERIAL
		data = bytes()
		avail = _SERIAL.inWaiting()
		if avail > 0:
			data += _SERIAL.read(avail)
		return data
	
	def _get_time():
		return round(time.time() * 1000)
	
	def _time_diff(t1,t2):
		return t1-t2


async def uart_init():
	_uart_init()
	print("initialized UART")

async def uart_read():
	global _RX_BUFFER
	frame = _extract_mbus_frame()
	if frame is not None:
		return frame

	lastRead = _get_time()
	while _time_diff(_get_time(),lastRead) < 1000:
		chunk = _uart_read()
		if len(chunk) > 0:
			lastRead = _get_time()
			_RX_BUFFER.extend(chunk)
			frame = _extract_mbus_frame()
			if frame is not None:
				return frame
		await asyncio.sleep(0.1)
	return bytes()
