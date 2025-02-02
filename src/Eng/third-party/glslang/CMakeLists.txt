cmake_minimum_required(VERSION 3.5)
project(spirv_compiler)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release;RelWithDebInfo;Asan;Tsan" CACHE STRING "" FORCE)

if(MSVC)
    set(CMAKE_CXX_FLAGS_DEBUG "/MP /MD /Zi /Od /RTC1")
    if (NOT DEFINED CMAKE_CXX_FLAGS_ASAN OR CMAKE_CXX_FLAGS_ASAN STREQUAL "")
        set(CMAKE_CXX_FLAGS_ASAN "/MP /MD /Zi /Od /RTC1 /fsanitize=address")
        set(CMAKE_C_FLAGS_ASAN "/MP /MD /Zi /Od /RTC1 /fsanitize=address")
        set(CMAKE_EXE_LINKER_FLAGS_ASAN "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /INCREMENTAL:NO")
    endif()
    if (NOT DEFINED CMAKE_CXX_FLAGS_TSAN OR CMAKE_CXX_FLAGS_TSAN STREQUAL "")
        set(CMAKE_CXX_FLAGS_TSAN "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
        set(CMAKE_C_FLAGS_TSAN "${CMAKE_C_FLAGS_RELWITHDEBINFO}")
        set(CMAKE_EXE_LINKER_FLAGS_TSAN "${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO}")
    endif()
else(MSVC)
    if (NOT DEFINED CMAKE_CXX_FLAGS_TSAN OR CMAKE_CXX_FLAGS_TSAN STREQUAL "")
        set(CMAKE_CXX_FLAGS_TSAN "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -fsanitize=address")
        set(CMAKE_C_FLAGS_TSAN "${CMAKE_C_FLAGS_RELWITHDEBINFO} -fsanitize=address")
        set(CMAKE_EXE_LINKER_FLAGS_TSAN "${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO} -fsanitize=address")
    endif()
endif(MSVC)

set(GLSLANG_LIBS glslang
                 glslang-default-resource-limits
                 MachineIndependent
                 GenericCodeGen
                 OSDependent
                 SPIRV-Tools-diff
                 SPIRV-Tools-link
                 SPIRV-Tools-lint
                 SPIRV-Tools-opt
                 SPIRV-Tools-reduce
                 SPIRV-Tools
                 SPIRV
                 SPVRemapper)

add_library(spirv_compiler SHARED main.cpp)
target_compile_definitions(spirv_compiler PUBLIC -D_ITERATOR_DEBUG_LEVEL=0)
target_include_directories(spirv_compiler PUBLIC ../)
target_include_directories(spirv_compiler PUBLIC include)
if (WIN32)
    if(CMAKE_GENERATOR_PLATFORM MATCHES "ARM64")
        target_link_directories(spirv_compiler PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/win/arm64)
    else(CMAKE_GENERATOR_PLATFORM MATCHES "ARM64")
        target_link_directories(spirv_compiler PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/win/x64)
    endif(CMAKE_GENERATOR_PLATFORM MATCHES "ARM64")
else(WIN32)
    if (APPLE)
        if(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "arm64")
            target_link_directories(spirv_compiler PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/macos/arm64)
        else()
            target_link_directories(spirv_compiler PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/macos/x64)
        endif()
    else(APPLE)
        target_link_directories(spirv_compiler PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/linux)
        set(GLSLANG_LIBS ${GLSLANG_LIBS} X11)
    endif(APPLE)
endif(WIN32)
target_link_libraries(spirv_compiler ${GLSLANG_LIBS})
set_target_properties(spirv_compiler PROPERTIES PREFIX "")