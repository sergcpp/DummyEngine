cmake_minimum_required(VERSION 3.1)
project(Sys)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release;RelWithDebInfo;Asan" CACHE STRING "" FORCE)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_library(CORE_GRAPHICS CoreGraphics)

IF(MSVC)
    if(NOT "${CMAKE_GENERATOR_PLATFORM}" STREQUAL "ARM64")
        set(CMAKE_CXX_FLAGS_ASAN "/MTd /Zi /Od /RTC1 /fsanitize=address")
    else(NOT "${CMAKE_GENERATOR_PLATFORM}" STREQUAL "ARM64")
        set(CMAKE_CXX_FLAGS_ASAN "/MTd /Zi /Od /RTC1")
    endif(NOT "${CMAKE_GENERATOR_PLATFORM}" STREQUAL "ARM64")
    set(CMAKE_EXE_LINKER_FLAGS_ASAN "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /INCREMENTAL:NO")
ELSE(MSVC)
    if(APPLE)
        set(LIBS ${CORE_GRAPHICS})
    elseif(NOT CMAKE_SYSTEM_NAME MATCHES "Android")
        set(LIBS X11)
    endif(APPLE)
ENDIF(MSVC)

IF(WIN32)

ELSE(WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -fPIC")
    if(NOT CMAKE_SYSTEM_NAME MATCHES "Android")
        set(LIBS dl pthread)
    endif()
ENDIF(WIN32)

set(SOURCE_FILES AssetFile.h
                 AssetFile.cpp
                 AssetFileIO.h
                 AssetFileIO.cpp
                 AsyncFileReader.h
                 BinaryTree.h
                 BitmapAlloc.h
                 Delegate.h
                 DynLib.h
                 DynLib.cpp
                 InplaceFunction.h
                 Json.h
                 Json.cpp
                 MemBuf.h
                 MonoAlloc.h
                 Optional.h
                 Pack.h
                 Pack.cpp
                 PoolAlloc.h
                 ScopeExit.h
                 Signal_.h
                 SmallVector.h
                 SpinLock.h
                 ThreadPool.h
                 ThreadWorker.h
                 Time_.h
                 Time.cpp
                 Variant.h
                 WindowRect.h
                 WindowRect.cpp)

IF(WIN32)
set(SOURCE_FILES ${SOURCE_FILES}
                 AsyncFileReader_win32.cpp)
ELSE(WIN32)
IF(APPLE)
    set(SOURCE_FILES ${SOURCE_FILES}
                     AsyncFileReader_posix_aio.cpp)
ELSE(APPLE)
    set(SOURCE_FILES ${SOURCE_FILES}
                     AsyncFileReader_aio.cpp)
ENDIF(APPLE)
ENDIF(WIN32)

list(APPEND SYS_SOURCE_FILES ${SOURCE_FILES})
source_group("src" FILES ${SOURCE_FILES})

if(MSVC)
    list(APPEND SYS_SOURCE_FILES _CustomTypes.natvis)
endif(MSVC)

add_library(Sys STATIC ${SYS_SOURCE_FILES})
target_link_libraries(Sys ${LIBS})

add_subdirectory(tests)
