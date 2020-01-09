cmake_minimum_required(VERSION 3.2)
project(test_DummyLib)

set(SOURCE_FILES main.cpp
                 test_common.h)

add_executable(test_DummyLib ${SOURCE_FILES})
target_link_libraries(test_DummyLib ${LIBS} DummyLib)