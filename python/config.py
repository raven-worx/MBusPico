#################################
#  USER CONFIGURATION
#################################
import os

# The device key is required for decryption of the read meter data
# You must request this key for your Smart Meter from your power provider
# The key must be exactly 32 characters long (hex format -> only alphanumeric characters)
MBUSPICO_DEVICE_KEY = os.getenv("MBUSPICO_DEVICE_KEY", "")

# LOGLEVEL
#  Controls the verbosity of message log output
#  Possible values:
MBUSPICO_LOG_LEVEL=0


# ---------- WIFI ----------
#  NOTE: only used when running on RaspberryPi Pico (MicroPython)

# when 'True' the device should connect to a Wifi network, 'False' to disable
MBUSPICO_WIFI_ENABLED = True
# The Wifi SSID to connect to
MBUSPICO_WIFI_SSID = "ssid"
# The password of the Wifi network
MBUSPICO_WIFI_PWD = "pwd"
# The hostname of the device on the Wifi network
MBUSPICO_WIFI_HOSTNAME = "MBusPico"

# ------ UDP RECEIVER ------

# when 'True' the device should send the read meter data to a UDP server on the network
MBUSPICO_UDP_ENABLED = os.getenv("MBUSPICO_UDP_ENABLED", False)
# The network address/hostname of the UDP receiver
MBUSPICO_UDP_RECEIVER_HOST = os.getenv("MBUSPICO_UDP_RECEIVER_HOST", "127.0.0.1")
# The UDP port the receiver is listening on
MBUSPICO_UDP_RECEIVER_PORT = os.getenv("MBUSPICO_UDP_RECEIVER_PORT", 0)
# The send interval (seconds). NOTE: Dont set the value too low
MBUSPICO_UDP_INTERVAL_S = os.getenv("MBUSPICO_UDP_INTERVAL_S", 30)

# ------ HTTP SERVER -------

# when 'True' the device should provide the meter data via a simple web interface
MBUSPICO_HTTP_ENABLED = os.getenv("MBUSPICO_HTTP_ENABLED", True)
# the port on the web server should be launched
MBUSPICO_HTTP_SERVER_PORT = os.getenv("MBUSPICO_HTTP_SERVER_PORT", 8080)
# username/password required to view the meter data, if both are empty no authentication is required
MBUSPICO_HTTP_AUTH_USER = os.getenv("MBUSPICO_HTTP_AUTH_USER", "")
MBUSPICO_HTTP_AUTH_PWD = os.getenv("MBUSPICO_HTTP_AUTH_PWD", "")