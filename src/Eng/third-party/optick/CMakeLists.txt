cmake_minimum_required(VERSION 3.10)
project(optick)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release;RelWithDebInfo;Asan" CACHE STRING "" FORCE)

IF(MSVC)
    if (NOT DEFINED CMAKE_CXX_FLAGS_ASAN OR CMAKE_CXX_FLAGS_ASAN STREQUAL "")
        set(CMAKE_CXX_FLAGS_ASAN "/MTd /Zi /Od /RTC1 /fsanitize=address")
        set(CMAKE_EXE_LINKER_FLAGS_ASAN "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /INCREMENTAL:NO")
    endif()
ELSE(MSVC)
ENDIF(MSVC)

set(SOURCE_FILES    optick.config.h
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
                    #optick_gpu.d3d12.cpp
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

add_library(optick STATIC ${SOURCE_FILES})
