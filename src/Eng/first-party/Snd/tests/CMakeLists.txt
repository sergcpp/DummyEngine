cmake_minimum_required(VERSION 3.1)
project(test_Snd)

set(SOURCE_FILES main.cpp
                 test_common.h
                 test_hashmap.cpp)

add_executable(test_Snd ${SOURCE_FILES})
target_link_libraries(test_Snd Snd)
