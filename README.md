# MBusPico

[![GitHub release (latest SemVer)](https://img.shields.io/github/v/release/raven-worx/mbuspico?logo=github&sort=semver)](https://github.com/raven-worx/mbuspico/releases)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](/LICENSE)

This project was created mostly out of interest and the wish to read out and display my power meter data in my Loxone smart home setup. (Thus the option with the 24V power supply - see below)

Currently the only smart meter device supported is the **Kaifa MA309M** (Austrian power provider Netz NÖ), but could be extended to any other model. Credits for the data intepretation code goes out to **[firegore/esphome-dlms-meter](https://github.com/firegore/esphome-dlms-meter)**!

The read meter data values are published via HTTP and/or UDP in JSON format:

| **Field**            |  **Description / Unit** |
| -------------------- | ----------------------  |
| timestamp            | Timestamp of the reading in the format "0000-00-00T00:00:00Z" |
| lxTimestamp          | Loxone timestamp (seconds since 1.1.2009) |
| meterNumber          | serial number of the meter device |
| activePowerPlus      | [W]                     |
| activePowerMinus     | [W]                     |
| activeEnergyPlus     | [Wh]                    |
| activeEnergyMinus    | [Wh]                    |
| voltageL1            | [V]                     |
| voltageL2            | [V]                     |
| voltageL3            | [V]                     |
| currentL1            | [A]                     |
| currentL2            | [A]                     |
| currentL3            | [A]                     |
| powerFactor          | [see Wikipedia](https://en.wikipedia.org/wiki/Power_factor) |

```
{
"timestamp": "2022-11-20T19:03:40Z",
"lxTimestamp": 438203020,
"meterNumber": "123456789012",
"activePowerPlus": 472.00,
"activePowerMinus": 0.00,
"activeEnergyPlus": 1385960.00,
"activeEnergyMinus": 0.00,
"voltageL1": 235.00,
"voltageL2": 234.30,
"voltageL3": 235.00,
"currentL1": 1.72,
"currentL2": 0.17,
"currentL3": 0.99,
"powerFactor": 823.00
}
```

## Hardware

* [RaspberryPi Pico W(H)](https://amzn.to/3OlR064)
* [M-BUS SLAVE CLICK module](https://www.mikroe.com/m-bus-slave-click)
* [RJ11/RJ12 cable](https://amzn.to/3TSGO6r) (or an old telephone cable you may have lying around)
* [24V -> 3.3V step down regulator]
    * [Example](https://www.berrybase.at/step-down-netzteilmodul-4-38v-1-25-36v/5a-mit-schraubklemmen) (used in 3d case v2)
    * [Example](https://www.reichelt.at/at/de/dc-dc-wandler-3-3-w-3-3-v-1000-ma-sil-lmo78-03-1-0-p242837.html?search=lmo78_03&&r=1)
* [power connector clamp](https://www.berrybase.at/anschlussklemme-schraubbar-900-gewinkelt-1-25mm2-rm-5-08mm-2-polig) (optional)

*Note*: the links of the above listed hardware components to Amazon are exemplary. If you want to support me you can use them to order the linked products using these links.

## Software

The software for the Pico W microcontroller is written in C using
* [pico-sdk](https://github.com/raspberrypi/pico-sdk)
* [FreeRTOS](https://github.com/FreeRTOS/FreeRTOS-Kernel)
* [mbedtls](https://github.com/Mbed-TLS/mbedtls)
* [mongoose](https://github.com/cesanta/mongoose)

## Build

1. Download and extract [latest release archive](https://github.com/raven-worx/mbuspico/releases) - or clone repository: `git clone --recurse-submodules https://github.com/raven-worx/MBusPico.git`
2. Edit build options (see below) in `options.ini` to configure MBusPico application to your needs
3. start building (e.g. using Docker - see below)

### Linux

Simply run the `build.sh` script.

Make sure you have set the `PICO_SDK_PATH` environment variable pointing to your local pico-sdk checkout.
If you do not have a checkout of the pico-sdk yet, you can instead add the option `PICO_SDK_FETCH_FROM_GIT=ON` in the `options.ini` file to automatically checkout the pico-sdk as part of the build process.

### Windows

For Windows i recommend using [Docker](https://docs.docker.com/get-docker/) for a quick and painless build.

Create a (local) docker image with the file provided in the `docker` folder of this repository:
```console
docker build -t mbuspico/build -f docker/Dockerfile
```
After the build of the docker image has finished (might take a few minutes) simply start the built docker image which implicitly starts building the application:
```console
docker run -v ${PWD}:/opt/mbuspico mbuspico/build
```

### Build options

| **Option**            | Default value | **required**            |  **Description** |
| --------------------- | ------------- | ----------------------- |  --------------- |
| MBUSPICO_DEVICE_KEY   |               | yes / device dependant  | Key used to decrypt data read from the meter. Must be requested from the power provider. |
| MBUSPICO_LOG_LEVEL    | LOG_ERROR     | no                      | level of log verbosity. Possible values: LOG_NONE, LOG_ERROR, LOG_INFO, LOG_DEBUG |
| MBUSPICO_WIFI_ENABLED | ON            | no                      | specifies if the device should connect to wifi network |
| MBUSPICO_WIFI_SSID    |               | yes, if MBUSPICO_WIFI_ENABLED | Wifi SSID to connect to |
| MBUSPICO_WIFI_PWD     |               | yes, if MBUSPICO_WIFI_ENABLED | Wifi password |
| MBUSPICO_WIFI_HOSTNAME | "MBusPico"   | no                      | The hostname of the Pico W device in the network |
| MBUSPICO_UDP_ENABLED  | OFF           | no                      | specifies if the device should send out the read meter data via UDP |
| MBUSPICO_UDP_RECEIVER_HOST |          | yes, if MBUSPICO_UDP_ENABLED | the UDP receiver address the meter data packet should be send to |
| MBUSPICO_UDP_RECEIVER_PORT |          | yes, if MBUSPICO_UDP_ENABLED | the UDP receiver port the meter data packet should be send to |
| MBUSPICO_UDP_INTERVAL_S | 30          | no                      | the maximum interval [sec] the meter data should be send out via UDP |
| MBUSPICO_HTTP_ENABLED | ON            | no                      | specifies if the device should launch a simple HTTP server to provide the read meter data |
| MBUSPICO_HTTP_SERVER_PORT  | 80       | no                      | specifies the listening port for the HTTP webserver |
| MBUSPICO_HTTP_AUTH_USER |             | no                      | Protect access to data with authentication
| MBUSPICO_HTTP_AUTH_PWD |              | no                      | Protect access to data with authentication

# Transfer MBusPico onto the device

Hold the BOOTSEL button on the Pico W while connecting it to the PC via a USB cable. Since v1.2 its also possible to call `http://<ip-address>/update` to restart into the bootloader (if built with HTTP option).
The Pico will show up as a flash drive. Copy the `mbuspico.uf2` file from the build directory to the appeared flash drive. The Pico W will automatically reboot into the just flashed firmware after copying has finished. Done.

# HTTP endpoints

When built with `MBUSPICO_HTTP_ENABLED` option enabled the following HTTP endpoints are provided.

| **Method**    | **Endpoint URL**      | **Description**    |
| --------------| --------------------- | ------------------ |
| GET           | /                     | Return last read meter data in JSON format |
| GET           | /update               | Reboot device into USB bootloader for a quick and easy update of MBusPico application |
| GET           | /reboot               | Reboot device      |

# Hardware wiring / assembly

See the tables below how to wire the Raspberry Pico W with the used hardware components and power supply.

| **Pico W**      | **M-Bus Slave CLICK**   | **RJ11** |  **Notes** |
| --------------- | ----------------------- | -------- |  --------- |
| GPIO4 (PIN 6)   | RX                      | -        | UART RX of MBUS module <--> UART1 TX of Pico W |
| GPIO5 (PIN 7)   | TX                      | -        | UART TX of MBUS module <--> UART1 RX of Pico W |
| -               | MBUS1                   | 3        | MBUS module to RJ11 cable line 3 or 4 (polarity doesn't matter) |
| -               | MBUS2                   | 4        | MBUS module to RJ11 cable line 3 or 4 (polarity doesn't matter) |

Power supply (24V):

| **Pico W**      | **M-Bus Slave CLICK**   | **24V -> 3.3V module**  | **24V power source** |  **Notes** |
| --------------- | ----------------------- | ----------------------- | -------------------- |  --------- |
| VSYS (PIN 39)   | -                       | VOUT                    |                      | 3.3V power supply for Pico W |
| GND (PIN 38)    | -                       | GND                     |                      | Ground |
| -               | 3V3                     | VOUT                    |                      | 3.3V power supply for MBus CLICK module |
| -               | GND                     | GND                     |                      | Ground |
| -               | -                       | VIN                     | DC+/24V              | 24V external power supply |
| -               | -                       | GND                     | DC-/GND              | Ground |

[<img src="/3d/image.jpg?raw=true" width="600"/>](/3d/image.jpg?raw=true)

# Loxone integration

The folder `loxone` contains a ready made HTTP VI template file for quick integration into Loxone Config.

[<img src="/loxone/config.png?raw=true" width="600"/>](/loxone/config.png?raw=true)

Visualization in Loxone App:

[<img src="/loxone/app.png?raw=true" width="200"/>](/loxone/app.png?raw=true)

# 3D printer files

I created a 3d printable *reference design* of a case for the used hardware components listed above.
You can find the files in the `3d` folder of this repository.

# License

Licensed under [GPLv3](/LICENSE)
