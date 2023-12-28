cmake_minimum_required(VERSION 3.5)
project(DummyLib)

set(SOURCE_FILES    Viewer.h
                    Viewer.cpp)

set(GUI_SOURCES Gui/CaptionsUI.h
                Gui/CaptionsUI.cpp
                Gui/DebugInfoUI.h
                Gui/DebugInfoUI.cpp
                Gui/DialogEditUI.h
                Gui/DialogEditUI.cpp
				Gui/DialogUI.h
				Gui/DialogUI.cpp
                Gui/FontStorage.h
                Gui/PagedReader.h
                Gui/PagedReader.cpp
                Gui/SeqCanvasUI.h
                Gui/SeqCanvasUI.cpp
				Gui/SeqEditUI.h
				Gui/SeqEditUI.cpp
                Gui/TimelineUI.h
                Gui/TimelineUI.cpp
                Gui/WordPuzzleUI.h
                Gui/WordPuzzleUI.cpp)

set(STATES_SOURCES  States/GSBaseState.h
                    States/GSBaseState.cpp
                    States/GSDrawTest.h
                    States/GSDrawTest.cpp
                    States/GSPhyTest.h
                    States/GSPhyTest.cpp
                    States/GSPlayTest.h
                    States/GSPlayTest.cpp
                    States/GSUITest.h
                    States/GSUITest.cpp
                    States/GSUITest2.h
                    States/GSUITest2.cpp
                    States/GSUITest3.h
                    States/GSUITest3.cpp
                    States/GSUITest4.h
                    States/GSUITest4.cpp
                    States/GSVideoTest.h
                    States/GSVideoTest.cpp)

set(UTILS_SOURCES   Utils/DialogController.h
                    Utils/DialogController.cpp
                    Utils/Dictionary.h
                    Utils/Dictionary.cpp)


if(${RENDERER} STREQUAL "GL")
    set(STATES_SOURCES ${STATES_SOURCES}
                        States/GSUITest3GL.cpp
                        States/GSVideoTestGL.cpp)
elseif(${RENDERER} STREQUAL "VK")
    set(STATES_SOURCES ${STATES_SOURCES}
                        States/GSUITest3VK.cpp
                        States/GSVideoTestVK.cpp)
elseif(${RENDERER} STREQUAL "SW")

endif()

list(APPEND ALL_SOURCE_FILES ${SOURCE_FILES})
set_source_files_properties(${SOURCE_FILES} PROPERTIES UNITY_GROUP "common")
source_group("src" FILES ${SOURCE_FILES})

list(APPEND ALL_SOURCE_FILES ${GUI_SOURCES})
set_source_files_properties(${GUI_SOURCES} PROPERTIES UNITY_GROUP "gui")
source_group("src\\Gui" FILES ${GUI_SOURCES})

list(APPEND ALL_SOURCE_FILES ${STATES_SOURCES})
set_source_files_properties(${STATES_SOURCES} PROPERTIES UNITY_GROUP "states")
source_group("src\\States" FILES ${STATES_SOURCES})

list(APPEND ALL_SOURCE_FILES ${UTILS_SOURCES})
set_source_files_properties(${UTILS_SOURCES} PROPERTIES UNITY_GROUP "utils")
source_group("src\\Utils" FILES ${UTILS_SOURCES})

add_library(DummyLib STATIC ${ALL_SOURCE_FILES})
target_link_libraries(DummyLib Eng Net Phy Ray Ren Snd Sys astc ${LIBS})

set_target_properties(DummyLib PROPERTIES UNITY_BUILD_MODE GROUP)

add_custom_command(TARGET DummyLib
                   POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_CURRENT_SOURCE_DIR}/../Eng/renderer/shaders
                                                              ${WORKING_DIRECTORY}/assets/shaders/internal)

add_subdirectory(tests)
