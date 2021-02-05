cmake_minimum_required(VERSION 2.8.4)
project(Phy)

OPTION(ENABLE_UNITYBUILD "Enable compilation of one large cpp file" ON)

IF(WIN32)

ELSE(WIN32)
    set(CMAKE_CXX_STANDARD  11)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -fPIC")
    if(NOT CMAKE_SYSTEM_NAME MATCHES "Android")
        set(LIBS dl pthread)
    endif()
ENDIF(WIN32)

set(SOURCE_FILES    Body.h
                    Body.cpp
                    Bounds.h
                    Core.h
                    MMat.h
                    MQuat.h
                    MVec.h
                    Shape.h
                    Shape.cpp
                    Utils.h
                    Utils.cpp)

list(APPEND ALL_SOURCE_FILES ${SOURCE_FILES})

# enable or disable unity build
if(ENABLE_UNITYBUILD)
    set_source_files_properties(${SOURCE_FILES} PROPERTIES HEADER_FILE_ONLY TRUE)
    source_group("src" FILES ${SOURCE_FILES})

    list(APPEND ALL_SOURCE_FILES _Phy.cpp)
    source_group("src" FILES _Phy.cpp)
endif()

add_library(Phy STATIC ${ALL_SOURCE_FILES})
target_link_libraries(Phy ${LIBS})

add_subdirectory(tests)