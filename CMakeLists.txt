cmake_minimum_required(VERSION 2.8.4)
project(Sys)

OPTION(ENABLE_UNITYBUILD "Enable compilation of one large cpp file" ON)

IF(WIN32)

ELSE(WIN32)
    set(CMAKE_CXX_STANDARD  11)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -fPIC")
    if(NOT CMAKE_SYSTEM_NAME MATCHES "Android")
        set(LIBS dl pthread)
    endif()
ENDIF(WIN32)

set(SOURCE_FILES AssetFile.h
                 AssetFile.cpp
                 AssetFileIO.h
                 AssetFileIO.cpp
                 AsyncFileReader.h
                 AsyncFileReader.cpp
                 BinaryTree.h
                 Delegate.h
                 DynLib.h
                 DynLib.cpp
                 Json.h
                 Json.cpp
                 Log.h
                 MemBuf.h
                 MonoAlloc.h
                 Optional.h
                 Pack.h
                 Pack.cpp
                 Platform.h
                 PlatformSDL.h
                 PlatformSDL.cpp
                 PoolAlloc.h
                 Signal_.h
                 SpinLock.h
                 ThreadPool.h
                 ThreadWorker.h
                 Time_.h
                 Time.cpp)

list(APPEND ALL_SOURCE_FILES ${SOURCE_FILES})

# enable or disable unity build
if(ENABLE_UNITYBUILD)
    set_source_files_properties(${SOURCE_FILES} PROPERTIES HEADER_FILE_ONLY TRUE)
    source_group("src" FILES ${SOURCE_FILES})

    list(APPEND ALL_SOURCE_FILES _Sys.cpp)
    source_group("src" FILES _Sys.cpp)
endif()

add_library(Sys STATIC ${ALL_SOURCE_FILES})
target_link_libraries(Sys ${LIBS})

add_subdirectory(tests)