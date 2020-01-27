cmake_minimum_required(VERSION 2.8.4)
project(Net)

set(CMAKE_CXX_STANDARD  11)

set(SOURCE_FILES    Address.h
                    Address.cpp
                    IConnection.h
                    Socket.h
                    Socket.cpp
                    UDPConnection.h
                    UDPConnection.cpp
                    PacketQueue.h
                    ReliabilitySystem.h
                    ReliabilitySystem.cpp
                    ReliableUDPConnection.h
                    ReliableUDPConnection.cpp
                    Var.h
                    VarContainer.h
                    VarContainer.cpp
                    HTTPBase.h
                    HTTPRequest.h
                    HTTPResponse.h
                    HTTPBase.cpp
                    HTTPRequest.cpp
                    HTTPResponse.cpp
                    BitMsg.h
                    BitMsg.cpp
                    Types.h GLMTypes.h
                    Compress.h
                    Compress.cpp
                    NAT_PMP.h
                    NAT_PMP.cpp
                    NAT_PCP.h
                    NAT_PCP.cpp
                    WsConnection.h
                    WsConnection.cpp)

set(HASH_SOURCE_FILES   hash/base64.h
                        hash/base64.cpp
                        hash/Crc32.h
                        hash/Crc32.cpp
                        hash/md5.h
                        hash/md5.cpp
                        hash/sha1.h
                        hash/sha1.cpp)

set(LZO_SOURCE_FILES    minilzo/lzoconf.h
                        minilzo/lzodefs.h
                        minilzo/minilzo.h
                        minilzo/minilzo.c)

list(APPEND ALL_SOURCE_FILES ${SOURCE_FILES})
set_source_files_properties(${SOURCE_FILES} PROPERTIES HEADER_FILE_ONLY TRUE)
source_group("src" FILES ${SOURCE_FILES})

list(APPEND ALL_SOURCE_FILES ${HASH_SOURCE_FILES})
set_source_files_properties(${HASH_SOURCE_FILES} PROPERTIES HEADER_FILE_ONLY TRUE)
source_group("src\\hash" FILES ${HASH_SOURCE_FILES})

list(APPEND ALL_SOURCE_FILES ${LZO_SOURCE_FILES})
set_source_files_properties(${LZO_SOURCE_FILES} PROPERTIES HEADER_FILE_ONLY TRUE)
source_group("src\\minilzo" FILES ${LZO_SOURCE_FILES})

list(APPEND ALL_SOURCE_FILES _Net.cpp)
source_group("src" FILES _Net.cpp)

add_library(Net STATIC ${ALL_SOURCE_FILES})
if(WIN32)
    target_link_libraries(Net ws2_32)
else()
endif()

add_subdirectory(tests)
