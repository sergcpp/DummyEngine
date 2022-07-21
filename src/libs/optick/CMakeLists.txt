cmake_minimum_required(VERSION 3.1)
project(optick)

set(SOURCE_FILES _optick.cpp)

set(ORIG_SOURCE_FILES optick.config.h
                      optick.h
											optick_capi.cpp
											optick_capi.h
											optick_common.h
											optick_core.cpp
											optick_core.freebsd.h
											optick_core.h
											optick_core.linux.h
											optick_core.macos.h
											optick_core.platform.h
											optick_core.win.h
											optick_gpu.cpp
											optick_gpu.d3d12.cpp
											optick_gpu.h
											optick_gpu.vulkan.cpp
											optick_memory.h
											optick_message.cpp
											optick_message.h
											optick_miniz.cpp
											optick_miniz.h
											optick_serialization.cpp
											optick_serialization.h
											optick_server.cpp
											optick_server.h)

list(APPEND ALL_SOURCE_FILES ${ORIG_SOURCE_FILES})
set_source_files_properties(${ORIG_SOURCE_FILES} PROPERTIES HEADER_FILE_ONLY TRUE)

list(APPEND ALL_SOURCE_FILES ${SOURCE_FILES})
list(APPEND ALL_SOURCE_FILES ${ORIG_SOURCE_FILES})

add_library(optick STATIC ${ALL_SOURCE_FILES})

