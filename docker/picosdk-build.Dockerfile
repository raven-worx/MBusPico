# image to build MBusPico against pico-sdk

FROM debian:bullseye-slim

ENV DEBIAN_FRONTEND=noninteractive

RUN apt update && apt install -y git cmake gcc-arm-none-eabi python3 g++

RUN git clone --branch "1.5.0" https://github.com/raspberrypi/pico-sdk /opt/pico-sdk \
    && cd /opt/pico-sdk \
    && git submodule update --init --recursive
ENV PICO_SDK_PATH=/opt/pico-sdk

WORKDIR /opt/mbuspico
ENTRYPOINT ["/opt/mbuspico/build.sh"]