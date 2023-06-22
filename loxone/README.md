# Loxone integration

To integrate MBusPico into Loxone and start optimizing your energy management you can download & import the [HTTP VirtualIn](VI_MBusPico.xml) 
or [UDP VirtualIn](VIU_MBusPico.xml) template into Loxone Config.

[<img src="config.png?raw=true" width="600"/>](config.png?raw=true)

Visualization in Loxone App:

[<img src="app.png?raw=true" width="200"/>](app.png?raw=true)

## Raspberry Pi / Loxberry

Since MBusPico v2.0 there is also a Python version available. Including a ready-made Python docker container running MBusPico.

This enables the usage of MBusPico application on your RaspberryPi already running [LoxBerry](https://github.com/mschlenstedt/Loxberry).

## Enable serial port

On an generic Raspberry Pi (for example running Raspian) execute  `sudo raspi-config` and enable the serial port in 
`(3) Interface options` -> `(I6) Serial Port`. Just enable the serial port and do not enable the console on it.
Afterwards reboot the device.

On a Loxberry this can be done directly in the web UI. From `System settings` select `Loxberry services` and `More options`:

[<img src="loxberry_enable_serialport.png?raw=true" width="600"/>](loxberry_enable_serialport.png?raw=true)

Again reboot the device after saving the settings.

## Loxberry

Install the [Loxberry Docker Plugin](https://wiki.loxberry.de/plugins/docker/start). In the portainer web UI you can create 
a MBusPico docker container from `Containers` and hit the `Add container` button.

Make sure that you set some mandatory options:

* the `Network mode` must be `host`
* the serial port `device` (in most cases `/dev/ttyS0`) must be mapped to the docker container
* MBusPico options (like at least the decryption key `MBUSPICO_DEVICE_KEY`) can be set via `environment variables`

## Running manually

Extract the `python.zip` archive of an MBusPico release and execute it directly with Python:
```sh
cd /path/to/extracted
python3 main.py
```

Example docker run call:
```sh
docker run --name MBusPico \
	--network host \
	--device=/dev/ttyS0 \
	-e MBUSPICO_DEVICE_KEY=XXXXXXXXXXXXXXXXXXXX \
	-e MBUSPICO_HTTP_SERVER_PORT=8080 \
	-e MBUSPICO_HTTP_ENABLED=1 \
	-e MBUSPICO_SERIAL_PORT=/dev/ttyS0 \
	ravenworx/mbuspico:latest
```
