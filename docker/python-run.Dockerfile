# Python runtime image to execute MBusPico

FROM --platform=$TARGETPLATFORM python:3.10.12-slim

ENV DEBIAN_FRONTEND=noninteractive \
    PIP_DEFAULT_TIMEOUT=100 \
    PYTHONUNBUFFERED=1 \
    PIP_DISABLE_PIP_VERSION_CHECK=1 \
    PIP_NO_CACHE_DIR=1

RUN pip3 install pyserial pycryptodomex

COPY python/ /opt/mbuspico/

WORKDIR /opt/mbuspico
ENTRYPOINT ["python3 /opt/mbuspico/main.py"]
