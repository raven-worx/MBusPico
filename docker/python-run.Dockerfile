# Python runtime image to execute MBusPico

FROM debian:12-slim

ENV DEBIAN_FRONTEND=noninteractive \
    PIP_DEFAULT_TIMEOUT=100 \
    PYTHONUNBUFFERED=1 \
    PIP_DISABLE_PIP_VERSION_CHECK=1 \
    PIP_NO_CACHE_DIR=1

RUN set -ex \
    && apt update && apt install -y python3-pycryptodome python3-serial python3-paho-mqtt \
    && apt autoremove -y \
    && apt clean -y \
    && rm -rf /var/lib/apt/lists/*

COPY python/dist /opt/mbuspico/

# labels used by Home Assistant, as the image is also used for the Home Assistant add-on
ARG BUILD_VERSION=dev
LABEL io.hass.name="MBusPico" \
      io.hass.description="Reads smart meter data via M-Bus and provides it via MQTT, HTTP and UDP" \
      io.hass.type="addon" \
      io.hass.version="${BUILD_VERSION}"

WORKDIR /opt/mbuspico
ENTRYPOINT ["python3", "/opt/mbuspico/main.py"]
