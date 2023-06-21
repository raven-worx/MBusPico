# image to build MBusPico against pico-sdk

FROM debian:bookworm-slim

ENV DEBIAN_FRONTEND=noninteractive \
    PICO_SDK_PATH=/opt/pico-sdk

ENTRYPOINT ["/bin/bash"]

RUN apt update && apt install -y git cmake gcc-arm-none-eabi python3 g++ curl python3-venv \
    && apt autoremove -y \
    && apt clean -y \
    && rm -rf /var/lib/apt/lists/* \
    && git clone --branch "1.5.0" https://github.com/raspberrypi/pico-sdk /opt/pico-sdk \
    && cd /opt/pico-sdk \
    && git submodule update --init --recursive