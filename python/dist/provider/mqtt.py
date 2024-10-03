import sys
import random

if sys.implementation.name == "micropython":
	import uasyncio as asyncio
else:
	import asyncio
	import paho.mqtt.publish as publish

async def Send(meterdata, config):
	if sys.implementation.name == "micropython":
		return

	cid = "MBusPico-" + str(random.getrandbits(24))
	
	prefix = config.MBUSPICO_MQTT_TOPIC_PREFIX.strip()
	if len(prefix) > 0:
		prefix += "/"
	
	msgs = [
		{'topic':prefix+"activePowerPlus", 'payload':str(meterdata.activePowerPlus), 'qos':0, 'retain':False},
		{'topic':prefix+"activePowerMinus", 'payload':str(meterdata.activePowerMinus), 'qos':0, 'retain':False},
		{'topic':prefix+"activeEnergyPlus", 'payload':str(meterdata.activeEnergyPlus), 'qos':0, 'retain':False},
		{'topic':prefix+"activeEnergyMinus", 'payload':str(meterdata.activeEnergyMinus), 'qos':0, 'retain':False},
		{'topic':prefix+"reactiveEnergyPlus", 'payload':str(meterdata.reactiveEnergyPlus), 'qos':0, 'retain':False},
		{'topic':prefix+"reactiveEnergyMinus", 'payload':str(meterdata.reactiveEnergyMinus), 'qos':0, 'retain':False},
		{'topic':prefix+"voltageL1", 'payload':str(meterdata.voltageL1), 'qos':0, 'retain':False},
		{'topic':prefix+"voltageL2", 'payload':str(meterdata.voltageL2), 'qos':0, 'retain':False},
		{'topic':prefix+"voltageL3", 'payload':str(meterdata.voltageL3), 'qos':0, 'retain':False},
		{'topic':prefix+"currentL1", 'payload':str(meterdata.currentL1), 'qos':0, 'retain':False},
		{'topic':prefix+"currentL2", 'payload':str(meterdata.currentL2), 'qos':0, 'retain':False},
		{'topic':prefix+"currentL3", 'payload':str(meterdata.currentL3), 'qos':0, 'retain':False},
		{'topic':prefix+"powerFactor", 'payload':str(meterdata.powerFactor), 'qos':0, 'retain':False},
		{'topic':prefix+"timestamp", 'payload':meterdata.timestamp, 'qos':0, 'retain':False},
		{'topic':prefix+"lxTimestamp", 'payload':str(meterdata.lxTimestamp), 'qos':0, 'retain':False},
		{'topic':prefix+"meterNumber", 'payload':meterdata.meterNumber, 'qos':0, 'retain':False}
	]
	
	if len(config.MBUSPICO_MQTT_AUTH_USER) > 0:
		broker_auth={'username':config.MBUSPICO_MQTT_AUTH_USER, 'password':config.MBUSPICO_MQTT_AUTH_PWD}
	else:
		broker_auth=None
	
	publish.multiple(msgs, hostname=config.MBUSPICO_MQTT_BROKER_ADDRESS, port=config.MBUSPICO_MQTT_BROKER_PORT, auth=broker_auth, client_id=cid)

