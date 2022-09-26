cmake_minimum_required(VERSION 3.16)
include(CMakeDependentOption)

# initialize pico-sdk from GIT
# (note this can come from environment, CMake cache etc)
set(PICO_SDK_FETCH_FROM_GIT off)
set(PICO_SDK_FETCH_FROM_GIT_PATH ${CMAKE_CURRENT_LIST_DIR})

set(PICO_SDK_PATH ${CMAKE_CURRENT_LIST_DIR}/pico-sdk)
set(FREERTOS_KERNEL_PATH ${CMAKE_CURRENT_LIST_DIR}/FreeRTOS/FreeRTOS-Kernel)

set(PICO_BOARD "pico_w")

# options
include(options.cmake)

# note: this must happen before project()
include(pico_sdk_import.cmake)

set(TARGETNAME "rpi-mbus")
project(${TARGETNAME})

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

if (NOT PICO_CYW43_SUPPORTED OR NOT TARGET pico_cyw43_arch) # set by PICO_BOARD=pico_w
	message(FATAL_ERROR "CYW43 driver not available")
endif()

# create configuration header file
configure_file(include/mbuspi.conf.h.in mbuspi.conf.h @ONLY)

include_directories(
	${CMAKE_CURRENT_BINARY_DIR}
	include
	FreeRTOS
	src/httpparser
)

add_executable(${TARGETNAME}
	src/main.c
	src/wifi.c
	src/uart.c
	src/mbuspi.c
	src/http.c
	src/udp.c
	src/devices/kaifa_ma309m.c
)

include(FreeRTOS/FreeRTOS_Kernel_import.cmake)

target_compile_definitions(${TARGETNAME} PUBLIC
	PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS=5000
)

# pull in common dependencies
target_link_libraries(${TARGETNAME}
	pico_cyw43_arch_lwip_sys_freertos
	pico_stdlib
	FreeRTOS-Kernel-Heap4 # FreeRTOS kernel and dynamic heap
)

# enable usb output, disable uart output
pico_enable_stdio_usb(${TARGETNAME} 1)
pico_enable_stdio_uart(${TARGETNAME} 0)

# create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(${TARGETNAME})
