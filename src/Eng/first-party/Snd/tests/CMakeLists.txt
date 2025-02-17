cmake_minimum_required(VERSION 3.5)
project(test_Snd)

set(SOURCE_FILES main.cpp
                 test_common.h
                 test_hashmap.cpp
                 test_span.cpp)

add_executable(test_Snd ${SOURCE_FILES})
target_link_libraries(test_Snd Snd)

set_target_properties(test_Snd PROPERTIES OUTPUT_NAME_DEBUG test_Snd-dbg)
set_target_properties(test_Snd PROPERTIES OUTPUT_NAME_RELWITHDEBINFO test_Snd-dev)
set_target_properties(test_Snd PROPERTIES OUTPUT_NAME_ASAN test_Snd-asan)
set_target_properties(test_Snd PROPERTIES OUTPUT_NAME_RELEASE test_Snd)