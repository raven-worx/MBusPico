#!/bin/bash -e

BASE_DIR=${PWD}
OPENOCD_DIR=${BASE_DIR}/openocd
OUT_DIR=${BASE_DIR}/out
LIBUS_DIR=${BASE_DIR}/libusb/libusb-MinGW-x64

#apt install -y libtool pkg-config mingw-w64
#git clone --recursive https://github.com/raspberrypi/openocd.git
#wget https://github.com/libusb/libusb/releases/download/v1.0.26/libusb-1.0.26-binaries.7z

export PKG_CONFIG_PATH=${LIBUS_DIR}/lib/pkgconfig

patch -u ${LIBUS_DIR}/lib/pkgconfig/libusb-1.0.pc <<EOF
@@ -1,4 +1,4 @@
-prefix=/home/appveyor/libusb-MinGW-x64
+prefix=${LIBUS_DIR}
 exec_prefix=\${prefix}
 libdir=\${exec_prefix}/lib
 includedir=\${prefix}/include
EOF

cd "${OPENOCD_DIR}"

./bootstrap

./configure \
	--disable-doxygen-html \
	--disable-doxygen-pdf \
	--enable-ftdi \
	--enable-jlink \
	--enable-ulink \
	--enable-picoprobe \
	--build="$(uname -m)-linux-gnu" \
	--host=x86_64-w64-mingw32 \
	--prefix="${OUT_DIR}" \
	LDFLAGS="-Wl,--no-as-needed -Wno-error" \
	CPPFLAGS="-I${LIBUS_DIR}/include/libusb-1.0 -D__USE_MINGW_ANSI_STDIO=1 -Wno-error" \
	CFLAGS="-Wno-error"

make -j$(nproc)
make install

cp "${LIBUS_DIR}/../VS2015-x64/dll/libusb-1.0.dll" "${OUT_DIR}/bin/"

# In Windows start with:
# <OUT_DIR>\bin>openocd.exe -s ../share/openocd/scripts -f interface/jlink.cfg -c "transport select swd" -c "adapter speed 4000" -f target/rp2040.cfg