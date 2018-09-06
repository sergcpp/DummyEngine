cmake_minimum_required(VERSION 2.8)
project(DemoLib)

add_definitions(-DBUILDING_DLL)

set(SOURCE_FILES    exports.cpp
                    Viewer.h
                    Viewer.cpp)

set(GUI_SOURCES  Gui/FontStorage.h)

set(SCENE_SOURCES  Scene/FrameBuf.h
					 Scene/Renderer.h
                     Scene/Renderer.cpp
					 Scene/SceneData.h
                     Scene/SceneManager.h
                     Scene/SceneManager.cpp
					 Scene/SceneManager_BVH.cpp)

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

set(UTILS_SOURCES Utils/BVHSplit.h
				  Utils/BVHSplit.cpp)

if(${RENDERER} STREQUAL "GL")
    set(SCENE_SOURCES ${SCENE_SOURCES}
						Scene/FrameBufGL.cpp
                        Scene/RendererGL.cpp)
    set(STATES_SOURCES ${STATES_SOURCES}
                       States/GSDefTestGL.cpp
                       States/GSOccTestGL.cpp)
elseif(${RENDERER} STREQUAL "SW")
    set(SCENE_SOURCES ${SCENE_SOURCES}
                        Renderer/RendererSW.cpp)
endif()

list(APPEND ALL_SOURCE_FILES ${SOURCE_FILES})
#set_source_files_properties(${SOURCE_FILES} PROPERTIES HEADER_FILE_ONLY TRUE)
source_group("src" FILES ${SOURCE_FILES})

list(APPEND ALL_SOURCE_FILES ${GUI_SOURCES})
#set_source_files_properties(${GUI_SOURCES} PROPERTIES HEADER_FILE_ONLY TRUE)
source_group("src\\Gui" FILES ${GUI_SOURCES})

list(APPEND ALL_SOURCE_FILES ${SCENE_SOURCES})
#set_source_files_properties(${SCENE_SOURCES} PROPERTIES HEADER_FILE_ONLY TRUE)
source_group("src\\Managers" FILES ${SCENE_SOURCES})

list(APPEND ALL_SOURCE_FILES ${STATES_SOURCES})
#set_source_files_properties(${STATES_SOURCES} PROPERTIES HEADER_FILE_ONLY TRUE)
source_group("src\\States" FILES ${STATES_SOURCES})

list(APPEND ALL_SOURCE_FILES ${UTILS_SOURCES})
#set_source_files_properties(${UTILS_SOURCES} PROPERTIES HEADER_FILE_ONLY TRUE)
source_group("src\\Utils" FILES ${UTILS_SOURCES})

add_library(DemoLib SHARED ${ALL_SOURCE_FILES})
target_link_libraries(DemoLib Eng Ren Sys ${LIBS})

# dot not prefix dll with 'lib' in MinGW
set_target_properties(DemoLib PROPERTIES PREFIX "")

add_custom_command(TARGET DemoLib
                   POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:DemoLib> ${WORKING_DIRECTORY})
