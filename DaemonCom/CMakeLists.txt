cmake_minimum_required (VERSION 2.6)
project (DaemonCom)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake/")

add_executable(mmcom mmcom.c)

find_package(LibWebSockets REQUIRED)
if (LIBWEBSOCKETS_FOUND)
 include_directories(${LIBWEBSOCKETS_INCLUDE_DIR})
 target_link_libraries(mmcom ${LIBWEBSOCKETS_LIBRARIES})
endif (LIBWEBSOCKETS_FOUND)


