cmake_minimum_required(VERSION 3.10)

set(REN_BACKEND "VK" CACHE STRING "Render backend")

if(${REN_BACKEND} STREQUAL "GL")
    project(DummyGL)
elseif(${REN_BACKEND} STREQUAL "VK")
    project(DummyVK)
endif()

set(WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR})

add_subdirectory(src)

