cmake_minimum_required(VERSION 2.8.4)
project(Shiny)

set(SOURCE_FILES include/Shiny.h
				 include/ShinyConfig.h
				 include/ShinyData.h
				 include/ShinyMacros.h
				 include/ShinyManager.h
				 include/ShinyNode.h
				 include/ShinyNodePool.h
				 include/ShinyNodeState.h
				 include/ShinyOutput.h
				 include/ShinyPrereqs.h
				 include/ShinyTools.h
				 include/ShinyVersion.h
				 include/ShinyZone.h
				 src/ShinyManager.c
				 src/ShinyNode.c
				 src/ShinyNodePool.c
				 src/ShinyNodeState.c
				 src/ShinyOutput.c
				 src/ShinyTools.c
				 src/ShinyZone.c)


list(APPEND ALL_SOURCE_FILES ${SOURCE_FILES})
set_source_files_properties(${SOURCE_FILES} PROPERTIES HEADER_FILE_ONLY TRUE)

list(APPEND ALL_SOURCE_FILES src/_Shiny.c)

add_library(Shiny STATIC ${ALL_SOURCE_FILES})

