cmake_minimum_required(VERSION 3.5)
project(ltc_fit)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release;RelWithDebInfo;Asan" CACHE STRING "" FORCE)

IF(MSVC)
    if (NOT DEFINED CMAKE_CXX_FLAGS_ASAN OR CMAKE_CXX_FLAGS_ASAN STREQUAL "")
        set(CMAKE_CXX_FLAGS_ASAN "/MTd /Zi /Od /RTC1 /fsanitize=address")
        set(CMAKE_EXE_LINKER_FLAGS_ASAN "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /INCREMENTAL:NO")
    endif()
ELSE(MSVC)
ENDIF(MSVC)

if(WIN32)
    set(LIBS )
elseif(APPLE)
    set(LIBS )
else()
    set(LIBS X11)
endif()

set(SOURCE_FILES      brdf.h
                      brdf_beckmann.h
                      brdf_principledClearcoat.h
                      brdf_disneyDiffuse.h
                      brdf_principledDiffuse.h
                      brdf_principledSheen.h
                      brdf_principledSpecular.h
                      brdf_ggx.h
                      CImg.h
                      dds.cpp
                      dds.h
                      export.h
                      fitLTC.cpp
                      LTC.h
                      MMat.h
                      MVec.h
                      nelder_mead.h
                      plot.h)

add_executable(ltc_fit ${SOURCE_FILES})
target_link_libraries(ltc_fit ${LIBS})

set_target_properties(ltc_fit PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}")