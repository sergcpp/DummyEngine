cmake_minimum_required(VERSION 2.8.4)
project(RayDemo)

set(CMAKE_CXX_STANDARD 11)

set(SDL_STATIC ON)

include_directories(libs/ocl/include)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    link_directories(libs/ocl/lib/x86_64)
else(CMAKE_SIZEOF_VOID_P EQUAL 8)
    link_directories(libs/ocl/lib/x86)
endif()

add_definitions(-DCPU_ARCH_SSE2)
#add_definitions(-DMATH_NO_INLINE)

set(RENDERER GL)

if(${RENDERER} STREQUAL "GL")
  add_definitions(-DUSE_GL_RENDER)
elseif(${RENDERER} STREQUAL "SW")
  add_definitions(-DUSE_SW_RENDER)
elseif(${RENDERER} STREQUAL "VK")
  add_definitions(-DUSE_VK_RENDER)
endif()

if(WIN32)
    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
		if(CMAKE_SIZEOF_VOID_P EQUAL 8)
			set(SDL2_INCLUDE_DIRS libs/SDL2/x86_64-w64-mingw32/include)
			set(SDL2_LIBRARIES "libs/SDL2/x86_64-w64-mingw32/lib")
		else(CMAKE_SIZEOF_VOID_P EQUAL 8)
			set(SDL2_INCLUDE_DIRS libs/SDL2/i686-w64-mingw32/include)
			set(SDL2_LIBRARIES "libs/SDL2/i686-w64-mingw32/lib")
		endif()
    elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
            set(SDL2_LIBRARIES "libs/SDL2/lib/x64")
        else(CMAKE_SIZEOF_VOID_P EQUAL 8)
            set(SDL2_LIBRARIES "libs/SDL2/lib/x86")
        endif()
        set(SDL2_INCLUDE_DIRS libs/SDL2/include)
    endif()
else(WIN32)
    include(FindPkgConfig)
    pkg_search_module (SDL2 REQUIRED sdl2)

    if(APPLE)
        set(SDL2_INCLUDE_DIRS /usr/local/include)
        set(SDL2_LIBRARIES /usr/local/lib)
    endif(APPLE)
endif(WIN32)

include_directories(libs)
include_directories(${SDL2_INCLUDE_DIRS})
include_directories(.)

link_directories(${SDL2_LIBRARIES})

#set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..)
#set(WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..)
enable_testing()

add_subdirectory(engine)
add_subdirectory(demo_app)
add_subdirectory(demo_lib)
add_subdirectory(math)
add_subdirectory(ren)
add_subdirectory(sys)
add_subdirectory(ui)

add_subdirectory(libs/SOIL2)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set_target_properties(demo_app
                      demo_lib PROPERTIES FOLDER App)

set_target_properties(engine
                      math
                      ren
                      SW
                      sys 
                      ui PROPERTIES FOLDER Libs)

set_target_properties(test_engine
                      test_math
                      test_ren
                      test_SW
                      test_sys
                      test_ui PROPERTIES FOLDER Tests)
					  
set_target_properties(SOIL2 PROPERTIES FOLDER _)

add_test(EngineTest     engine/tests/test_engine)
add_test(MathTest       math/tests/test_math)
add_test(RenTest        ren/tests/test_ren)
add_test(SWTest         ren/SW/tests/test_SW)
add_test(SysTest        sys/tests/test_sys)
add_test(UiTest         ui/tests/test_ui)

add_custom_target(Check COMMAND ${CMAKE_CTEST_COMMAND} DEPENDS test_engine
                                                               test_math
                                                               test_ren
                                                               test_SW
                                                               test_sys
                                                               test_ui)
set_target_properties(Check PROPERTIES FOLDER Tests)
