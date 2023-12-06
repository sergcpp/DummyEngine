cmake_minimum_required(VERSION 3.1)
project(Phy)

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

add_library(Phy STATIC ${ALL_SOURCE_FILES})
target_link_libraries(Phy ${LIBS})

add_subdirectory(tests)