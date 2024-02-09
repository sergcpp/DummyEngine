cmake_minimum_required(VERSION 3.1)
project(glslx)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release;RelWithDebInfo;Asan" CACHE STRING "" FORCE)

if(MSVC)
    if (NOT DEFINED CMAKE_CXX_FLAGS_ASAN OR CMAKE_CXX_FLAGS_ASAN STREQUAL "")
        set(CMAKE_CXX_FLAGS_ASAN "/MTd /Zi /Od /RTC1 /fsanitize=address")
        set(CMAKE_EXE_LINKER_FLAGS_ASAN "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /INCREMENTAL:NO")
    endif()
else(MSVC)
endif(MSVC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCE_FILES Bitmask.h
                 Fixup.h
                 Fixup.cpp
                 glslx.h
                 glslx.cpp
                 KeywordsHLSL.inl
                 PreludeGLSL.h
                 PreludeGLSL.cpp
                 Preprocessor.h
                 Preprocessor.cpp
                 Prune.h
                 Prune.cpp
                 ScopeExit.h
                 SmallVector.h
                 Span.h
                 WriterBase.h
                 WriterBase.cpp
                 WriterGLSL.h
                 WriterGLSL.cpp
                 WriterHLSL.h
                 WriterHLSL.cpp)

set(PARSER_SOURCE_FILES parser/AST.h
                        parser/AST.cpp
                        parser/BuiltinPrototypes.inl
                        parser/Keywords.inl
                        parser/LayoutQualifiers.inl
                        parser/Lexer.h
                        parser/Lexer.cpp
                        parser/Operators.inl
                        parser/Parser.h
                        parser/Parser.cpp
                        parser/Utils.h
                        parser/Utils.cpp)

list(APPEND LIB_SOURCE_FILES ${SOURCE_FILES})
source_group("src" FILES ${SOURCE_FILES})

list(APPEND LIB_SOURCE_FILES ${PARSER_SOURCE_FILES})
source_group("src/parser" FILES ${PARSER_SOURCE_FILES})

add_library(glslx_lib STATIC ${LIB_SOURCE_FILES})

add_executable(glslx main.cpp)
target_link_libraries(glslx glslx_lib)

set_target_properties(glslx PROPERTIES OUTPUT_NAME_DEBUG glslx-dbg)
set_target_properties(glslx PROPERTIES OUTPUT_NAME_RELWITHDEBINFO glslx-dev)
set_target_properties(glslx PROPERTIES OUTPUT_NAME_ASAN glslx-asan)
set_target_properties(glslx PROPERTIES OUTPUT_NAME_RELEASE glslx)

add_subdirectory(tests)
