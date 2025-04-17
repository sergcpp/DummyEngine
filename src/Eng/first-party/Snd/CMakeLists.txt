cmake_minimum_required(VERSION 3.10)
project(Snd)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_definitions(-D_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR)

# Only one backend for now
set(SOUND_BACKEND AL)

find_library(COREAUDIO_LIB CoreAudio)
find_library(AUDIO_TOOLBOX AudioToolbox)
find_library(CORE_FOUNDATION CoreFoundation)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release;RelWithDebInfo;Asan" CACHE STRING "" FORCE)

IF(MSVC)
	if (NOT DEFINED CMAKE_CXX_FLAGS_ASAN OR CMAKE_CXX_FLAGS_ASAN STREQUAL "")
        set(CMAKE_CXX_FLAGS_ASAN "/MDd /Zi /Od /RTC1 /fsanitize=address /analyze /WX")
        set(CMAKE_EXE_LINKER_FLAGS_ASAN "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /INCREMENTAL:NO")
    endif()
ELSE(MSVC)
ENDIF(MSVC)

if(WIN32)
	if(${SOUND_BACKEND} STREQUAL "AL")
		add_definitions(-DAL_LIBTYPE_STATIC=1)
		if(CMAKE_SIZEOF_VOID_P EQUAL 8)
			set(OPENAL_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/third-party/OpenAL/include")
			if(CMAKE_GENERATOR_PLATFORM MATCHES "ARM64")
				set(OPENAL_LIBRARIES "${CMAKE_CURRENT_SOURCE_DIR}/third-party/OpenAL/libs/winarm64")
			else(CMAKE_GENERATOR_PLATFORM MATCHES "ARM64")
				set(OPENAL_LIBRARIES "${CMAKE_CURRENT_SOURCE_DIR}/third-party/OpenAL/libs/Win64")
			endif(CMAKE_GENERATOR_PLATFORM MATCHES "ARM64")
		else(CMAKE_SIZEOF_VOID_P EQUAL 8)
			set(OPENAL_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/third-party/OpenAL/include")
			set(OPENAL_LIBRARIES "${CMAKE_CURRENT_SOURCE_DIR}/third-party/OpenAL/libs/Win32")
		endif()
		if(MSVC)
			set(LIBS winmm $<$<CONFIG:Debug>:OpenAL32d>
			               $<$<CONFIG:Asan>:OpenAL32asan>
			               $<$<CONFIG:Release>:OpenAL32>
			               $<$<CONFIG:RelWithDebInfo>:OpenAL32>
			               $<$<CONFIG:Tsan>:OpenAL32>)
		else(MSVC)
			set(LIBS winmm debug OpenAL32d optimized OpenAL32)
		endif(MSVC)
	endif()
else(WIN32)
    if(${SOUND_BACKEND} STREQUAL "AL")
		set(OPENAL_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/third-party/OpenAL/include")
		if(NOT CMAKE_SYSTEM_NAME MATCHES "Android")
            if(APPLE)
                if(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "arm64")
                    set(OPENAL_LIBRARIES "${CMAKE_CURRENT_SOURCE_DIR}/third-party/OpenAL/libs/macos_arm64")
                else()
                    set(OPENAL_LIBRARIES "${CMAKE_CURRENT_SOURCE_DIR}/third-party/OpenAL/libs/macos")
                endif()
                set(LIBS openal ${COREAUDIO_LIB} ${AUDIO_TOOLBOX} ${CORE_FOUNDATION})
            else(APPLE)
                set(OPENAL_LIBRARIES "${CMAKE_CURRENT_SOURCE_DIR}/third-party/OpenAL/libs/linux")
                set(LIBS openal sndio)
            endif(APPLE)
		else()
			set(OPENAL_LIBRARIES "${CMAKE_CURRENT_SOURCE_DIR}/third-party/OpenAL/libs/arm64-v8a")
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
				 Span.h
				 SparseArray.h
				 Storage.h
				 String.h
				 Utils.h
				 Utils.cpp)

if(${SOUND_BACKEND} STREQUAL "AL")
    set(SOURCE_FILES ${SOURCE_FILES}
					 BufferAL.cpp
                     ContextAL.cpp
					 SourceAL.cpp)
endif()

list(APPEND SND_SOURCE_FILES ${SOURCE_FILES})
source_group("src" FILES ${SOURCE_FILES})

add_library(Snd STATIC ${SND_SOURCE_FILES})
target_compile_definitions(Snd PUBLIC USE_AL_SOUND)
target_link_libraries(Snd ${LIBS})
target_include_directories(Snd PRIVATE ${OPENAL_INCLUDE_DIRS})
if(NOT CMAKE_SYSTEM_NAME MATCHES "Android")
	target_link_directories(Snd PUBLIC ${OPENAL_LIBRARIES})
endif()

add_subdirectory(tests)
