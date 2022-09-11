cmake_minimum_required(VERSION 3.16)
include(CMakeDependentOption)

# initialize pico-sdk from GIT
# (note this can come from environment, CMake cache etc)
set(PICO_SDK_FETCH_FROM_GIT off)
set(PICO_SDK_FETCH_FROM_GIT_PATH ${CMAKE_CURRENT_LIST_DIR})

set(PICO_SDK_PATH ${CMAKE_CURRENT_LIST_DIR}/pico-sdk)

# options
option(UDP_ENABLED "Enable UDP sender" ON)
cmake_dependent_option(UDP_RECEIVER_HOST "UDP receiver host" "" "UDP_ENABLED" "")
cmake_dependent_option(UDP_RECEIVER_PORT "UDP receiver port" 0 "UDP_ENABLED" 0)

option(HTTP_ENABLED "Enable HTTP server" ON)

option(MBUS_DEVICE "MBUS device" "KAIFA_MA309M")
if ("${MBUS_DEVICE}" STREQUAL "KAIFA_MA309M")
	list(APPEND SOURCES src/devices/kaifa_ma309m.cpp)
else()
	message( FATAL_ERROR "Unsupported value for MBUS_DEVICE option: '${MBUS_DEVICE}'" )
endif()

# note: this must happen before project()
include(pico_sdk_import.cmake)

set(TARGETNAME "rpi-mbus_${MBUS_DEVICE}")
project(${TARGETNAME})

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

# create configuration header file
configure_file(include/conf.h.in conf.h @ONLY)
include_directories(
	include
	${CMAKE_CURRENT_BINARY_DIR}
)

add_executable(${TARGETNAME}
	src/main.cpp
	${SOURCES}
)

add_subdirectory(FreeRTOS)

# pull in common dependencies
target_link_libraries(${TARGETNAME} pico_stdlib freertos)

# enable usb output, disable uart output
pico_enable_stdio_usb(${TARGETNAME} 1)
pico_enable_stdio_uart(${TARGETNAME} 0)

# create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(${TARGETNAME})
