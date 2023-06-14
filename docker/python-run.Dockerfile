# Python runtime image to execute MBusPico

FROM python:3.10.12-slim

ENV DEBIAN_FRONTEND=noninteractive \
    PIP_DEFAULT_TIMEOUT=100 \
    PYTHONUNBUFFERED=1 \
    PIP_DISABLE_PIP_VERSION_CHECK=1 \
    PIP_NO_CACHE_DIR=1

RUN set -ex \
    && apt update && apt install -y python3-pycryptodome python3-serial \
    && apt autoremove -y \
    && apt clean -y \
    && rm -rf /var/lib/apt/lists/*

COPY python/ /opt/mbuspico/

WORKDIR /opt/mbuspico
ENTRYPOINT ["python3 /opt/mbuspico/main.py"]
