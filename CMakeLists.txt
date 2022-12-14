cmake_minimum_required(VERSION 3.16)
include(CMakeDependentOption)

# initialize pico-sdk from GIT
# (note this can come from environment, CMake cache etc)
#set(PICO_SDK_FETCH_FROM_GIT OFF)
#set(PICO_SDK_FETCH_FROM_GIT_PATH ${CMAKE_CURRENT_LIST_DIR})

set(FREERTOS_KERNEL_PATH ${CMAKE_CURRENT_LIST_DIR}/FreeRTOS/FreeRTOS-Kernel)

set(PICO_BOARD "pico_w")

# options
include(options.cmake)

# note: this must happen before project()
include(pico_sdk_import.cmake)

set(TARGETNAME "mbuspico")
project(MBusPico VERSION 1.4)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

if (NOT PICO_CYW43_SUPPORTED OR NOT TARGET pico_cyw43_arch) # set by PICO_BOARD=pico_w
	message(FATAL_ERROR "CYW43 driver not available")
endif()

# create configuration header file
configure_file(include/mbuspico.conf.h.in mbuspico.conf.h @ONLY)

include_directories(
	${CMAKE_CURRENT_BINARY_DIR}
	include
	FreeRTOS
)

add_executable(${TARGETNAME}
	src/main.c
	src/wifi.c
	src/uart.c
	src/mbuspico.c
	src/http.c
	src/udp.c
	src/devices/kaifa_ma309m_EVN.c
)

include(FreeRTOS/FreeRTOS_Kernel_import.cmake)
add_subdirectory(dep)

target_compile_definitions(${TARGETNAME} 
PUBLIC
	PROJECT_NAME="${PROJECT_NAME}"
	PROJECT_VERSION="${PROJECT_VERSION}"
	PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS=5000
	CYW43_HOST_NAME="${MBUSPICO_WIFI_HOSTNAME}"
PRIVATE
	$<$<CONFIG:DEBUG>:_DEBUG=1>
	$<$<STREQUAL:${CMAKE_HOST_SYSTEM_NAME},Windows>:_BUILD_HOST_WINDOWS=1>
)

# pull in common dependencies
target_link_libraries(${TARGETNAME}
	pico_cyw43_arch_lwip_sys_freertos
	pico_stdlib
	FreeRTOS-Kernel-Heap4 # FreeRTOS kernel and dynamic heap
	mbedtls_crypto
	mongoose
)

# enable usb output, disable uart output
pico_enable_stdio_usb(${TARGETNAME} 1)
pico_enable_stdio_uart(${TARGETNAME} 0)

# create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(${TARGETNAME})
