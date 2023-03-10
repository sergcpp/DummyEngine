cmake_minimum_required(VERSION 3.1)
project(test_Net)

IF(WIN32)
ELSE(WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
    set(LIBS pthread)
ENDIF(WIN32)

set(SRC_FILES main.cpp test_address.cpp
                       test_bitmsg.cpp
                       test_common.h
                       test_compress.cpp
                       test_hton.cpp
                       test_http.cpp
                       test_packet_queue.cpp
                       test_pcp.cpp
                       test_pmp.cpp
                       test_reliable_udp_connection.cpp
                       test_tcp_socket.cpp
                       test_types.cpp
                       test_udp_connection.cpp
                       test_udp_socket.cpp
                       test_var.cpp)

add_executable(test_Net ${SRC_FILES})

set_target_properties(test_Net PROPERTIES OUTPUT_NAME_DEBUG test_Net-dbg)
set_target_properties(test_Net PROPERTIES OUTPUT_NAME_RELWITHDEBINFO test_Net-dev)
set_target_properties(test_Net PROPERTIES OUTPUT_NAME_ASAN test_Net-asan)
set_target_properties(test_Net PROPERTIES OUTPUT_NAME_RELEASE test_Net)

target_link_libraries(test_Net ${LIBS} Net)
