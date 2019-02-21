cmake_minimum_required(VERSION 2.8)
project(SW)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    #set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ansi -Wall -Wno-switch -Wno-attributes")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DNDEBUG -O3 -ftree-vectorize -msse -msse2 -msse3 -mssse3 -fvariable-expansion-in-unroller -ftree-vectorizer-verbose=0 -ftree-loop-if-convert-stores -std=c99 -Wall -Wno-switch -Wno-attributes -ffast-math")
    #set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -pedantic-errors -Wall -Wno-switch -Wno-attributes -funroll-loops")
endif()

set(SOURCE_FILES    SW.h
                    SWtypes.h
                    SWcore.h
                    SWcore.c
                    SWcontext.h
                    SWcontext.c
                    SWculling.h
                    SWculling.c
                    SWprogram.h
                    SWprogram.c
                    SWrasterize.h
                    SWrasterize.c
                    SWframebuffer.h
                    SWframebuffer.c
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

list(APPEND ALL_SOURCE_FILES _SW.c)
source_group("src" FILES _SW.c)

add_library(SW STATIC ${ALL_SOURCE_FILES})

add_subdirectory(tests)