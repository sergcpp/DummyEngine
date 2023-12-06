cmake_minimum_required(VERSION 3.1)

set(RENDERER "VK" CACHE STRING "Render backend")

if(${RENDERER} STREQUAL "GL")
    project(DummyGL)
elseif(${RENDERER} STREQUAL "VK")
    project(DummyVK)
elseif(${RENDERER} STREQUAL "SW")
    project(DummySW)
endif()

set(WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR})

add_subdirectory(src)

