cmake_minimum_required(VERSION 3.1)
project(_Dummy)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release;RelWithDebInfo;Asan" CACHE STRING "" FORCE)
set_property(GLOBAL PROPERTY DEBUG_CONFIGURATIONS "Debug;Asan")

if(NOT CMAKE_SYSTEM_NAME MATCHES "Android")
    if(WIN32)
        if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            # allow to inline some functions in debug mode
            set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -finline-small-functions ")
        elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
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

include_directories(.)

include_directories(libs)

enable_testing()

if(CMAKE_SYSTEM_NAME MATCHES "Android")
    link_directories(${CMAKE_CURRENT_SOURCE_DIR}/Snd/OpenAL/libs/arm64-v8a)
endif()

add_subdirectory(DummyApp)
add_subdirectory(DummyLib)
add_subdirectory(Eng)
add_subdirectory(Modl)
add_subdirectory(Ray)

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

set_target_properties(00_basic
                      01_texturing
                      02_multithreading
                      03_principled
                      04_denoising
                      05_physical_sky PROPERTIES FOLDER Samples)

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
                      ltc_fit
                      stb
                      SPIRV-Reflect
                      optick PROPERTIES FOLDER _)

add_test(DummyLibTest   DummyLib/tests/test_DummyLib)
add_test(EngTest        Eng/tests/test_Eng)
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
