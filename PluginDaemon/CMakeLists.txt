cmake_minimum_required (VERSION 2.6)

project (SmartReflect)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake/")

set(SYSTEM_LIBS rt m)
set(CMAKE_C_FLAGS "-std=gnu99")

aux_source_directory(source SR_SOURCES)
aux_source_directory(source/plugins SR_PLUGIN_SOURCES)
aux_source_directory(source/api SR_API_SOURCES)

include_directories(includes includes/api /usr/local/lib)

add_executable(smartreflect ${SR_SOURCES} ${SR_PLUGIN_SOURCES} ${SR_API_SOURCES})

find_package(LibUUID REQUIRED)
find_package(LibWebSockets REQUIRED)

if (LIBWEBSOCKETS_FOUND AND LIBUUID_FOUND)
  include_directories(${LIBWEBSOCKETS_INCLUDE_DIR})
  include_directories(${LIBUUID_INCLUDE_DIRS})
  
 target_link_libraries(smartreflect ${SYSTEM_LIBS} ${LIBWEBSOCKETS_LIBRARIES} ${LIBUUID_LIBRARIES})
endif ()


