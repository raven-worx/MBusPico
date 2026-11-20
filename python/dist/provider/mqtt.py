import sys
import random

if sys.implementation.name == "micropython":
	import uasyncio as asyncio
else:
	import asyncio
	import json
	import os
	import urllib.request
	import paho.mqtt.publish as publish

# Home Assistant Supervisor endpoint returning the MQTT broker provided by an add-on (e.g. Mosquitto broker)
_SUPERVISOR_MQTT_SERVICE_URL = "http://supervisor/services/mqtt"
# topic prefix Home Assistant listens on for MQTT discovery
_HA_DISCOVERY_PREFIX = "homeassistant"

# broker connection settings resolved by setup(); None if MQTT can not be used
_BROKER = None
# meter number the Home Assistant discovery config was last published for
_DISCOVERY_PUBLISHED_FOR = None

# meter values announced to Home Assistant: (field, name, unit, device class, state class)
_HA_SENSORS = [
	("activePowerPlus", "Power import", "W", "power", "measurement"),
	("activePowerMinus", "Power export", "W", "power", "measurement"),
	("activeEnergyPlus", "Energy import", "Wh", "energy", "total_increasing"),
	("activeEnergyMinus", "Energy export", "Wh", "energy", "total_increasing"),
	("reactiveEnergyPlus", "Reactive energy import", "varh", None, "total_increasing"),
	("reactiveEnergyMinus", "Reactive energy export", "varh", None, "total_increasing"),
	("voltageL1", "Voltage L1", "V", "voltage", "measurement"),
	("voltageL2", "Voltage L2", "V", "voltage", "measurement"),
	("voltageL3", "Voltage L3", "V", "voltage", "measurement"),
	("currentL1", "Current L1", "A", "current", "measurement"),
	("currentL2", "Current L2", "A", "current", "measurement"),
	("currentL3", "Current L3", "A", "current", "measurement"),
	# the scale of the power factor differs between meter profiles -> published without unit/device class
	("powerFactor", "Power factor", None, None, "measurement"),
]

# meter profile -> (manufacturer, model) shown in Home Assistant
_HA_DEVICE_MODELS = {
	"KAIFA_MA309M_NETZNOE": ("Kaifa", "MA309M"),
	"SAGEMCOM_T210D": ("Sagemcom", "T210-D"),
}


def _supervisor_mqtt_service():
	# Only available when running as Home Assistant add-on:
	# the Supervisor passes SUPERVISOR_TOKEN to every add-on it runs
	token = os.getenv("SUPERVISOR_TOKEN")
	if not token:
		return None
	request = urllib.request.Request(_SUPERVISOR_MQTT_SERVICE_URL, headers={"Authorization": "Bearer " + token})
	try:
		with urllib.request.urlopen(request, timeout=10) as response:
			result = json.load(response)
	except Exception as e:
		print("MQTT: Home Assistant provides no MQTT broker (is the Mosquitto broker add-on installed?):", e)
		return None
	data = result.get("data", result) if isinstance(result, dict) else None
	if not isinstance(data, dict) or not data.get("host"):
		print("MQTT: Home Assistant provides no MQTT broker (is the Mosquitto broker add-on installed?)")
		return None
	return data


def setup(config):
	# Resolves the MQTT broker to publish to. Returns False if MQTT can not be used.
	global _BROKER
	_BROKER = None
	if sys.implementation.name == "micropython":
		return False

	host = config.MBUSPICO_MQTT_BROKER_ADDRESS.strip()
	port = config.MBUSPICO_MQTT_BROKER_PORT
	user = config.MBUSPICO_MQTT_AUTH_USER
	password = config.MBUSPICO_MQTT_AUTH_PWD
	source = "configured"

	if not host:
		# no broker configured: use the broker of Home Assistant (only when running as add-on)
		service = _supervisor_mqtt_service()
		if service:
			host = str(service["host"])
			port = int(service.get("port") or 1883)
			user = service.get("username") or ""
			password = service.get("password") or ""
			source = "Home Assistant"
			if service.get("ssl"):
				print("MQTT: the broker of Home Assistant requires SSL, which is not supported")

	if not host:
		print("MQTT: no broker address configured -> MQTT disabled")
		return False

	_BROKER = {
		"hostname": host,
		"port": port,
		"auth": {'username':user, 'password':password} if len(user) > 0 else None,
	}
	print("MQTT: using " + source + " broker " + host + ":" + str(port))
	return True


def _ha_id(text):
	# Home Assistant ids and MQTT topic levels: only use [a-zA-Z0-9_-]
	return "".join([c if (c.isalnum() or c in "_-") else "_" for c in text])


def _ha_discovery_messages(meterdata, prefix, config):
	node_id = "mbuspico_" + _ha_id(meterdata.meterNumber)
	manufacturer, model = _HA_DEVICE_MODELS.get(config.MBUSPICO_DEVICE.upper(), (None, config.MBUSPICO_DEVICE))
	device = {
		"identifiers": [node_id],
		"name": "MBusPico",
		"model": model,
		"serial_number": meterdata.meterNumber,
	}
	if manufacturer:
		device["manufacturer"] = manufacturer
	origin = {"name": "MBusPico", "support_url": "https://github.com/raven-worx/MBusPico"}

	msgs = []
	for field, name, unit, device_class, state_class in _HA_SENSORS:
		payload = {
			"name": name,
			"unique_id": node_id + "_" + field.lower(),
			"state_topic": prefix + field,
			"state_class": state_class,
			"device": device,
			"origin": origin,
		}
		if unit:
			payload["unit_of_measurement"] = unit
		if device_class:
			payload["device_class"] = device_class
		topic = _HA_DISCOVERY_PREFIX + "/sensor/" + node_id + "/" + field.lower() + "/config"
		msgs.append({'topic':topic, 'payload':json.dumps(payload), 'qos':0, 'retain':True})
	return msgs


async def Send(meterdata, config):
	global _DISCOVERY_PUBLISHED_FOR
	if sys.implementation.name == "micropython" or _BROKER is None:
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
	
	# announce the sensors to Home Assistant once per meter (retained, so Home Assistant gets them after a restart too).
	# Requires the meter number, as it is part of the sensor ids.
	discovery_for = None
	if config.MBUSPICO_HA_MQTT_DISCOVERY and meterdata.meterNumber and meterdata.meterNumber != _DISCOVERY_PUBLISHED_FOR:
		discovery_for = meterdata.meterNumber
		msgs = _ha_discovery_messages(meterdata, prefix, config) + msgs

	try:
		publish.multiple(msgs, client_id=cid, **_BROKER)
	except Exception as e:
		print("MQTT: publishing meter data failed:", e)
		return

	if discovery_for is not None:
		_DISCOVERY_PUBLISHED_FOR = discovery_for
		print("MQTT: published Home Assistant discovery for meter", discovery_for)

