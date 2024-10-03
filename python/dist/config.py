import sys
if sys.implementation.name == "micropython":
	def OPTION(name,value):
		return value
else:
	import os
	def OPTION(name,defaultValue):
		return os.getenv(name,defaultValue)

#################################
#  USER CONFIGURATION
#################################

# The device key is required for decryption of the MBus meter data
# You must request this key for your Smart Meter from your power provider
# The key must be exactly 32 characters long (hex format -> only alphanumeric characters [a-f0-9])
#  e.g. '1A2B3CFF1234DE567890F1234DE5EE85'
MBUSPICO_DEVICE_KEY = OPTION("MBUSPICO_DEVICE_KEY", "")

# ------ SERIAL PORT -------
#  NOTE: only used when running on Raspberry Pi / CPython

# the serial port to read MBus data from
MBUSPICO_SERIAL_PORT = OPTION("MBUSPICO_SERIAL_PORT", "/dev/ttyS0")

# ---------- WIFI ----------
#  NOTE: only used when running on RaspberryPi Pico / MicroPython

# when 'True' the device should connect to a Wifi network, 'False' to disable
MBUSPICO_WIFI_ENABLED = True
# The Wifi SSID to connect to
MBUSPICO_WIFI_SSID = ""
# The password of the Wifi network
MBUSPICO_WIFI_PWD = ""
# The hostname of the device on the Wifi network
MBUSPICO_WIFI_HOSTNAME = "MBusPico"

# ------ UDP RECEIVER ------

# when 'True' the device should send the read meter data to a UDP server on the network
MBUSPICO_UDP_ENABLED = OPTION("MBUSPICO_UDP_ENABLED", False)
# The network address/hostname of the UDP receiver
MBUSPICO_UDP_RECEIVER_HOST = OPTION("MBUSPICO_UDP_RECEIVER_HOST", "127.0.0.1")
# The UDP port the receiver is listening on
MBUSPICO_UDP_RECEIVER_PORT = OPTION("MBUSPICO_UDP_RECEIVER_PORT", 0)
# The send interval (seconds). NOTE: Dont set the value too low
MBUSPICO_UDP_INTERVAL_S = OPTION("MBUSPICO_UDP_INTERVAL_S", 30)

# ------ HTTP SERVER -------

# when 'True' the device should provide the meter data via a simple web interface
MBUSPICO_HTTP_ENABLED = OPTION("MBUSPICO_HTTP_ENABLED", True)
# the port on the web server should be launched
MBUSPICO_HTTP_SERVER_PORT = OPTION("MBUSPICO_HTTP_SERVER_PORT", 80)
# username/password required to view the meter data, if both are empty no authentication is required
MBUSPICO_HTTP_AUTH_USER = OPTION("MBUSPICO_HTTP_AUTH_USER", "")
MBUSPICO_HTTP_AUTH_PWD = OPTION("MBUSPICO_HTTP_AUTH_PWD", "")

# --------- MQTT -----------

# when 'True' the device should send the meter data via MQTT to a configured broker
MBUSPICO_MQTT_ENABLED = OPTION("MBUSPICO_MQTT_ENABLED", False)
# the network address of the MQTT broker
MBUSPICO_MQTT_BROKER_ADDRESS = OPTION("MBUSPICO_MQTT_BROKER_ADDRESS", "")
# the port of the MQTT broker
MBUSPICO_MQTT_BROKER_PORT = OPTION("MBUSPICO_MQTT_BROKER_PORT", 1883)
# username/password used for authentication on MQTT broker
MBUSPICO_MQTT_AUTH_USER = OPTION("MBUSPICO_MQTT_AUTH_USER", "")
MBUSPICO_MQTT_AUTH_PWD = OPTION("MBUSPICO_MQTT_AUTH_PWD", "")
# MQTT topic prefix
MBUSPICO_MQTT_TOPIC_PREFIX = OPTION("MBUSPICO_MQTT_TOPIC_PREFIX", "MBusPico")
