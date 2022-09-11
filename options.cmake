option(UDP_ENABLED "Enable UDP sender" ON)
cmake_dependent_option(UDP_RECEIVER_HOST "UDP receiver host" "" "UDP_ENABLED" "")
cmake_dependent_option(UDP_RECEIVER_PORT "UDP receiver port" 0 "UDP_ENABLED" 0)

option(HTTP_ENABLED "Enable HTTP server" ON)
cmake_dependent_option(HTTP_SERVER_PORT "HTTP server port" 0 "HTTP_ENABLED" 0)

#option(MBUS_DEVICE "MBUS device" "kaifa_ma309m")
#if ("${MBUS_DEVICE}" STREQUAL "")
#	message( FATAL_ERROR "Value for MBUS_DEVICE option cannot be empty." )
#endif()
#if(NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/src/devices/")
#	message( FATAL_ERROR "Unsupported value for MBUS_DEVICE option: '${MBUS_DEVICE}'" )
#endif()