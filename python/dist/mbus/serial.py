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
_UART_CONFIG = None


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

	def _normalize_uart_config(uart_config):
		config = {
			"baudrate": 2400,
			"data_bits": 8,
			"stop_bits": 1,
			"parity": "EVEN",
		}
		if uart_config is not None:
			config.update(uart_config)
		config["parity"] = str(config["parity"]).upper()
		return config

	def _get_micropython_parity(uart_config):
		parity_name = uart_config["parity"]
		if parity_name in ("E", "EVEN"):
			return parity_name, 0
		if parity_name in ("N", "NONE"):
			return parity_name, None
		raise ValueError("Unsupported UART parity: " + str(uart_config["parity"]))
	
	def _uart_init(uart_config):
		global _SERIAL
		uart_config = _normalize_uart_config(uart_config)
		parity_name, parity = _get_micropython_parity(uart_config)
		_SERIAL = UART(1,
			tx=Pin(4), rx=Pin(5),
			baudrate=uart_config["baudrate"],
			parity=parity,
			stop=uart_config["stop_bits"],
			bits=uart_config["data_bits"],
			timeout=0
		)
		print("configured serial parity:", parity_name)
	
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

	def _normalize_uart_config(uart_config):
		config = {
			"baudrate": 2400,
			"data_bits": 8,
			"stop_bits": 1,
			"parity": "EVEN",
		}
		if uart_config is not None:
			config.update(uart_config)
		config["parity"] = str(config["parity"]).upper()
		return config

	def _get_serial_parity(uart_config):
		parity_name = uart_config["parity"]
		parity_map = {
			"E": serial.PARITY_EVEN,
			"EVEN": serial.PARITY_EVEN,
			"N": serial.PARITY_NONE,
			"NONE": serial.PARITY_NONE,
		}
		if parity_name not in parity_map:
			raise ValueError("Unsupported UART parity: " + str(uart_config["parity"]))
		return parity_name, parity_map[parity_name]
	
	def _uart_init(uart_config):
		global _SERIAL
		uart_config = _normalize_uart_config(uart_config)
		parity_name, parity = _get_serial_parity(uart_config)
		port = uart_config["port"]
		try:
			_SERIAL = serial.Serial(
				port,
				uart_config["baudrate"],
				bytesize=uart_config["data_bits"],
				parity=parity,
				stopbits=uart_config["stop_bits"],
			)
		except Exception:
			resolved_port = os.path.realpath(port)
			if parity_name in ("E", "EVEN") and (port.endswith("serial0") or resolved_port.endswith("ttyS0")):
				print("Hint: /dev/serial0 may point to mini-UART (ttyS0), which does not support 8E1 for direct TSS721 use. Use /dev/ttyAMA0 and move PL011 to GPIO.")
			raise
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


async def uart_init(device):
	global _UART_CONFIG
	_UART_CONFIG = device.uart_config()
	if sys.implementation.name != "micropython":
		_UART_CONFIG = dict(_UART_CONFIG)
		_UART_CONFIG["port"] = _UART_CONFIG.get("port", config.MBUSPICO_SERIAL_PORT)
	_uart_init(_UART_CONFIG)
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
