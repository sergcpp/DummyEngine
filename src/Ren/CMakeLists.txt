cmake_minimum_required(VERSION 3.1)
project(Ren)

find_library(METAL_LIB Metal)

if(WIN32)
    add_definitions(-DVK_USE_PLATFORM_WIN32_KHR)
else(WIN32)
    set(CMAKE_CXX_STANDARD 11)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
    if(NOT CMAKE_SYSTEM_NAME MATCHES "Android")
        #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx2 -msse2 -mfma -mavx512f -mavx512bw -mavx512dq")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse4.1")
    endif()
if(${RENDERER} STREQUAL "GL")
    set(LIBS GL)
elseif(${RENDERER} STREQUAL "VK")
    if(APPLE)
        set(LIBS ${METAL_LIB})
        add_definitions(-DVK_PROTOTYPES -DVK_USE_PLATFORM_MACOS_MVK)
    else(APPLE)
        add_definitions(-DVK_PROTOTYPES -DVK_USE_PLATFORM_XLIB_KHR)
    endif(APPLE)
endif()
ENDIF(WIN32)

if(CMAKE_SYSTEM_NAME MATCHES "Android")
    set(LIBS GLESv2 GLESv3 EGL)
endif()

set(SOURCE_FILES Anim.h
                 Anim.cpp
                 AccStructure.h
                 AccStructure.cpp
                 Buffer.h
                 Camera.h
                 Camera.cpp
                 Common.h
                 Context.h
                 Context.cpp
                 CPUFeatures.h
                 CPUFeatures.cpp
                 DebugMarker.h
                 DescriptorPool.h
                 DrawCall.h
                 MemoryAllocator.h
                 Fence.h
                 Fixed.h
                 Framebuffer.h
                 Fwd.h
                 HashMap32.h
                 LinearAlloc.h
                 LinearAlloc.cpp
                 Log.h
                 Material.h
                 Material.cpp
                 Mesh.h
                 Mesh.cpp
                 MMat.h
                 MQuat.h
                 MVec.h
                 Pipeline.h
                 ProbeStorage.h
                 Program.h
                 RastState.h
                 RenderPass.h
                 Resource.h
                 Resource.cpp
                 RingBuffer.h
                 Sampler.h
                 SamplingParams.h
                 Shader.h
                 SmallVector.h
                 SparseArray.h
                 Storage.h
                 String.h
                 TaskExecutor.h
                 TaskExecutor.cpp
                 Texture.h
                 Texture.cpp
                 TextureAtlas.h
                 TextureParams.h
                 TextureRegion.h
                 TextureRegion.cpp
                 TextureSplitter.h
                 TextureSplitter.cpp
                 Utils.h
                 Utils.cpp
                 Utils_AVX.cpp
                 Utils_SSE2.cpp
                 VertexInput.h)

if(${RENDERER} STREQUAL "GL")
    add_definitions(-DUSE_GL_RENDER)
    set(SOURCE_FILES ${SOURCE_FILES}
                     BufferGL.cpp
                     ContextGL.cpp
                     DrawCallGL.cpp
                     FramebufferGL.cpp
                     GL.h
                     GLCtx.h
                     GLCtx.cpp
                     GLExt.cpp
                     GLExtDSAEmu.h
                     GLExtDSAEmu.cpp
                     FenceGL.cpp
                     MemoryAllocatorGL.cpp
                     ResourceGL.cpp
                     SamplerGL.h
                     SamplerGL.cpp
                     ShaderGL.h
                     ShaderGL.cpp
                     PipelineGL.cpp
                     ProbeStorageGL.cpp
                     ProgramGL.h
                     ProgramGL.cpp
                     RastStateGL.cpp
                     RenderPassGL.cpp
                     TextureGL.h
                     TextureGL.cpp
                     TextureAtlasGL.cpp
                     VertexInputGL.cpp)

    if(WIN32)
        set(LIBS ${LIBS} opengl32 stb SPIRV-Reflect)
    else(WIN32)
        set(LIBS ${LIBS} stb SPIRV-Reflect)
    ENDIF(WIN32)
elseif(${RENDERER} STREQUAL "SW")
    add_definitions(-DUSE_SW_RENDER)
    set(SOURCE_FILES ${SOURCE_FILES}
                     BufferSW.cpp
                     ContextSW.cpp
                     ProgramSW.h
                     ProgramSW.cpp
                     TextureSW.h
                     TextureSW.cpp)
elseif(${RENDERER} STREQUAL "VK")
    set(SOURCE_FILES ${SOURCE_FILES}
                     AccStructureVK.cpp
                     BufferVK.cpp
                     ContextVK.cpp
                     DescriptorPoolVK.cpp
                     DrawCallVK.cpp
                     FenceVK.cpp
                     FramebufferVK.cpp
                     MemoryAllocatorVK.cpp
                     PipelineVK.cpp
                     ProbeStorageVK.cpp
                     ProgramVK.h
                     ProgramVK.cpp
                     RastStateVK.cpp
                     RenderPassVK.cpp
                     ResourceVK.cpp
                     SamplerVK.h
                     SamplerVK.cpp
                     ShaderVK.h
                     ShaderVK.cpp
                     TextureVK.h
                     TextureVK.cpp
                     TextureAtlasVK.cpp
                     VertexInputVK.cpp
                     VK.h
                     VKCtx.h
                     VKCtx.cpp
                     VKExt.cpp)

    set(VULKAN_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/vk_layer.h
                       ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/vk_platform.h
                       ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/vk_sdk_platform.h
                       ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/vulkan.h
                       ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/vulkan_core.h)

    list(APPEND ALL_SOURCE_FILES ${VULKAN_HEADERS})
    source_group("src/vulkan" FILES ${VULKAN_HEADERS})

    set(LIBS ${LIBS} stb SPIRV-Reflect)
endif()

add_subdirectory(SW)
add_subdirectory(stb)
add_subdirectory(SPIRV-Reflect)

list(APPEND ALL_SOURCE_FILES ${SOURCE_FILES})
set_source_files_properties(${SOURCE_FILES} PROPERTIES HEADER_FILE_ONLY TRUE)
source_group("src" FILES ${SOURCE_FILES})

list(APPEND ALL_SOURCE_FILES _Ren.cpp _Ren_SSE2.cpp _Ren_AVX.cpp _Ren_AVX512.cpp _Ren_NEON.cpp)
source_group("src" FILES _Ren.cpp _Ren_SSE2.cpp _Ren_AVX.cpp _Ren_AVX512.cpp _Ren_NEON.cpp)

if(MSVC)
    if(NOT CMAKE_CL_64)
        set_source_files_properties(_Ren_SSE2.cpp PROPERTIES COMPILE_FLAGS /arch:SSE2)
    endif()
    set_source_files_properties(_Ren_AVX.cpp PROPERTIES COMPILE_FLAGS /arch:AVX)
    set_source_files_properties(_Ren_AVX512.cpp PROPERTIES COMPILE_FLAGS /arch:AVX512)

    list(APPEND ALL_SOURCE_FILES _CustomTypes.natvis)
endif(MSVC)

add_library(Ren STATIC ${ALL_SOURCE_FILES})
target_link_libraries(Ren SW ${LIBS})

add_subdirectory(tests)
