#!/bin/bash -e

#
# Script to build MicroPython for Raspberry Pico W
# with AES-CTR mode support enabled
#
# For example using Docker:
#    docker run -it --rm --entrypoint /bin/bash -v ${PWD}:/source ravenworx/pico-sdk-build:1.5.0
#

# download MicroPython source code and extract it (into a subdirectory) in the current working directory
wget https://micropython.org/resources/source/micropython-1.20.0.tar.xz -O /tmp/mp.tar.xz
tar -xvf /tmp/mp.tar.xz

cd micropython*/ports/rp2

# patch mbedtls config to enable AES-CTR methods
patch -p0 <<EOF
--- mbedtls/mbedtls_config.h
+++ mbedtls/mbedtls_config.h
@@ -29,6 +29,7 @@
 // Set mbedtls configuration
 #define MBEDTLS_ECP_NIST_OPTIM
 #define MBEDTLS_KEY_EXCHANGE_ECDHE_RSA_ENABLED
+#define MBEDTLS_CIPHER_MODE_CTR // needed for MICROPY_PY_UCRYPTOLIB_CTR

 // Enable mbedtls modules
 #define MBEDTLS_GCM_C
EOF

# create build-directory and start building
mkdir build-PICO_W
cd build-PICO_W
CFLAGS='-DMICROPY_PY_UCRYPTOLIB_CTR=1' cmake -DMICROPY_BOARD=PICO_W ..
make