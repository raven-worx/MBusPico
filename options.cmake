# WIFI
option(WIFI_ENABLED "Connect to Wifi network" ON)
cmake_dependent_option(WIFI_SSID "Wifi network SSID" "" "WIFI_ENABLED" "")
cmake_dependent_option(WIFI_PWD "Wifi password" "" "WIFI_ENABLED" "")
cmake_dependent_option(WIFI_HOSTNAME "Network hostname" "RPiMbus" "WIFI_ENABLED" "")

# UDP SENDER
cmake_dependent_option(UDP_ENABLED "Enable UDP sender" ON "WIFI_ENABLED" OFF)
cmake_dependent_option(UDP_RECEIVER_HOST "UDP receiver host" "" "WIFI_ENABLED;UDP_ENABLED" "")
cmake_dependent_option(UDP_RECEIVER_PORT "UDP receiver port" 0 "WIFI_ENABLED;UDP_ENABLED" 0)

# HTTP SERVER
cmake_dependent_option(HTTP_ENABLED "Enable HTTP server" ON "WIFI_ENABLED" OFF)
cmake_dependent_option(HTTP_SERVER_PORT "HTTP server port" 80 "WIFI_ENABLED;HTTP_ENABLED" 80)
