cmake_minimum_required(VERSION 3.1)
project(test_Eng)

find_library(CORE_GRAPHICS CoreGraphics)

if(NOT CMAKE_SYSTEM_NAME MATCHES "Android")
    if(WIN32)
        set(LIBS winmm opengl32)
    elseif(APPLE)
        set(LIBS ${CORE_GRAPHICS})
    else()

    endif()
else()

endif()

add_executable(test_Eng main.cpp
                        test_common.h
                        test_cmdline.cpp
                        test_materials.cpp)

target_link_libraries(test_Eng ${LIBS} Eng)

set_target_properties(test_Eng PROPERTIES OUTPUT_NAME_DEBUG test_Eng-dbg)
set_target_properties(test_Eng PROPERTIES OUTPUT_NAME_RELWITHDEBINFO test_Eng-dev)
set_target_properties(test_Eng PROPERTIES OUTPUT_NAME_ASAN test_Eng-asan)
set_target_properties(test_Eng PROPERTIES OUTPUT_NAME_TSAN test_Eng-tsan)
set_target_properties(test_Eng PROPERTIES OUTPUT_NAME_RELEASE test_Eng)

set_target_properties(test_Eng PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/test_data")

add_custom_command(TARGET test_Eng
                   POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_CURRENT_SOURCE_DIR}/../renderer/shaders
                                                              ${CMAKE_CURRENT_SOURCE_DIR}/test_data/assets/shaders/internal)
add_custom_command(TARGET test_Eng
                   POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_CURRENT_SOURCE_DIR}/../textures
                                                              ${CMAKE_CURRENT_SOURCE_DIR}/test_data/assets/textures/internal)

if(${REN_BACKEND} STREQUAL "VK")
    add_custom_command(TARGET test_Eng
                    POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:spirv_compiler> ${CMAKE_CURRENT_LIST_DIR}/test_data)
endif()