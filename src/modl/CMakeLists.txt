cmake_minimum_required(VERSION 2.8.4)
project(modl)

IF(WIN32)
    IF(SDL_STATIC)
        if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
            set(LIBS :libSDL2main.a :libSDL2.a :libSDL2_mixer.a winmm imm32 version)
		elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
            set(LIBS SDL2main.lib SDL2.lib winmm imm32 version)
        endif()
    ELSE(SDL_STATIC)
        if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
            set(LIBS opengl32 SDL2 SDL2_mixer)
        elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
            set(LIBS opengl32 SDL2 SDL2_mixer)
        endif()
    ENDIF(SDL_STATIC)
ELSE(WIN32)
    set(LIBS X11 SDL2 dl pthread GL glut c)
ENDIF(WIN32)

set(SOURCE_FILES main.cpp
                 ModlApp.h
                 ModlApp.cpp
                 ModlAppGL.cpp)

list(APPEND ALL_SOURCE_FILES ${SOURCE_FILES})
source_group("src" FILES ${SOURCE_FILES})

add_executable(modl ${ALL_SOURCE_FILES})
target_link_libraries(modl ${LIBS} Ren Sys)
