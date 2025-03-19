cmake_minimum_required(VERSION 3.10)
project(test_Gui)

find_library(CORE_GRAPHICS CoreGraphics)

IF(MSVC)
ELSE(MSVC)
    if(APPLE)
        set(LIBS ${CORE_GRAPHICS})
    else(APPLE)
        set(LIBS X11)
    endif(APPLE)
ENDIF(MSVC)

add_executable(test_Gui main.cpp
                        test_common.h
                        test_signal.cpp
                        test_small_vector.cpp
                        test_span.cpp
                        test_unicode.cpp
                        test_widgets.cpp)

target_link_libraries(test_Gui Gui ${LIBS})

set_target_properties(test_Gui PROPERTIES OUTPUT_NAME_DEBUG test_Gui-dbg)
set_target_properties(test_Gui PROPERTIES OUTPUT_NAME_RELWITHDEBINFO test_Gui-dev)
set_target_properties(test_Gui PROPERTIES OUTPUT_NAME_ASAN test_Gui-asan)
set_target_properties(test_Gui PROPERTIES OUTPUT_NAME_RELEASE test_Gui)

set_target_properties(test_Gui PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}")