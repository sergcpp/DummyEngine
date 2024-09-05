cmake_minimum_required(VERSION 3.1)
project(test_Sys)

find_library(CORE_GRAPHICS CoreGraphics)

IF(MSVC)
ELSE(MSVC)
    if(APPLE)
        set(LIBS ${CORE_GRAPHICS})
    else(APPLE)
        set(LIBS X11)
    endif(APPLE)
ENDIF(MSVC)

add_executable(test_Sys main.cpp
                        test_alloc.cpp
                        test_async_file.cpp
                        test_common.h
                        test_inplace_function.cpp
                        test_json.cpp
                        test_scope_exit.cpp
                        test_thread_pool.cpp
                        test_vector.cpp)

target_link_libraries(test_Sys Sys ${LIBS})

set_target_properties(test_Sys PROPERTIES OUTPUT_NAME_DEBUG test_Sys-dbg)
set_target_properties(test_Sys PROPERTIES OUTPUT_NAME_RELWITHDEBINFO test_Sys-dev)
set_target_properties(test_Sys PROPERTIES OUTPUT_NAME_ASAN test_Sys-asan)
set_target_properties(test_Sys PROPERTIES OUTPUT_NAME_RELEASE test_Sys)

set_target_properties(test_Sys PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}")