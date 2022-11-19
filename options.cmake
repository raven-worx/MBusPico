# BASIC
option(MBUSPICO_DEVICE_KEY "Device encryption key (if required)" "")
set(MBUSPICO_LOG_LEVEL "LOG_ERROR" CACHE STRING "Log level (LOG_NONE, LOG_ERROR, LOG_INFO, LOG_DEBUG)")
set_property(CACHE MBUSPICO_LOG_LEVEL PROPERTY STRINGS "LOG_NONE" "LOG_ERROR" "LOG_INFO" "LOG_DEBUG")

# WIFI
option(MBUSPICO_WIFI_ENABLED "Connect to Wifi network" ON)
cmake_dependent_option(MBUSPICO_WIFI_SSID "Wifi network SSID" "" "MBUSPICO_WIFI_ENABLED" "")
cmake_dependent_option(MBUSPICO_WIFI_PWD "Wifi password" "" "MBUSPICO_WIFI_ENABLED" "")
cmake_dependent_option(MBUSPICO_WIFI_HOSTNAME "Network hostname" "MBusPico" "MBUSPICO_WIFI_ENABLED" "MBusPico")

# UDP SENDER
cmake_dependent_option(MBUSPICO_UDP_ENABLED "Enable UDP sender" ON "MBUSPICO_WIFI_ENABLED" OFF)
cmake_dependent_option(MBUSPICO_UDP_RECEIVER_HOST "UDP receiver host" "" "MBUSPICO_WIFI_ENABLED;MBUSPICO_UDP_ENABLED" "")
cmake_dependent_option(MBUSPICO_UDP_RECEIVER_PORT "UDP receiver port" 0 "MBUSPICO_WIFI_ENABLED;MBUSPICO_UDP_ENABLED" 0)
cmake_dependent_option(MBUSPICO_UDP_INTERVAL_S "UDP send interval [s]" 30 "MBUSPICO_WIFI_ENABLED;MBUSPICO_UDP_ENABLED" 30)

# HTTP SERVER
cmake_dependent_option(MBUSPICO_HTTP_ENABLED "Enable HTTP server" ON "MBUSPICO_WIFI_ENABLED" OFF)
cmake_dependent_option(MBUSPICO_HTTP_SERVER_PORT "HTTP server port" 80 "MBUSPICO_WIFI_ENABLED;MBUSPICO_HTTP_ENABLED" 80)
