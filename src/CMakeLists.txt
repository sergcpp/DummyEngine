cmake_minimum_required(VERSION 3.1)
project(RayDemo)

set(CMAKE_CXX_STANDARD 11)

set(SDL_STATIC ON)

if(NOT CMAKE_SYSTEM_NAME MATCHES "Android")
    if(WIN32)
        if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
            if(CMAKE_SIZEOF_VOID_P EQUAL 8)
                set(SDL2_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/libs/SDL2/x86_64-w64-mingw32/include)
                set(SDL2_LIBRARIES "${CMAKE_CURRENT_SOURCE_DIR}/libs/SDL2/x86_64-w64-mingw32/lib")
            else(CMAKE_SIZEOF_VOID_P EQUAL 8)
                set(SDL2_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/libs/SDL2/i686-w64-mingw32/include)
                set(SDL2_LIBRARIES "${CMAKE_CURRENT_SOURCE_DIR}/libs/SDL2/i686-w64-mingw32/lib")
            endif()
        elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
            if(CMAKE_SIZEOF_VOID_P EQUAL 8)
                set(SDL2_LIBRARIES "${CMAKE_CURRENT_SOURCE_DIR}/libs/SDL2/lib/x64")
            else(CMAKE_SIZEOF_VOID_P EQUAL 8)
                set(SDL2_LIBRARIES "${CMAKE_CURRENT_SOURCE_DIR}/libs/SDL2/lib/x86")
            endif()
            set(SDL2_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/libs/SDL2/include)
        endif()
    else(WIN32)
        include(FindPkgConfig)
        pkg_search_module (SDL2 REQUIRED sdl2)

        if(APPLE)
            set(SDL2_INCLUDE_DIRS /usr/local/include)
            set(SDL2_LIBRARIES /usr/local/lib)
        endif(APPLE)
    endif(WIN32)
    
    include_directories(${SDL2_INCLUDE_DIRS})
    link_directories(${SDL2_LIBRARIES})
endif()

set(RENDERER GL)

if(${RENDERER} STREQUAL "GL")
  add_definitions(-DUSE_GL_RENDER)
elseif(${RENDERER} STREQUAL "SW")
  add_definitions(-DUSE_SW_RENDER)
elseif(${RENDERER} STREQUAL "VK")
  add_definitions(-DUSE_VK_RENDER)
endif()

include_directories(.)
include_directories(libs/dirent)

#set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..)
#set(WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..)
enable_testing()

add_subdirectory(Eng)
add_subdirectory(DemoApp)
add_subdirectory(DemoLib)
add_subdirectory(Modl)
add_subdirectory(Ray)
add_subdirectory(Ren)
add_subdirectory(Sys)
add_subdirectory(Gui)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set_target_properties(DemoApp
                      DemoLib
                      Modl PROPERTIES FOLDER App)

set_target_properties(Eng
                      Ray
                      Ren
                      SW
                      Sys 
                      Gui PROPERTIES FOLDER Libs)

set_target_properties(test_Eng
                      test_Ray
                      test_Ren
                      test_SW
                      test_Sys
                      test_Gui PROPERTIES FOLDER Tests)
                      
set_target_properties(SOIL2 PROPERTIES FOLDER _)

add_test(EngTest        Eng/tests/test_Eng)
add_test(RayTest        Ray/tests/test_Ray)
add_test(RenTest        Ren/tests/test_Ren)
add_test(SWTest         Ren/SW/tests/test_SW)
add_test(SysTest        Sys/tests/test_Sys)
add_test(GuiTest        Gui/tests/test_Gui)

add_custom_target(Check COMMAND ${CMAKE_CTEST_COMMAND} DEPENDS test_Eng
                                                               test_Ray
                                                               test_Ren
                                                               test_SW
                                                               test_Sys
                                                               test_Gui)
set_target_properties(Check PROPERTIES FOLDER Tests)
