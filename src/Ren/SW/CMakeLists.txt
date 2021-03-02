cmake_minimum_required(VERSION 3.1)
project(SW)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    #set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ansi -Wall -Wno-switch -Wno-attributes")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DNDEBUG -O3 -ftree-vectorize -msse -msse2 -msse3 -mssse3 -mavx -mavx2 -mfma -mavx512f -mavx512bw -mavx512dq -fvariable-expansion-in-unroller -ftree-vectorizer-verbose=0 -ftree-loop-if-convert-stores -std=c99 -Wall -Wno-switch -Wno-attributes -ffast-math")
    #set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -pedantic-errors -Wall -Wno-switch -Wno-attributes -funroll-loops")
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

list(APPEND ALL_SOURCE_FILES ${SOURCE_FILES})
set_source_files_properties(${SOURCE_FILES} PROPERTIES HEADER_FILE_ONLY 1)
source_group("src" FILES ${SOURCE_FILES})

list(APPEND ALL_SOURCE_FILES _SW.c _SW_SSE2.c _SW_AVX2.c _SW_AVX512.c)
source_group("src" FILES _SW.c _SW_SSE2.c _SW_AVX2.c _SW_AVX512.c)

if(MSVC)
    if(NOT CMAKE_CL_64)
        #set_source_files_properties(_Ren_Sse2.cpp PROPERTIES COMPILE_FLAGS /arch:SSE2)
    endif()
    set_source_files_properties(_SW_AVX.cpp PROPERTIES COMPILE_FLAGS /arch:AVX)
endif(MSVC)

add_library(SW STATIC ${ALL_SOURCE_FILES})

add_subdirectory(tests)