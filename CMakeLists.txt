cmake_minimum_required (VERSION 2.6)

project (SmartReflect C)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake/")

# Both DaemonCom and PluginDaemon require this package
find_package(LibWebSockets REQUIRED)


# Build DaemonCom first

aux_source_directory(DaemonCom DAEMONCOM_SRC)
add_executable(mmcom ${DAEMONCOM_SRC})
target_compile_options(mmcom PRIVATE "-O1")

if (LIBWEBSOCKETS_FOUND)
	include_directories(${LIBWEBSOCKETS_INCLUDE_DIR})
	target_link_libraries(mmcom ${LIBWEBSOCKETS_LIBRARIES})
else ()
	message(FATAL_ERROR "Missing libwebsockets")
endif ()


# Build PluginDaemon

set(SYSTEM_LIBS rt m)
set(CMAKE_C_FLAGS "-std=gnu99")

aux_source_directory(PluginDaemon/source SR_SOURCES)
aux_source_directory(PluginDaemon/source/plugins SR_PLUGIN_SOURCES)
aux_source_directory(PluginDaemon/source/api SR_API_SOURCES)

include_directories(PluginDaemon/includes PluginDaemon/includes/api /usr/local/lib)
add_executable(smartreflect ${SR_SOURCES} ${SR_PLUGIN_SOURCES} ${SR_API_SOURCES})

target_compile_options(smartreflect PRIVATE "-O3")

find_package(LibUUID REQUIRED)
if (LIBUUID_FOUND)
	include_directories(${LIBUUID_INCLUDE_DIRS})
	target_link_libraries(smartreflect ${SYSTEM_LIBS} ${LIBWEBSOCKETS_LIBRARIES} ${LIBUUID_LIBRARIES})
else ()
	message(FATAL_ERROR "Missing uuid-dev")
endif ()


