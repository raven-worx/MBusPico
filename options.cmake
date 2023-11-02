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
cmake_dependent_option(MBUSPICO_UDP_INTERVAL_S "UDP send interval [s]" 10 "MBUSPICO_WIFI_ENABLED;MBUSPICO_UDP_ENABLED" 10)

# HTTP SERVER
cmake_dependent_option(MBUSPICO_HTTP_ENABLED "Enable HTTP server" ON "MBUSPICO_WIFI_ENABLED" OFF)
cmake_dependent_option(MBUSPICO_HTTP_SERVER_PORT "HTTP server port" 80 "MBUSPICO_WIFI_ENABLED;MBUSPICO_HTTP_ENABLED" 80)
cmake_dependent_option(MBUSPICO_HTTP_AUTH_USER "HTTP Authentication user" "" "MBUSPICO_WIFI_ENABLED;MBUSPICO_HTTP_ENABLED" "")
cmake_dependent_option(MBUSPICO_HTTP_AUTH_PWD "HTTP Authentication password" "" "MBUSPICO_WIFI_ENABLED;MBUSPICO_HTTP_ENABLED" "")

#
# INIT OPTIONS FROM FILE
#
set(OPTIONS_FILE ${CMAKE_CURRENT_LIST_DIR}/options.ini)
if (EXISTS ${OPTIONS_FILE})
	message("=== Reading options from file '${OPTIONS_FILE}'")
	file(READ "${OPTIONS_FILE}" content)
	string(REPLACE "\\\n" "" content ${content})
	string(REPLACE "\n" ";" lines ${content})
	list(REMOVE_ITEM lines "")
	foreach(line ${lines})
		string(REPLACE "=" ";" line_split ${line})
		list(LENGTH line_split count)
		if (count LESS 2)
			continue()
		endif()
		list(GET line_split -1 var_value)
		if (var_value STREQUAL "")
			continue()
		endif()
		string(STRIP ${var_value} var_value)
		separate_arguments(var_value)
		list(REMOVE_AT line_split -1)
		foreach(var_name ${line_split})
			string(STRIP ${var_name} var_name)
			string(FIND ${var_name} "PICO_" idx)
			if (idx GREATER_EQUAL 0)
				set(${var_name} ${var_value})
				message(STATUS "  OPTION: ${var_name}\t   ${var_value}")
			endif()
		endforeach()
	endforeach()
endif()