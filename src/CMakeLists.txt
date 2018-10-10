cmake_minimum_required(VERSION 2.8)
project(RayDemo)

set(CMAKE_CXX_STANDARD 11)

set(SDL_STATIC ON)

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

include_directories(libs)
include_directories(.)

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
