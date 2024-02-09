cmake_minimum_required(VERSION 3.1)
project(astc)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release;RelWithDebInfo;Asan" CACHE STRING "" FORCE)

IF(MSVC)
    if (NOT DEFINED CMAKE_CXX_FLAGS_ASAN OR CMAKE_CXX_FLAGS_ASAN STREQUAL "")
        set(CMAKE_CXX_FLAGS_ASAN "/MTd /Zi /Od /RTC1 /fsanitize=address")
        set(CMAKE_EXE_LINKER_FLAGS_ASAN "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /INCREMENTAL:NO")
    endif()
ELSE(MSVC)
ENDIF(MSVC)

set(SOURCE_FILES _astc.cpp)

set(ORIG_SOURCE_FILES astc_averages_and_directions.cpp
                      astc_block_sizes2.cpp
                      astc_color_quantize.cpp
                      astc_color_unquantize.cpp
                      astc_compress_symbolic.cpp
                      astc_compute_variance.cpp
                      astc_decompress_symbolic.cpp
                      astc_encoding_choice_error.cpp
                      astc_find_best_partitioning.cpp
                      astc_ideal_endpoints_and_weights.cpp
                      astc_image_load_store.cpp
                      astc_integer_sequence.cpp
                      astc_kmeans_partitioning.cpp
                      astc_ktx_dds.cpp
                      astc_partition_tables.cpp
                      astc_percentile_tables.cpp
                      astc_pick_best_endpoint_format.cpp
                      astc_quantization.cpp
                      astc_stb_tga.cpp
                      astc_symbolic_physical.cpp
                      astc_toplevel.cpp
                      astc_weight_align.cpp
                      astc_weight_quant_xfer_tables.cpp
                      exr_to_htga.cpp
                      imgdiff.cpp
                      mathlib.cpp
                      softfloat.cpp
                      stb_image.c)

list(APPEND ASTC_SOURCE_FILES ${ORIG_SOURCE_FILES})
set_source_files_properties(${ORIG_SOURCE_FILES} PROPERTIES HEADER_FILE_ONLY TRUE)

list(APPEND ASTC_SOURCE_FILES ${SOURCE_FILES})
list(APPEND ASTC_SOURCE_FILES ${ORIG_SOURCE_FILES})

add_library(astc STATIC ${ASTC_SOURCE_FILES})

