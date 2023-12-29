cmake_minimum_required(VERSION 3.1)
project(ModlApp)

IF(WIN32)
    set(LIBS winmm imm32 version)
ELSE(WIN32)
    set(LIBS X11 dl pthread GL c)
ENDIF(WIN32)

set(SOURCE_FILES main.cpp
                 ModlApp.h
                 ModlApp_wgl.cpp
                 ModlAppGL.cpp)

list(APPEND ALL_SOURCE_FILES ${SOURCE_FILES})
source_group("src" FILES ${SOURCE_FILES})

add_executable(ModlApp ${ALL_SOURCE_FILES})
target_link_libraries(ModlApp Eng ${LIBS})

add_custom_command(TARGET ModlApp
                   POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:ModlApp> ${WORKING_DIRECTORY})

set_target_properties(ModlApp PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${WORKING_DIRECTORY}")