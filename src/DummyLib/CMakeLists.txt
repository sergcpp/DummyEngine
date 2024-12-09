cmake_minimum_required(VERSION 3.5)
project(DummyLib)

set(SOURCE_FILES    Viewer.h
                    Viewer.cpp)

set(WIDGETS_SOURCES widgets/CaptionsUI.h
                    widgets/CaptionsUI.cpp
                    widgets/DialogEditUI.h
                    widgets/DialogEditUI.cpp
				    widgets/DialogUI.h
				    widgets/DialogUI.cpp
                    widgets/FontStorage.h
                    widgets/PagedReader.h
                    widgets/PagedReader.cpp
                    widgets/SeqCanvasUI.h
                    widgets/SeqCanvasUI.cpp
				    widgets/SeqEditUI.h
				    widgets/SeqEditUI.cpp
                    widgets/TimelineUI.h
                    widgets/TimelineUI.cpp
                    widgets/WordPuzzleUI.h
                    widgets/WordPuzzleUI.cpp)

set(STATES_SOURCES  states/BaseState.h
                    states/BaseState.cpp
                    states/DrawTest.h
                    states/DrawTest.cpp
                    states/PhyTest.h
                    states/PhyTest.cpp
                    states/PlayTest.h
                    states/PlayTest.cpp
                    states/UITest.h
                    states/UITest.cpp
                    states/UITest2.h
                    states/UITest2.cpp
                    states/UITest3.h
                    states/UITest3.cpp
                    states/UITest4.h
                    states/UITest4.cpp
                    #states/VideoTest.h
                    #states/VideoTest.cpp
    )

set(UTILS_SOURCES   utils/DialogController.h
                    utils/DialogController.cpp
                    utils/Dictionary.h
                    utils/Dictionary.cpp
                    utils/Log.h
                    utils/Log.cpp)


if(${REN_BACKEND} STREQUAL "GL")
    set(STATES_SOURCES ${STATES_SOURCES}
                        states/UITest3GL.cpp
                        #states/VideoTestGL.cpp
        )
elseif(${REN_BACKEND} STREQUAL "VK")
    set(STATES_SOURCES ${STATES_SOURCES}
                        states/UITest3VK.cpp
                        #states/VideoTestVK.cpp
        )
elseif(${REN_BACKEND} STREQUAL "SW")

endif()

list(APPEND ALL_SOURCE_FILES ${SOURCE_FILES})
set_source_files_properties(${SOURCE_FILES} PROPERTIES UNITY_GROUP "common")
source_group("src" FILES ${SOURCE_FILES})

list(APPEND ALL_SOURCE_FILES ${WIDGETS_SOURCES})
set_source_files_properties(${WIDGETS_SOURCES} PROPERTIES UNITY_GROUP "widgets")
source_group("src\\widgets" FILES ${WIDGETS_SOURCES})

list(APPEND ALL_SOURCE_FILES ${STATES_SOURCES})
set_source_files_properties(${STATES_SOURCES} PROPERTIES UNITY_GROUP "states")
source_group("src\\states" FILES ${STATES_SOURCES})

list(APPEND ALL_SOURCE_FILES ${UTILS_SOURCES})
set_source_files_properties(${UTILS_SOURCES} PROPERTIES UNITY_GROUP "utils")
source_group("src\\utils" FILES ${UTILS_SOURCES})

add_library(DummyLib STATIC ${ALL_SOURCE_FILES})
target_link_libraries(DummyLib Eng Ray ${LIBS})

set_target_properties(DummyLib PROPERTIES UNITY_BUILD_MODE GROUP)

add_custom_command(TARGET DummyLib
                   POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_CURRENT_SOURCE_DIR}/../Eng/renderer/shaders
                                                              ${WORKING_DIRECTORY}/assets/shaders/internal)
add_custom_command(TARGET DummyLib
                   POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_CURRENT_SOURCE_DIR}/../Eng/textures
                                                              ${WORKING_DIRECTORY}/assets/textures/internal)

