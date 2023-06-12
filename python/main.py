from mbus import meterdata, serial
from mbus.devices import *
from provider import www, udp
import config
import sys

if sys.implementation.name == "micropython":
	import uasyncio as asyncio
else:
	import asyncio


async def handler_task():
	dev = kaifa_ma309m_netznoe.Kaifa_MA309M_NetzNoe(config.MBUSPICO_DEVICE_KEY)

	await serial.uart_init()
	
	while True:
		# read serial data
		#data = await serial.uart_read()
		if len(data) > 0:
			# parse meter data
			meter = dev.parse_data(data)
			if meter:
				METERDATA = meter
				udp.METERDATA = meter
				www.METERDATA = meter
			else:
				print("parsing meter data failed")
			data = bytes()
		else:
			await asyncio.sleep(1) # 1s

async def main():
	tasks = []
	
	if config.MBUSPICO_UDP_ENABLED:
		tasks.append(asyncio.create_task(udp.run(config))) # udp sender
	if config.MBUSPICO_HTTP_ENABLED:
		tasks.append(asyncio.create_task(www.run(config))) # webserver
	
	
	tasks.append(asyncio.create_task(handler_task())) # uart handler
	
	await asyncio.gather(*tasks)


# Start event loop and run entry point coroutine
asyncio.run(main())