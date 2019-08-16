cmake_minimum_required(VERSION 3.1)
project(stb)

set(SOURCE_FILES stb_dxt.h
				 stb_image.h
				 stb_image_resize.h
				 stb_image_write.h
				 stb.c)

source_group("src" FILES ${SOURCE_FILES})

add_library(stb STATIC ${SOURCE_FILES})

