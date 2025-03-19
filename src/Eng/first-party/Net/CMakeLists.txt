cmake_minimum_required(VERSION 3.10)
project(Net)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_definitions(-D_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release;RelWithDebInfo;Asan" CACHE STRING "" FORCE)

IF(MSVC)
    if (NOT DEFINED CMAKE_CXX_FLAGS_ASAN OR CMAKE_CXX_FLAGS_ASAN STREQUAL "")
        set(CMAKE_CXX_FLAGS_ASAN "/MTd /Zi /Od /RTC1 /fsanitize=address")
        set(CMAKE_EXE_LINKER_FLAGS_ASAN "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /INCREMENTAL:NO")
    endif()
ELSE(MSVC)
ENDIF(MSVC)

set(SOURCE_FILES    Address.h
                    Address.cpp
                    BitMsg.h
                    BitMsg.cpp
                    Compress.h
                    Compress.cpp
                    GLMTypes.h
                    HTTPBase.h
                    HTTPBase.cpp
                    HTTPRequest.h
                    HTTPRequest.cpp
                    HTTPResponse.h
                    HTTPResponse.cpp
                    IConnection.h
                    InterprocessLock.h
                    NAT_PCP.h
                    NAT_PCP.cpp
                    NAT_PMP.h
                    NAT_PMP.cpp
                    Net.h
                    Net.cpp
                    PacketQueue.h
                    ReliabilitySystem.h
                    ReliabilitySystem.cpp
                    ReliableUDPConnection.h
                    ReliableUDPConnection.cpp
                    Socket.h
                    Socket.cpp
                    Types.h
                    UDPConnection.h
                    UDPConnection.cpp
                    Var.h
                    VarContainer.h
                    VarContainer.cpp
                    WsConnection.h
                    WsConnection.cpp)

set(HASH_SOURCE_FILES   hash/base64.h
                        hash/base64.cpp
                        hash/Crc32.h
                        hash/Crc32.cpp
                        hash/md5.h
                        hash/md5.cpp
                        hash/murmur.h
                        hash/murmur.cpp
                        hash/sha1.h
                        hash/sha1.cpp)

set(LZO_SOURCE_FILES    minilzo/lzoconf.h
                        minilzo/lzodefs.h
                        minilzo/minilzo.h
                        minilzo/minilzo.c)

list(APPEND NET_SOURCE_FILES ${SOURCE_FILES})
source_group("src" FILES ${SOURCE_FILES})

list(APPEND NET_SOURCE_FILES ${HASH_SOURCE_FILES})
source_group("src\\hash" FILES ${HASH_SOURCE_FILES})

list(APPEND NET_SOURCE_FILES ${LZO_SOURCE_FILES})
source_group("src\\minilzo" FILES ${LZO_SOURCE_FILES})

add_library(Net STATIC ${NET_SOURCE_FILES})
if(WIN32)
    target_link_libraries(Net ws2_32)
else()
endif()

add_subdirectory(tests)
