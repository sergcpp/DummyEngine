cmake_minimum_required(VERSION 3.1)
project(Ren)

if(WIN32)
    add_definitions(-DVK_USE_PLATFORM_WIN32_KHR)
else(WIN32)
    set(CMAKE_CXX_STANDARD 11)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
    if(NOT CMAKE_SYSTEM_NAME MATCHES "Android")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx2 -msse2")
    endif()
if(${RENDERER} STREQUAL "GL")
    set(LIBS GL)
elseif(${RENDERER} STREQUAL "VK")
    set(LIBS GL)
    add_definitions(-DVK_PROTOTYPES -DVK_USE_PLATFORM_XCB_KHR)
endif()
ENDIF(WIN32)

if(CMAKE_SYSTEM_NAME MATCHES "Android")
    set(LIBS GLESv2 GLESv3 EGL)
endif()

set(SOURCE_FILES Anim.h
                 Anim.cpp
                 Buffer.h
                 Buffer.cpp
                 Camera.h
                 Camera.cpp
                 Context.h
                 Context.cpp
                 CPUFeatures.h
                 CPUFeatures.cpp
                 Fwd.h
                 HashMap32.h
                 Log.h
                 Material.h
                 Material.cpp
                 Mesh.h
                 Mesh.cpp
                 MMat.h
                 MQuat.h
                 MVec.h
                 Program.h
                 RingBuffer.h
                 Shader.h
                 SparseArray.h
                 Storage.h
                 String.h
                 TaskExecutor.h
                 TaskExecutor.cpp
                 Texture.h
                 Texture.cpp
                 TextureAtlas.h
                 TextureRegion.h
                 TextureRegion.cpp
                 TextureSplitter.h
                 TextureSplitter.cpp
                 Utils.h
                 Utils.cpp
                 Utils_AVX.cpp
                 Utils_SSE2.cpp)

if(${RENDERER} STREQUAL "GL")
    add_definitions(-DUSE_GL_RENDER)
    set(SOURCE_FILES ${SOURCE_FILES}
                     ContextGL.cpp
                     GL.h
                     GLExt.cpp
                     GLExtDSAEmu.h
                     GLExtDSAEmu.cpp
                     ShaderGL.h
                     ShaderGL.cpp
                     ProgramGL.h
                     ProgramGL.cpp
                     TextureGL.h
                     TextureGL.cpp
                     TextureAtlasGL.cpp)

    if(WIN32)
        set(LIBS ${LIBS} opengl32 SOIL2 SPIRV-Reflect)
    else(WIN32)
        set(LIBS ${LIBS} SOIL2 SPIRV-Reflect)
    ENDIF(WIN32)
elseif(${RENDERER} STREQUAL "SW")
    add_definitions(-DUSE_SW_RENDER)
    set(SOURCE_FILES ${SOURCE_FILES}
                     ContextSW.cpp
                     ProgramSW.h
                     ProgramSW.cpp
                     TextureSW.h
                     TextureSW.cpp)
elseif(${RENDERER} STREQUAL "VK")
    set(SOURCE_FILES    ${SOURCE_FILES}
                        RenderStateVK.cpp
                        RenderStateVK.h)
endif()

add_subdirectory(SW)
add_subdirectory(SOIL2)
add_subdirectory(SPIRV-Reflect)

list(APPEND ALL_SOURCE_FILES ${SOURCE_FILES})
set_source_files_properties(${SOURCE_FILES} PROPERTIES HEADER_FILE_ONLY TRUE)
source_group("src" FILES ${SOURCE_FILES})

list(APPEND ALL_SOURCE_FILES _Ren.cpp _Ren_SSE2.cpp _Ren_AVX.cpp)
source_group("src" FILES _Ren.cpp _Ren_SSE2.cpp _Ren_AVX.cpp)

if(MSVC)
    if(NOT CMAKE_CL_64)
        set_source_files_properties(_Ren_Sse2.cpp PROPERTIES COMPILE_FLAGS /arch:SSE2)
    endif()
    set_source_files_properties(_Ren_Avx.cpp PROPERTIES COMPILE_FLAGS /arch:AVX)
endif(MSVC)

add_library(Ren STATIC ${ALL_SOURCE_FILES})
target_link_libraries(Ren ${LIBS})

add_subdirectory(tests)
