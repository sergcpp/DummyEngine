cmake_minimum_required(VERSION 3.1)
project(test_Eng)

if(NOT CMAKE_SYSTEM_NAME MATCHES "Android")
    if(WIN32)
        set(LIBS winmm)
    elseif(APPLE)
        
    else()
        
    endif()
else()
	
endif()

add_executable(test_Eng main.cpp
                        test_common.h
                        test_cmdline.cpp
                        #test_object_pool.cpp
                        test_unicode.cpp
                        test_widgets.cpp
                        )

target_link_libraries(test_Eng ${LIBS} Eng)