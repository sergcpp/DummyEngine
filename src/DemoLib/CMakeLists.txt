cmake_minimum_required(VERSION 2.8)
project(DemoLib)

add_definitions(-DBUILDING_DLL)

set(SOURCE_FILES    exports.cpp
                    Viewer.h
                    Viewer.cpp)

set(MANAGER_SOURCES  Managers/Renderer.h
                     Managers/Renderer.cpp
					 Managers/SceneData.h
                     Managers/SceneManager.h
                     Managers/SceneManager.cpp)

set(STATES_SOURCES  States/GSCreate.h
                    States/GSCreate.cpp
                    States/GSBicubicTest.h
                    States/GSBicubicTest.cpp
                    States/GSDefTest.h
                    States/GSDefTest.cpp
					States/GSDrawTest.h
					States/GSDrawTest.cpp
                    States/GSIKTest.h
                    States/GSIKTest.cpp
                    States/GSOccTest.h
                    States/GSOccTest.cpp)

set(UI_SOURCES  ui/FontStorage.h)

if(${RENDERER} STREQUAL "GL")
    set(MANAGER_SOURCES ${MANAGER_SOURCES}
                        Managers/RendererGL.cpp)
    set(STATES_SOURCES ${STATES_SOURCES}
                       States/GSDefTestGL.cpp
                       States/GSOccTestGL.cpp)
elseif(${RENDERER} STREQUAL "SW")
    set(MANAGER_SOURCES ${MANAGER_SOURCES}
                        Renderer/RendererSW.cpp)
endif()

list(APPEND ALL_SOURCE_FILES ${SOURCE_FILES})
#set_source_files_properties(${SOURCE_FILES} PROPERTIES HEADER_FILE_ONLY TRUE)
source_group("src" FILES ${SOURCE_FILES})

list(APPEND ALL_SOURCE_FILES ${MANAGER_SOURCES})
#set_source_files_properties(${MANAGER_SOURCES} PROPERTIES HEADER_FILE_ONLY TRUE)
source_group("src\\Managers" FILES ${MANAGER_SOURCES})

list(APPEND ALL_SOURCE_FILES ${STATES_SOURCES})
#set_source_files_properties(${STATES_SOURCES} PROPERTIES HEADER_FILE_ONLY TRUE)
source_group("src\\States" FILES ${STATES_SOURCES})

list(APPEND ALL_SOURCE_FILES ${UI_SOURCES})
#set_source_files_properties(${UI_SOURCES} PROPERTIES HEADER_FILE_ONLY TRUE)
source_group("src\\ui" FILES ${UI_SOURCES})

add_library(DemoLib SHARED ${ALL_SOURCE_FILES})
target_link_libraries(DemoLib Eng Ren Sys ${LIBS})

# dot not prefix dll with 'lib' in MinGW
set_target_properties(DemoLib PROPERTIES PREFIX "")

add_custom_command(TARGET DemoLib
                   POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:DemoLib> ${WORKING_DIRECTORY})
