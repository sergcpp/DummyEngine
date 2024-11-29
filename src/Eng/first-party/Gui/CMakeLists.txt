cmake_minimum_required(VERSION 3.1)
project(Gui)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release;RelWithDebInfo;Asan" CACHE STRING "" FORCE)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_library(CORE_GRAPHICS CoreGraphics)

IF(MSVC)
    if (NOT DEFINED CMAKE_CXX_FLAGS_ASAN OR CMAKE_CXX_FLAGS_ASAN STREQUAL "")
        if(NOT "${CMAKE_GENERATOR_PLATFORM}" STREQUAL "ARM64")
            set(CMAKE_CXX_FLAGS_ASAN "/MTd /Zi /Od /RTC1 /fsanitize=address")
        else(NOT "${CMAKE_GENERATOR_PLATFORM}" STREQUAL "ARM64")
            set(CMAKE_CXX_FLAGS_ASAN "/MTd /Zi /Od /RTC1")
        endif(NOT "${CMAKE_GENERATOR_PLATFORM}" STREQUAL "ARM64")
        set(CMAKE_EXE_LINKER_FLAGS_ASAN "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /INCREMENTAL:NO")
    endif()
ELSE(MSVC)
    if(APPLE)
        set(LIBS ${CORE_GRAPHICS})
    elseif(NOT CMAKE_SYSTEM_NAME MATCHES "Android")
        set(LIBS X11)
    endif(APPLE)
ENDIF(MSVC)

IF(WIN32)

ELSE(WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -fPIC")
    if(NOT CMAKE_SYSTEM_NAME MATCHES "Android")
        set(LIBS dl pthread)
    endif()
ENDIF(WIN32)

set(SOURCE_FILES    AlignedAlloc.h
                    BaseElement.h
                    BaseElement.cpp
                    BitmapFont.h
                    BitmapFont.cpp
                    Bitmask.h
                    ButtonBase.h
                    ButtonBase.cpp
                    ButtonImage.h
                    ButtonImage.cpp
                    Delegate.h
                    EditBox.h
                    EditBox.cpp
                    Gui.h
                    Gui.cpp
                    Image.h
                    Image.cpp
                    Image9Patch.h
                    Image9Patch.cpp
                    Input.h
                    Keycode.h
                    LinearLayout.h
                    LinearLayout.cpp
                    MVec.h
                    Renderer.h
                    Renderer.cpp
                    shaders.inl
                    Signal.h
                    SmallVector.h
                    Span.h
                    Utils.h
                    Utils.cpp)

if(${REN_BACKEND} STREQUAL "GL")
    set(SOURCE_FILES    ${SOURCE_FILES}
                        RendererGL.cpp)
elseif(${REN_BACKEND} STREQUAL "SW")
    set(SOURCE_FILES    ${SOURCE_FILES}
                        RendererSW.cpp)
elseif(${REN_BACKEND} STREQUAL "VK")
    set(SOURCE_FILES    ${SOURCE_FILES}
                        RendererVK.cpp)
endif()

list(APPEND GUI_SOURCE_FILES ${SOURCE_FILES})
source_group("src" FILES ${SOURCE_FILES})

if(MSVC)
    list(APPEND GUI_SOURCE_FILES _CustomTypes.natvis)
endif(MSVC)

add_library(Gui STATIC ${GUI_SOURCE_FILES})
target_link_libraries(Gui Ren Sys ${LIBS})

add_subdirectory(tests)
