cmake_minimum_required(VERSION 3.10)
project(_Dummy)

OPTION(ENABLE_GPU_DEBUG "Enable GPU debugging" ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_definitions(-D_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release;RelWithDebInfo;Asan;Tsan" CACHE STRING "" FORCE)
set_property(GLOBAL PROPERTY DEBUG_CONFIGURATIONS "Debug;Asan")

if(NOT CMAKE_SYSTEM_NAME MATCHES "Android")
    if(WIN32)
        if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            # allow to inline some functions in debug mode
            set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -finline-small-functions ")
        elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHs")
            # allow to inline some functions in debug mode
            set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Ob1 /fp:except")
            if (NOT DEFINED CMAKE_CXX_FLAGS_ASAN OR CMAKE_CXX_FLAGS_ASAN STREQUAL "")
                set(CMAKE_CXX_FLAGS_ASAN "/MP /MDd /Zi /Od /Ob1 /RTC1 /fsanitize=address /analyze /WX /wd6993")
                set(CMAKE_C_FLAGS_ASAN "/MP /MDd /Zi /Od /Ob1 /RTC1 /fsanitize=address /analyze /WX /wd6993")
                set(CMAKE_EXE_LINKER_FLAGS_ASAN "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /INCREMENTAL:NO")
            endif()
            if (NOT DEFINED CMAKE_CXX_FLAGS_TSAN OR CMAKE_CXX_FLAGS_TSAN STREQUAL "")
                set(CMAKE_CXX_FLAGS_TSAN "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
                set(CMAKE_C_FLAGS_TSAN "${CMAKE_C_FLAGS_RELWITHDEBINFO}")
                set(CMAKE_EXE_LINKER_FLAGS_TSAN "${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO}")
            endif()
        endif()
    else(WIN32)
        # allow to inline some functions in debug mode
        #set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -finline-functions-called-once -finline-functions")
        #set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -finline-limit=10")

        if (NOT DEFINED CMAKE_CXX_FLAGS_TSAN OR CMAKE_CXX_FLAGS_TSAN STREQUAL "")
            set(CMAKE_CXX_FLAGS_TSAN "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -fsanitize=address")
            set(CMAKE_C_FLAGS_TSAN "${CMAKE_C_FLAGS_RELWITHDEBINFO} -fsanitize=address")
            set(CMAKE_EXE_LINKER_FLAGS_TSAN "${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO} -fsanitize=address")
        endif()
    endif(WIN32)

    link_directories(${ITT_LIBRARIES})

    if(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "arm64" OR CMAKE_GENERATOR_PLATFORM MATCHES "ARM64")
        add_definitions(-DINTEL_NO_ITTNOTIFY_API)
    endif()
else()
    add_definitions(-DINTEL_NO_ITTNOTIFY_API)
    set(VPX_LIBRARIES ${CMAKE_CURRENT_SOURCE_DIR}/libs/vpx/arm64-v8a)
endif()

include_directories(.)
include_directories(libs)

if(CMAKE_SYSTEM_NAME MATCHES "Android")
    link_directories(${CMAKE_CURRENT_SOURCE_DIR}/Snd/third-party/OpenAL/libs/arm64-v8a)
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    # Ignore warnings about missing PDB files
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /ignore:4099")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /ignore:4099")
    set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} /ignore:4099")
endif()

add_subdirectory(DummyApp)
add_subdirectory(DummyLib)
add_subdirectory(Eng)
if(${REN_BACKEND} STREQUAL "GL")
    add_subdirectory(Modl)
endif()
add_subdirectory(Ray)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set_target_properties(DummyApp
                      DummyLib PROPERTIES FOLDER App)

if(${REN_BACKEND} STREQUAL "GL")
    set_target_properties(ModlAppGL PROPERTIES FOLDER App)
endif()

set_target_properties(Eng
                      Gui
                      Net
                      Phy
                      Ray
                      Ren
                      SW
                      Snd
                      Sys
                      glslx_lib PROPERTIES FOLDER Libs)

set_target_properties(00_basic
                      01_texturing
                      02_multithreading
                      03_principled
                      04_denoising
                      05_physical_sky PROPERTIES FOLDER Samples)

set_target_properties(test_Eng
                      test_Gui
                      test_Net
                      test_Phy
                      test_Ray
                      test_Ren
                      test_SW
                      test_Snd
                      test_Sys
                      test_glslx PROPERTIES FOLDER Tests)

set_target_properties(ltc_fit
                      stb
                      SPIRV-Reflect
                      optick
                      glslx
                      DummyLib_CreateSymLinks PROPERTIES FOLDER _)
if(${REN_BACKEND} STREQUAL "VK")
    set_target_properties(spirv_compiler PROPERTIES FOLDER _)
endif()