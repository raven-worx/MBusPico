# MBusPico add-on

MBusPico reads and decrypts the data your smart meter sends on its M-Bus customer interface.
The add-on provides the meter values as Home Assistant sensors (via MQTT), as JSON via HTTP and as JSON via UDP.

## Requirements

- An M-Bus slave module (e.g. [M-BUS SLAVE CLICK](https://www.mikroe.com/m-bus-slave-click)) connected to the customer interface of your smart meter. See the [MBusPico project](https://github.com/raven-worx/MBusPico) for the wiring.
- A serial connection from the M-Bus module to your Home Assistant machine: a 3.3 V USB serial adapter (recommended) or the GPIO serial port of a Raspberry Pi (see [Serial port](#serial-port)).
- The device key of your smart meter. You must request it from your power grid operator.

## Installation

1. In Home Assistant open **Settings → Add-ons → Add-on store**, open the menu (⋮) → **Repositories** and add `https://github.com/raven-worx/MBusPico`.
2. Install the **MBusPico** add-on.
3. On the **Configuration** tab select your meter profile, enter the device key and select the serial port.
4. Start the add-on. The **Log** tab shows every meter reading as JSON.

## Home Assistant sensors (MQTT)

1. Install and start the **Mosquitto broker** add-on and set up the **MQTT** integration.
2. In the MBusPico configuration enable **MQTT** and leave **MQTT broker address** empty. The add-on then uses the broker of Home Assistant, including its credentials.
3. Restart the add-on. After the first meter reading a device **MBusPico** with these sensors appears in the MQTT integration:

| Sensor                 | Unit | Meter value          |
| ---------------------- | ---- | -------------------- |
| Power import           | W    | activePowerPlus      |
| Power export           | W    | activePowerMinus     |
| Energy import          | Wh   | activeEnergyPlus     |
| Energy export          | Wh   | activeEnergyMinus    |
| Reactive energy import | varh | reactiveEnergyPlus   |
| Reactive energy export | varh | reactiveEnergyMinus  |
| Voltage L1/L2/L3       | V    | voltageL1/L2/L3      |
| Current L1/L2/L3       | A    | currentL1/L2/L3      |
| Power factor           | -    | powerFactor          |

For the **Energy dashboard** use *Energy import* as grid consumption and *Energy export* as return to grid.

To use another MQTT broker, enter its address, port, username and password.
Home Assistant only creates the sensors if its MQTT integration is connected to the same broker.
Disable **Home Assistant sensors** to publish the meter values only, without creating sensors.

The meter values are published to the topics `<MQTT topic prefix>/<meter value>`, e.g. `MBusPico/activePowerPlus`.

## HTTP

When **HTTP server** is enabled, the latest meter data is available as JSON at `http://<home-assistant-address>:8080/`.
The port can be changed in the **Network** section of the configuration.
Set **HTTP username** and **HTTP password** to protect the data with authentication.

## UDP

When **UDP sender** is enabled, the meter data is sent as JSON to the configured receiver in the configured interval.
See [Loxone integration](https://github.com/raven-worx/MBusPico/blob/main/loxone/README.md) for how to use this with a Loxone Miniserver.

## Serial port

The smart meter sends with 2400 baud, 8 data bits, even parity and 1 stop bit (8E1).

**USB serial adapter:** use a 3.3 V adapter. If the serial port list offers a `/dev/serial/by-id/...` entry for it, prefer it, because it does not change when other USB devices are added.

**Raspberry Pi GPIO serial port:** the mini UART (`/dev/ttyS0`) does not support parity, so the M-Bus module must be connected to the PL011 UART (`/dev/ttyAMA0`).
On a Raspberry Pi 3 or 4 the PL011 UART is used for Bluetooth by default. To use it on the GPIO pins (GPIO14/GPIO15), add the following lines to `config.txt` on the boot partition of the Home Assistant OS SD card or disk and restart:

```
enable_uart=1
dtoverlay=disable-bt
```

This disables the onboard Bluetooth. For other Raspberry Pi models see the Raspberry Pi documentation.
