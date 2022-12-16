cmake_minimum_required(VERSION 3.1)
project(_Dummy)

set(CMAKE_CXX_STANDARD 11)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release;RelWithDebInfo;Asan" CACHE STRING "" FORCE)
set_property(GLOBAL PROPERTY DEBUG_CONFIGURATIONS "Debug;Asan")

if(NOT CMAKE_SYSTEM_NAME MATCHES "Android")
    if(WIN32)
        if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            # allow to inline some functions in debug mode
            set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -finline-small-functions ")
        elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
            if(CMAKE_SIZEOF_VOID_P EQUAL 8)
                set(ITT_LIBRARIES ${CMAKE_CURRENT_SOURCE_DIR}/libs/vtune/win/x64)
                set(VPX_LIBRARIES ${CMAKE_CURRENT_SOURCE_DIR}/libs/vpx/win/Release/x64)
                set(GLSLANG_LIBRARIES ${CMAKE_CURRENT_SOURCE_DIR}/libs/glslang/win)
            else(CMAKE_SIZEOF_VOID_P EQUAL 8)
                set(ITT_LIBRARIES ${CMAKE_CURRENT_SOURCE_DIR}/libs/vtune/win/x86)
                set(VPX_LIBRARIES ${CMAKE_CURRENT_SOURCE_DIR}/libs/vpx/win/Release/x86)
            endif()
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHs")
            # allow to inline some functions in debug mode
            set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Ob1 /fp:except")
            set(CMAKE_CXX_FLAGS_ASAN "${CMAKE_CXX_FLAGS_DEBUG} /MDd /fsanitize=address")
            set(CMAKE_EXE_LINKER_FLAGS_ASAN "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /INCREMENTAL:NO")
        endif()
    else(WIN32)
        # allow to inline some functions in debug mode
        #set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -finline-functions-called-once -finline-functions")
        #set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -finline-limit=10")

        if(APPLE)
            set(ITT_LIBRARIES ${CMAKE_CURRENT_SOURCE_DIR}/libs/vtune/osx/x64)
            if(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "arm64")
                set(VPX_LIBRARIES ${CMAKE_CURRENT_SOURCE_DIR}/libs/vpx/osx/arm64)
            else()
                set(VPX_LIBRARIES ${CMAKE_CURRENT_SOURCE_DIR}/libs/vpx/osx/x64)
                set(GLSLANG_LIBRARIES ${CMAKE_CURRENT_SOURCE_DIR}/libs/glslang/macos)
            endif()
        else(APPLE)
            set(ITT_LIBRARIES ${CMAKE_CURRENT_SOURCE_DIR}/libs/vtune/linux/x64)
            set(VPX_LIBRARIES ${CMAKE_CURRENT_SOURCE_DIR}/libs/vpx/linux/x64)
            set(GLSLANG_LIBRARIES ${CMAKE_CURRENT_SOURCE_DIR}/libs/glslang/linux)
        endif(APPLE)
    endif(WIN32)

    link_directories(${ITT_LIBRARIES})

    if(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "arm64")
        add_definitions(-DINTEL_NO_ITTNOTIFY_API)
    endif()
    add_definitions(-DENABLE_OBJ_LABELS)
else()
    add_definitions(-DINTEL_NO_ITTNOTIFY_API)
    set(VPX_LIBRARIES ${CMAKE_CURRENT_SOURCE_DIR}/libs/vpx/arm64-v8a)
endif()

set(RENDERER "VK" CACHE STRING "Render backend")

if(${RENDERER} STREQUAL "GL")
  add_definitions(-DUSE_GL_RENDER)
  set(BIN_NAME "DummyAppGL")
elseif(${RENDERER} STREQUAL "VK")
  add_definitions(-DUSE_VK_RENDER)
  set(BIN_NAME "DummyAppVK")
elseif(${RENDERER} STREQUAL "SW")
  add_definitions(-DUSE_SW_RENDER)
  set(BIN_NAME "DummyAppSW")
endif()

set(SOUND AL)

if(${SOUND} STREQUAL "AL")
  add_definitions(-DUSE_AL_SOUND)
endif()

include_directories(.)

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    include_directories(libs/dirent)
endif()

include_directories(libs/vpx/include)
link_directories(${VPX_LIBRARIES})

include_directories(libs/glslang/include)
link_directories(${GLSLANG_LIBRARIES})

include_directories(libs)

enable_testing()

if(CMAKE_SYSTEM_NAME MATCHES "Android")
    link_directories(${CMAKE_CURRENT_SOURCE_DIR}/Snd/OpenAL/libs/arm64-v8a)
endif()

add_subdirectory(DummyApp)
add_subdirectory(DummyLib)
add_subdirectory(Eng)
add_subdirectory(Modl)
add_subdirectory(Net)
add_subdirectory(Phy)
add_subdirectory(Ray)
add_subdirectory(Ren)
add_subdirectory(Snd)
add_subdirectory(Sys)

add_subdirectory(libs/astc)
add_subdirectory(libs/optick)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set_target_properties(DummyApp
                      DummyLib
                      ModlApp PROPERTIES FOLDER App)

set_target_properties(Eng
                      Net
                      Phy
                      Ray
                      Ren
                      SW
                      Snd
                      Sys PROPERTIES FOLDER Libs)

set_target_properties(test_DummyLib
                      test_Eng
                      test_Net
                      test_Phy
                      test_Ray
                      test_Ren
                      test_SW
                      test_Snd
                      test_Sys PROPERTIES FOLDER Tests)
                      
set_target_properties(astc
                      stb
                      SPIRV-Reflect
                      optick PROPERTIES FOLDER _)

add_test(DummyLibTest   DummyLib/tests/test_DummyLib)
add_test(EngTest        Eng/_tests/test_Eng)
add_test(NetTest        Net/tests/test_Net)
add_test(PhyTest        Phy/tests/test_Phy)
add_test(RayTest        Ray/tests/test_Ray)
add_test(RenTest        Ren/tests/test_Ren)
add_test(SWTest         Ren/SW/tests/test_SW)
add_test(SndTest        Snd/tests/test_Snd)
add_test(SysTest        Sys/tests/test_Sys)

add_custom_target(Check COMMAND ${CMAKE_CTEST_COMMAND} DEPENDS test_DummyLib
                                                               test_Eng
                                                               test_Net
                                                               test_Phy
                                                               test_Ray
                                                               test_Ren
                                                               test_SW
                                                               test_Snd
                                                               test_Sys)
set_target_properties(Check PROPERTIES FOLDER Tests)
