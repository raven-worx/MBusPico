FROM debian:bullseye-slim

ENV DEBIAN_FRONTEND=noninteractive

RUN apt update && apt install -y git cmake gcc-arm-none-eabi python3 g++

RUN git clone https://github.com/raspberrypi/pico-sdk /opt/pico-sdk \
    && cd /opt/pico-sdk \
    && git submodule update --init
ENV PICO_SDK_PATH=/opt/pico-sdk