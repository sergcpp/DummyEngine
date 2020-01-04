cmake_minimum_required(VERSION 2.8.4)
project(Eng)

add_definitions(-DSHINY_IS_COMPILED=FALSE)

IF(WIN32)

ELSE(WIN32)
    set(CMAKE_CXX_STANDARD  11)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
ENDIF(WIN32)

set(SOURCE_FILES Config.h
                 FlowControl.h
                 FlowControl.cpp
                 FrameInfo.h
                 GameBase.h
                 GameBase.cpp
                 GameState.h
                 GameStateManager.h
                 GameStateManager.cpp
                 ObjectPool.h
                 Random.h
                 Random.cpp)

set(INPUT_SOURCE_FILES Input/InputManager.h
                       Input/InputManager.cpp
                       Input/Keycode.h)

set(GO_SOURCE_FILES go/Go.h
                    go/Go.cpp
                    go/GoAlloc.h
                    go/GoComponent.h
                    go/GoID.h)

list(APPEND ALL_SOURCE_FILES ${SOURCE_FILES})
set_source_files_properties(${SOURCE_FILES} PROPERTIES HEADER_FILE_ONLY TRUE)
source_group("src" FILES ${SOURCE_FILES})

list(APPEND ALL_SOURCE_FILES ${GO_SOURCE_FILES})
set_source_files_properties(${GO_SOURCE_FILES} PROPERTIES HEADER_FILE_ONLY TRUE)
source_group("src\\go" FILES ${GO_SOURCE_FILES})

list(APPEND ALL_SOURCE_FILES ${INPUT_SOURCE_FILES})
set_source_files_properties(${INPUT_SOURCE_FILES} PROPERTIES HEADER_FILE_ONLY TRUE)
source_group("src\\Input" FILES ${INPUT_SOURCE_FILES})

list(APPEND ALL_SOURCE_FILES _Eng.cpp)
source_group("src" FILES _Eng.cpp)

add_library(Eng STATIC ${ALL_SOURCE_FILES})
target_link_libraries(Eng Gui Ren Sys)

add_subdirectory(tests)