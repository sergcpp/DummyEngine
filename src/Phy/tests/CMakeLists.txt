cmake_minimum_required(VERSION 3.2)
project(test_Phy)

add_executable(test_Phy main.cpp
                        test_mat.cpp
                        test_svol.cpp
                        test_vec.cpp)

target_link_libraries(test_Phy Phy)