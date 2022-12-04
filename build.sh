#!/bin/bash -e

#../dep/mbedtls/scripts/config.py --write ../include/mbedtls_config.h crypto_baremetal

SOURCE_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null && pwd)

BUILD_DIR=$1
if [ -z "$BUILD_DIR" ]; then
  BUILD_DIR="$SOURCE_DIR/build"
fi

if [ ! -e "$BUILD_DIR" ]; then
mkdir -p "$BUILD_DIR"
fi

cd "$BUILD_DIR"
cmake "$SOURCE_DIR" -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel $(nproc)