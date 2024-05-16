cmake_minimum_required(VERSION 3.1)
project(test_Eng)

if(NOT CMAKE_SYSTEM_NAME MATCHES "Android")
    if(WIN32)
        set(LIBS winmm opengl32)
    elseif(APPLE)
        
    else()
        
    endif()
else()
	
endif()

add_executable(test_Eng main.cpp
                        test_common.h
                        test_cmdline.cpp
                        test_materials.cpp
                        #test_object_pool.cpp
                        test_unicode.cpp
                        test_widgets.cpp)

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
                   COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_CURRENT_SOURCE_DIR}/../renderer/textures
                                                              ${CMAKE_CURRENT_SOURCE_DIR}/test_data/assets/textures/internal)
