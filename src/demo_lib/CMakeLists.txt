cmake_minimum_required(VERSION 2.8)
project(demo_lib)

add_definitions(-DBUILDING_DLL)

set(SOURCE_FILES    exports.cpp
					Viewer.h
                    Viewer.cpp)

set(STATES_SOURCES  states/GSCreate.h
					states/GSCreate.cpp
					states/GSBicubicTest.h
					states/GSBicubicTest.cpp
					states/GSOccTest.h
					states/GSOccTest.cpp
					states/GSOccTestGL.cpp)

set(UI_SOURCES  ui/FontStorage.h)

if(${RENDERER} STREQUAL "GL")
    set(RENDERER_SOURCES ${RENDERER_SOURCES}
                         renderer/RendererGL.cpp)
elseif(${RENDERER} STREQUAL "SW")
    set(RENDERER_SOURCES ${RENDERER_SOURCES}
                         renderer/RendererSW.cpp)
endif()

list(APPEND ALL_SOURCE_FILES ${SOURCE_FILES})
#set_source_files_properties(${SOURCE_FILES} PROPERTIES HEADER_FILE_ONLY TRUE)
source_group("src" FILES ${SOURCE_FILES})

list(APPEND ALL_SOURCE_FILES ${STATES_SOURCES})
set_source_files_properties(${STATES_SOURCES} PROPERTIES HEADER_FILE_ONLY TRUE)
source_group("src\\states" FILES ${STATES_SOURCES})

list(APPEND ALL_SOURCE_FILES ${UI_SOURCES})
set_source_files_properties(${UI_SOURCES} PROPERTIES HEADER_FILE_ONLY TRUE)
source_group("src\\ui" FILES ${UI_SOURCES})

list(APPEND ALL_SOURCE_FILES states/_states.cpp)
source_group("src\\states" FILES states/_states.cpp)

add_library(demo_lib SHARED ${ALL_SOURCE_FILES})
target_link_libraries(demo_lib engine ren sys ${LIBS})

# dot not prefix dll with 'lib' in MinGW
set_target_properties(demo_lib PROPERTIES PREFIX "")

add_custom_command(TARGET demo_lib
                   POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:demo_lib> ${WORKING_DIRECTORY})
