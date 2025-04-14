cmake_minimum_required(VERSION 3.10)
project(test_glslx)

add_executable(test_glslx main.cpp
                          test_common.h
                          test_fixup.cpp
                          test_hashmap.cpp
                          test_hlsl_writer.cpp
                          test_lexer.cpp
                          test_parser.cpp
                          test_pool_alloc.cpp
                          test_preprocessor.cpp
                          test_scope_exit.cpp
                          test_small_vector.cpp
                          test_span.cpp)

target_link_libraries(test_glslx glslx_lib ${LIBS})

set_target_properties(test_glslx PROPERTIES OUTPUT_NAME_DEBUG test_glslx-dbg)
set_target_properties(test_glslx PROPERTIES OUTPUT_NAME_RELWITHDEBINFO test_glslx-dev)
set_target_properties(test_glslx PROPERTIES OUTPUT_NAME_ASAN test_glslx-asan)
set_target_properties(test_glslx PROPERTIES OUTPUT_NAME_RELEASE test_glslx)

set_target_properties(test_glslx PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}")