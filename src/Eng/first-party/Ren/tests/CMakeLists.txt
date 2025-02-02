cmake_minimum_required(VERSION 3.5)
project(test_Ren)

set(SDL_STATIC ON)

if(NOT CMAKE_SYSTEM_NAME MATCHES "Android")

    IF(WIN32)
        IF(SDL_STATIC)
            if (${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
                set(LIBS winmm imm32 version)
            elseif (${CMAKE_CXX_COMPILER_ID} STREQUAL "MSVC")
                set(LIBS winmm imm32 version)
            endif()
        ELSE(SDL_STATIC)
            if (${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
                set(LIBS opengl32)
            elseif (${CMAKE_CXX_COMPILER_ID} STREQUAL "MSVC")
                set(LIBS opengl32)
            endif()
        ENDIF(SDL_STATIC)
    ELSE(WIN32)
        set(LIBS X11 dl pthread GL c)
    ENDIF(WIN32)
endif()

set(SOURCE_FILES main.cpp
                 membuf.h
                 test_anim.cpp
                 test_buffer.cpp
                 test_common.h
                 test_freelist_alloc.cpp
                 test_hashmap.cpp
                 test_material.cpp
                 test_mesh.cpp
                 test_math.cpp
                 test_small_vector.cpp
                 test_span.cpp
                 test_sparse_array.cpp
                 test_storage.cpp
                 test_string.cpp
                 test_texture.cpp
                 test_utils.cpp)

if(${REN_BACKEND} STREQUAL "VK")
    set(SOURCE_FILES ${SOURCE_FILES}
                     test_programVK.cpp)
elseif(${REN_BACKEND} STREQUAL "GL")
    set(SOURCE_FILES ${SOURCE_FILES}
                     test_programGL.cpp)
elseif(${REN_BACKEND} STREQUAL "SW")
    set(SOURCE_FILES ${SOURCE_FILES}
                     test_programSW.cpp)
elseif(${REN_BACKEND} STREQUAL "VK")

endif()

add_executable(test_Ren ${SOURCE_FILES})
target_link_libraries(test_Ren ${LIBS} Ren)

set_target_properties(test_Ren PROPERTIES OUTPUT_NAME_DEBUG test_Ren-dbg)
set_target_properties(test_Ren PROPERTIES OUTPUT_NAME_RELWITHDEBINFO test_Ren-dev)
set_target_properties(test_Ren PROPERTIES OUTPUT_NAME_ASAN test_Ren-asan)
set_target_properties(test_Ren PROPERTIES OUTPUT_NAME_RELEASE test_Ren)