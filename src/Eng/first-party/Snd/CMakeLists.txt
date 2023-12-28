cmake_minimum_required(VERSION 3.1)
project(Snd)

find_library(COREAUDIO_LIB CoreAudio)
find_library(AUDIO_TOOLBOX AudioToolbox)
find_library(CORE_FOUNDATION CoreFoundation)

if(WIN32)
	if(${SOUND} STREQUAL "AL")
		add_definitions(-DAL_LIBTYPE_STATIC=1)
		if(CMAKE_SIZEOF_VOID_P EQUAL 8)
			set(OPENAL_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/OpenAL/include")
			set(OPENAL_LIBRARIES "${CMAKE_CURRENT_SOURCE_DIR}/OpenAL/libs/Win64")
		else(CMAKE_SIZEOF_VOID_P EQUAL 8)
			set(OPENAL_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/OpenAL/include")
			set(OPENAL_LIBRARIES "${CMAKE_CURRENT_SOURCE_DIR}/OpenAL/libs/Win32")
		endif()
		set(LIBS debug OpenAL32d optimized OpenAL32)
	endif()
else(WIN32)
    if(${SOUND} STREQUAL "AL")
		set(OPENAL_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/OpenAL/include")
		if(NOT CMAKE_SYSTEM_NAME MATCHES "Android")
            if(APPLE)
                if(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "arm64")
                    set(OPENAL_LIBRARIES "${CMAKE_CURRENT_SOURCE_DIR}/OpenAL/libs/macos_arm64")
                else()
                    set(OPENAL_LIBRARIES "${CMAKE_CURRENT_SOURCE_DIR}/OpenAL/libs/macos")
                endif()
                set(LIBS openal ${COREAUDIO_LIB} ${AUDIO_TOOLBOX} ${CORE_FOUNDATION})
            else(APPLE)
                set(OPENAL_LIBRARIES "${CMAKE_CURRENT_SOURCE_DIR}/OpenAL/libs/linux")
                set(LIBS openal sndio)
            endif(APPLE)
		else()
			set(OPENAL_LIBRARIES "${CMAKE_CURRENT_SOURCE_DIR}/OpenAL/libs/arm64-v8a")
			set(LIBS openal)
		endif()
	endif()
endif(WIN32)

set(SOURCE_FILES Buffer.h
				 Context.h
                 Context.cpp
				 HashMap32.h
                 Log.h
				 Source.h
				 SparseArray.h
				 Storage.h
				 String.h
				 Utils.h
				 Utils.cpp)

if(${SOUND} STREQUAL "AL")
	add_definitions(-DUSE_AL_SOUND)
    set(SOURCE_FILES ${SOURCE_FILES}
					 BufferAL.cpp
                     ContextAL.cpp
					 SourceAL.cpp)
endif()

list(APPEND SND_SOURCE_FILES ${SOURCE_FILES})
source_group("src" FILES ${SOURCE_FILES})

add_library(Snd STATIC ${SND_SOURCE_FILES})
target_link_libraries(Snd ${LIBS})
target_include_directories(Snd PRIVATE ${OPENAL_INCLUDE_DIRS})
if(NOT CMAKE_SYSTEM_NAME MATCHES "Android")
	target_link_directories(Snd PUBLIC ${OPENAL_LIBRARIES})
endif()

add_subdirectory(tests)
