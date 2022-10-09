cmake_minimum_required(VERSION 3.1)
project(ltc_fit)

set(SOURCE_FILES _ltc_fit.cpp)

set(ORIG_SOURCE_FILES brdf.h
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

list(APPEND ALL_SOURCE_FILES ${ORIG_SOURCE_FILES})
set_source_files_properties(${ORIG_SOURCE_FILES} PROPERTIES HEADER_FILE_ONLY TRUE)

list(APPEND ALL_SOURCE_FILES ${SOURCE_FILES})
list(APPEND ALL_SOURCE_FILES ${ORIG_SOURCE_FILES})

add_executable(ltc_fit ${ALL_SOURCE_FILES})

set_target_properties(ltc_fit PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}")