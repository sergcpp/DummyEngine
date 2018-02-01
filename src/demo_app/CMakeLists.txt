cmake_minimum_required(VERSION 2.8)
project(demo_app)

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
    #set(LIBS X11 :libSDL2.a dl pthread GL glut c)
    set(LIBS X11 SDL2 dl pthread GL glut c)
ENDIF(WIN32)

set(SOURCE_FILES    main.cpp
                    DemoApp.h
                    DemoApp.cpp)

list(APPEND ALL_SOURCE_FILES ${SOURCE_FILES})
#set_source_files_properties(${SOURCE_FILES} PROPERTIES HEADER_FILE_ONLY TRUE)
source_group("src" FILES ${SOURCE_FILES})

add_executable(demo_app ${ALL_SOURCE_FILES})
target_link_libraries(demo_app engine ${LIBS})

add_dependencies(demo_app demo_lib sys)

add_custom_command(TARGET demo_app 
                   POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:demo_app> ${WORKING_DIRECTORY})