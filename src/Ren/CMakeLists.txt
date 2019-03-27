cmake_minimum_required(VERSION 3.1)
project(Ren)

if(WIN32)
    add_definitions(-DVK_USE_PLATFORM_WIN32_KHR)
else(WIN32)
    set(CMAKE_CXX_STANDARD 11)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
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
                 Fwd.h
                 Material.h
                 Material.cpp
                 Mesh.h
                 Mesh.cpp
                 MMat.h
                 MQuat.h
                 MVec.h
                 Program.h
                 RenderThread.h
                 RenderThread.cpp
                 RingBuffer.h
                 SparseArray.h
                 Storage.h
                 Texture.h
                 TextureAtlas.h
                 TextureAtlas.cpp
                 TextureSplitter.h
                 TextureSplitter.cpp
                 Utils.h
                 Utils.cpp)

if(${RENDERER} STREQUAL "GL")
    add_definitions(-DUSE_GL_RENDER)
    set(SOURCE_FILES ${SOURCE_FILES}
                     ContextGL.cpp
                     GL.h
                     GLExt.cpp
                     ProgramGL.h
                     ProgramGL.cpp
                     TextureGL.h
                     TextureGL.cpp)

    if(WIN32)
        if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
            set(LIBS ${LIBS} SOIL2 opengl32)
        else()
            set(LIBS ${LIBS} SOIL2 opengl32)
        endif()
    else(WIN32)
        set(LIBS ${LIBS} SOIL2)
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

list(APPEND ALL_SOURCE_FILES ${SOURCE_FILES})
set_source_files_properties(${SOURCE_FILES} PROPERTIES HEADER_FILE_ONLY TRUE)
source_group("src" FILES ${SOURCE_FILES})

list(APPEND ALL_SOURCE_FILES _Ren.cpp)
source_group("src" FILES _Ren.cpp)

add_library(Ren STATIC ${ALL_SOURCE_FILES})
target_link_libraries(Ren ${LIBS})

add_subdirectory(tests)
