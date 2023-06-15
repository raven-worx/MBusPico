import sys
import socket

if sys.implementation.name == "micropython":
	import uasyncio as asyncio
else:
	import asyncio


METERDATA = None

async def run(config):
	print("Starting UDP task")
	sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
	receiver = (config.MBUSPICO_UDP_RECEIVER_HOST, config.MBUSPICO_UDP_RECEIVER_PORT)
	while True:
		if METERDATA:
			print("UDP - send to:", receiver)
			sock.sendto(METERDATA.to_json().encode('utf-8'), receiver)
		await asyncio.sleep(config.MBUSPICO_UDP_INTERVAL_S) # [s]

async def stop():
	pass