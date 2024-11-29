cmake_minimum_required(VERSION 3.1)
project(test_Phy)

add_executable(test_Phy main.cpp
                        test_mat.cpp
                        test_small_vector.cpp
                        test_svol.cpp
                        test_vec.cpp)

target_link_libraries(test_Phy Phy)

set_target_properties(test_Phy PROPERTIES OUTPUT_NAME_DEBUG test_Phy-dbg)
set_target_properties(test_Phy PROPERTIES OUTPUT_NAME_RELWITHDEBINFO test_Phy-dev)
set_target_properties(test_Phy PROPERTIES OUTPUT_NAME_ASAN test_Phy-asan)
set_target_properties(test_Phy PROPERTIES OUTPUT_NAME_RELEASE test_Phy)