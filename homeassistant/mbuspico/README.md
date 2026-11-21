# MBusPico Home Assistant add-on

Reads and decrypts the data of your smart meter via its M-Bus customer interface and provides it

* as Home Assistant sensors (via MQTT, ready for the Energy dashboard),
* as JSON via HTTP,
* as JSON via UDP (e.g. for a Loxone Miniserver).

Supported smart meters:

* Kaifa MA309M (Netz NÖ / EVN)
* Sagemcom T210-D (Netz NÖ / EVN)

See the **Documentation** tab of the add-on for setup instructions, and the
[MBusPico project](https://github.com/raven-worx/MBusPico) for the required hardware.
