# Changelog

## 3.2

- Fixed: no Home Assistant sensors were created when the meter does not send its meter number (e.g. some Sagemcom T210-D meters)
- The sensors are announced again every hour, so they come back if the MQTT broker lost them

**Note:** the sensor IDs changed. If you already got MBusPico sensors with 3.1, a second MBusPico device appears; delete the old one in Home Assistant (Settings → Devices & services → MQTT) and use the new sensors in your Energy dashboard.

## 3.1

- First release as Home Assistant add-on
- Meter values are announced to Home Assistant via MQTT discovery
- Uses the MQTT broker of Home Assistant (Mosquitto broker add-on) when no broker is configured
