#!/usr/bin/env python3

from mbus import serial
from mbus.devices import kaifa_ma309m_netznoe, sagemcom_t210d
from provider import www, udp, mqtt
import config
import sys

if sys.implementation.name == "micropython":
	import uasyncio as asyncio
	import network
else:
	import asyncio

async def connect_wifi(wlan,ssid,password):
	if wlan.isconnected():
		return
	print("connecting to WIFI: '", ssid, "'")
	wlan.connect(ssid, password)
	while wlan.isconnected() == False:
		print('Waiting for wifi connection...')
		await asyncio.sleep(1.5) # [s]
	print("WIFI connected:", wlan.ifconfig())


def create_device():
	device_name = config.MBUSPICO_DEVICE.upper()
	if device_name == "KAIFA_MA309M_NETZNOE":
		return kaifa_ma309m_netznoe.Kaifa_MA309M_NetzNoe(config.MBUSPICO_DEVICE_KEY)
	if device_name == "SAGEMCOM_T210D":
		return sagemcom_t210d.Sagemcom_T210D(config.MBUSPICO_DEVICE_KEY)
	raise ValueError("Unsupported MBUSPICO_DEVICE: " + config.MBUSPICO_DEVICE)


async def handler_task():
	dev = create_device()
	print("selected meter profile:", config.MBUSPICO_DEVICE)

	await serial.uart_init()
	
	if config.MBUSPICO_WIFI_ENABLED and sys.implementation.name == "micropython":
		wifi_hostname = config.MBUSPICO_WIFI_HOSTNAME if config.MBUSPICO_WIFI_HOSTNAME else "MBusPico"
		network.hostname(wifi_hostname)
		wlan = network.WLAN(network.STA_IF)
		wlan.config(hostname=wifi_hostname)
		wlan.active(True)
		if wlan.isconnected():
			print("WIFI connected:", wlan.ifconfig())
	
	while True:
		if config.MBUSPICO_WIFI_ENABLED and sys.implementation.name == "micropython":
			await connect_wifi(wlan, config.MBUSPICO_WIFI_SSID, config.MBUSPICO_WIFI_PWD)
		
		# read serial data
		data = await serial.uart_read()
		if len(data) > 0:
			# parse meter data
			meter = dev.parse_data(data)
			if meter:
				METERDATA = meter
				udp.METERDATA = meter
				www.METERDATA = meter
				if config.MBUSPICO_MQTT_ENABLED and sys.implementation.name != "micropython":
					asyncio.create_task(mqtt.Send(meter, config))
			else:
				print("parsing meter data failed")
		await asyncio.sleep(0)

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
