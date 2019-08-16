cmake_minimum_required(VERSION 3.1)
project(test_Sys)

add_executable(test_Sys main.cpp
                        test_alloc.cpp
                        test_async_file.cpp
                        test_common.h
                        test_json.cpp
                        test_optional.cpp
                        test_pack.cpp
                        test_signal.cpp
                        test_vector.cpp)

target_link_libraries(test_Sys Sys)