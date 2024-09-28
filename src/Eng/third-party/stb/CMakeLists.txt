cmake_minimum_required(VERSION 3.1)
project(stb)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release;RelWithDebInfo;Asan" CACHE STRING "" FORCE)

IF(MSVC)
    if (NOT DEFINED CMAKE_C_FLAGS_ASAN OR CMAKE_C_FLAGS_ASAN STREQUAL "")
        set(CMAKE_C_FLAGS_ASAN "/MTd /Zi /Od /RTC1 /fsanitize=address")
        set(CMAKE_EXE_LINKER_FLAGS_ASAN "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /INCREMENTAL:NO")
    endif()
ELSE(MSVC)
ENDIF(MSVC)

set(SOURCE_FILES stb_dxt.h
				 stb_image.h
				 stb_image_resize.h
				 stb_image_write.h
				 stb.c)

source_group("src" FILES ${SOURCE_FILES})

add_library(stb STATIC ${SOURCE_FILES})

