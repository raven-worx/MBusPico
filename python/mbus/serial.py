import sys
from . import meterdata

_SERIAL = None

# MICROPYTHON
if sys.implementation.name == "micropython":
	# https://github.com/raspberrypi/pico-micropython-examples/blob/master/uart/loopback/uart.py
	from machine import UART, Pin
	from ._datetime import datetime
	
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
	from Cryptodome.Cipher import AES
	
	#
	# UART
	#
	def _uart_init():
		_SERIAL = serial.Serial(
			port=os.getenv('UART_PORT'),
			baudrate=2400,
			parity=serial.PARITY_EVEN,
			stopbits=serial.STOPBITS_ONE,
			bytesize=serial.EIGHTBITS,
			timeout=0
		)
	
	def _uart_read():
		return _SERIAL.read()


def serial_loop():
	_uart_init()
	while True:
		data = _uart_read()
		if data.length > 0:
			_parse_data(data)

def serial_exit():
	pass # exit loop
