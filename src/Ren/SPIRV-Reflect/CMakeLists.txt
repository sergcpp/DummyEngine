cmake_minimum_required(VERSION 2.8.4)
project(SPIRV-Reflect)

set(SOURCE_FILES spirv_reflect.h
				 spirv_reflect.c)

add_library(SPIRV-Reflect STATIC ${SOURCE_FILES})

