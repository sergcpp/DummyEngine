cmake_minimum_required(VERSION 3.10)
project(DummyApp)

if(${REN_BACKEND} STREQUAL "GL")
  set(BIN_NAME "DummyAppGL")
elseif(${REN_BACKEND} STREQUAL "VK")
  set(BIN_NAME "DummyAppVK")
elseif(${REN_BACKEND} STREQUAL "SW")
  set(BIN_NAME "DummyAppSW")
endif()

find_library(CORE_GRAPHICS CoreGraphics)

if(NOT CMAKE_SYSTEM_NAME MATCHES "Android")
    if(WIN32)
        set(LIBS winmm imm32 version)
    elseif(APPLE)
        set(LIBS ${CORE_GRAPHICS})
    else()
        set(LIBS X11 ittnotify)
    endif()
else()
	set(LIBS android log)
endif()

set(SOURCE_FILES    DummyApp.h
                    DummyApp.cpp)

if(WIN32)
    if(${REN_BACKEND} STREQUAL "GL")
        list(APPEND SOURCE_FILES DummyApp_wgl.cpp)
    elseif(${REN_BACKEND} STREQUAL "VK")
        list(APPEND SOURCE_FILES DummyApp_vk_win32.cpp)
    elseif(${REN_BACKEND} STREQUAL "SW")

    endif()
elseif(APPLE)
    list(APPEND SOURCE_FILES DummyApp_vk_osx.cpp)
else()
    if(${REN_BACKEND} STREQUAL "GL")
        list(APPEND SOURCE_FILES DummyApp_glx.cpp)
    elseif(${REN_BACKEND} STREQUAL "VK")
        list(APPEND SOURCE_FILES DummyApp_vk_xlib.cpp)
    elseif(${REN_BACKEND} STREQUAL "SW")

    endif()
endif()

if(NOT CMAKE_SYSTEM_NAME MATCHES "Android")
	list(APPEND SOURCE_FILES main_pc.cpp)
else()
    list(APPEND SOURCE_FILES main_android.cpp)
endif()

list(APPEND ALL_SOURCE_FILES ${SOURCE_FILES})
source_group("src" FILES ${SOURCE_FILES})

if(NOT CMAKE_SYSTEM_NAME MATCHES "Android")
	add_executable(DummyApp ${ALL_SOURCE_FILES} DummyApp.rc)
else()
	add_library(DummyApp SHARED ${ALL_SOURCE_FILES})
endif()

target_link_libraries(DummyApp DummyLib ${LIBS})

set_target_properties(DummyApp PROPERTIES OUTPUT_NAME_ASAN ${BIN_NAME}-asan)
set_target_properties(DummyApp PROPERTIES OUTPUT_NAME_DEBUG ${BIN_NAME}-dbg)
set_target_properties(DummyApp PROPERTIES OUTPUT_NAME_RELWITHDEBINFO ${BIN_NAME}-dev)
set_target_properties(DummyApp PROPERTIES OUTPUT_NAME_RELEASE ${BIN_NAME})

add_custom_command(TARGET DummyApp
                   POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:DummyApp> ${WORKING_DIRECTORY})

if(${REN_BACKEND} STREQUAL "VK")
    add_custom_command(TARGET DummyApp
                       POST_BUILD
                       COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:spirv_compiler> ${WORKING_DIRECTORY})
endif()

set_target_properties(DummyApp PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${WORKING_DIRECTORY}")
