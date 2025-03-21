cmake_minimum_required(VERSION 3.10)
project(SW)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release;RelWithDebInfo;Asan" CACHE STRING "" FORCE)

IF(MSVC)
    if (NOT DEFINED CMAKE_C_FLAGS_ASAN OR CMAKE_C_FLAGS_ASAN STREQUAL "")
        set(CMAKE_C_FLAGS_ASAN "/MTd /Zi /Od /RTC1 /fsanitize=address")
        set(CMAKE_EXE_LINKER_FLAGS_ASAN "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /INCREMENTAL:NO")
    endif()
ELSE(MSVC)
ENDIF(MSVC)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DNDEBUG -O3 -ftree-vectorize -msse4.1 -fvariable-expansion-in-unroller -ftree-vectorizer-verbose=0 -ftree-loop-if-convert-stores -std=c99 -Wall -Wno-switch -Wno-attributes -Wno-unused-function -Wno-unknown-pragmas -ffast-math")
endif()

set(SOURCE_FILES    SW.h
                    SWalloc.h
                    SWtypes.h
                    SWcore.h
                    SWcore.c
                    SWcontext.h
                    SWcontext.c
                    SWculling.h
                    SWculling.c
                    SWculling_Ref.c
                    SWculling_AVX2.c
                    SWculling_AVX512.c
                    SWculling_NEON.c
                    SWculling_SSE2.c
                    SWculling_rast.inl
                    SWculling_rast_scanline.inl
                    SWculling_rast_tri.inl
                    SWprogram.h
                    SWprogram.c
                    SWrasterize.h
                    SWrasterize.c
                    SWframebuffer.h
                    SWframebuffer.c
                    SWintrin.inl
                    SWdraw.h
                    SWdraw.c
                    SWtexture.h
                    SWtexture.c
                    SWbuffer.h
                    SWbuffer.c
                    SWzbuffer.h
                    SWzbuffer.c
                    SWpixels.h
                    SWcpu.h
                    SWcpu.c
                    SWcompress.h
                    SWcompress.c)

list(APPEND SW_SOURCE_FILES ${SOURCE_FILES})
set_source_files_properties(${SOURCE_FILES} PROPERTIES HEADER_FILE_ONLY 1)
source_group("src" FILES ${SOURCE_FILES})

list(APPEND SW_SOURCE_FILES _SW.c _SW_SSE2.c _SW_AVX2.c _SW_AVX512.c _SW_NEON.c)
source_group("src" FILES _SW.c _SW_SSE2.c _SW_AVX2.c _SW_AVX512.c _SW_NEON.c)

STRING (REGEX REPLACE "/RTC(su|[1su])" "" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")

if(MSVC)
    if(NOT CMAKE_CL_64)
        set_source_files_properties(_SW_SSE2.c PROPERTIES COMPILE_FLAGS /arch:SSE2)
    endif()
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set_source_files_properties(_SW.c PROPERTIES COMPILE_FLAGS -msse4.1)
        set_source_files_properties(_SW_SSE2.c PROPERTIES COMPILE_FLAGS -msse4.1)
    endif()
    set_source_files_properties(_SW_AVX2.c PROPERTIES COMPILE_FLAGS /arch:AVX2)
    set_source_files_properties(_SW_AVX512.c PROPERTIES COMPILE_FLAGS /arch:AVX512)
else(MSVC)
    set_source_files_properties(_SW_SSE2.c PROPERTIES COMPILE_FLAGS "-msse2")
    set_source_files_properties(_SW_AVX2.c PROPERTIES COMPILE_FLAGS "-mavx2 -mfma")
    set_source_files_properties(_SW_AVX512.c PROPERTIES COMPILE_FLAGS "-mavx512f -mavx512bw -mavx512dq")
endif(MSVC)

add_library(SW STATIC ${SW_SOURCE_FILES})
set_target_properties(SW PROPERTIES UNITY_BUILD OFF)

add_subdirectory(tests)
