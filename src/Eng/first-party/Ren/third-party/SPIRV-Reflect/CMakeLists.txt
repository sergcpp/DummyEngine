cmake_minimum_required(VERSION 3.5)
project(SPIRV-Reflect)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release;RelWithDebInfo;Asan" CACHE STRING "" FORCE)

IF(MSVC)
    if (NOT DEFINED CMAKE_C_FLAGS_ASAN OR CMAKE_C_FLAGS_ASAN STREQUAL "")
        set(CMAKE_C_FLAGS_ASAN "/MTd /Zi /Od /RTC1 /fsanitize=address")
        set(CMAKE_EXE_LINKER_FLAGS_ASAN "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /INCREMENTAL:NO")
    endif()
ELSE(MSVC)
ENDIF(MSVC)

set(SOURCE_FILES spirv_reflect.h
				 spirv_reflect.c)

add_library(SPIRV-Reflect STATIC ${SOURCE_FILES})

