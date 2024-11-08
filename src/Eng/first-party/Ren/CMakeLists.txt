cmake_minimum_required(VERSION 3.1)
project(Ren)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_library(METAL_LIB Metal)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release;RelWithDebInfo;Asan" CACHE STRING "" FORCE)

IF(MSVC)
    if (NOT DEFINED CMAKE_CXX_FLAGS_ASAN)
        set(CMAKE_CXX_FLAGS_ASAN "/MTd /Zi /Od /RTC1 /fsanitize=address")
        set(CMAKE_EXE_LINKER_FLAGS_ASAN "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /INCREMENTAL:NO")
    endif()
ELSE(MSVC)
ENDIF(MSVC)

if(WIN32)
else(WIN32)
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
    else(APPLE)
        set(LIBS X11)
    endif(APPLE)
endif()
ENDIF(WIN32)

if(CMAKE_SYSTEM_NAME MATCHES "Android")
    set(LIBS GLESv2 GLESv3 EGL)
endif()

set(SOURCE_FILES AccStructure.h
                 AccStructure.cpp
                 Anim.h
                 Anim.cpp
                 ApiContext.h
                 Bitmask.h
                 Buffer.h
                 Buffer.cpp
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
                 FreelistAlloc.h
                 FreelistAlloc.cpp
                 MemoryAllocator.h
                 Fence.h
                 Fixed.h
                 Framebuffer.h
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
                 ScopeExit.h
                 Shader.h
                 SmallVector.h
                 Span.h
                 SparseArray.h
                 Storage.h
                 String.h
                 Texture.h
                 Texture.cpp
                 TextureArray.h
                 TextureAtlas.h
                 TextureAtlas.cpp
                 TextureParams.h
                 TextureParams.cpp
                 TextureRegion.h
                 TextureRegion.cpp
                 TextureSplitter.h
                 TextureSplitter.cpp
                 Utils.h
                 Utils.cpp
                 Utils_AVX.cpp
                 Utils_SSE2.cpp
                 Utils_NEON.cpp
                 VertexInput.h)

if(${RENDERER} STREQUAL "GL")
    set(SOURCE_FILES ${SOURCE_FILES}
                     BufferGL.cpp
                     ContextGL.cpp
                     DebugMarkerGL.cpp
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
                     TextureArrayGL.cpp
                     TextureAtlasGL.cpp
                     VertexInputGL.cpp)

    if(WIN32)
        set(LIBS ${LIBS} opengl32 SPIRV-Reflect)
    else(WIN32)
        set(LIBS ${LIBS} SPIRV-Reflect)
    ENDIF(WIN32)
elseif(${RENDERER} STREQUAL "SW")
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
                     DebugMarkerVK.cpp
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
                     TextureArrayVK.cpp
                     TextureAtlasVK.cpp
                     VertexInputVK.cpp
                     VK.h
                     VKCtx.h
                     VKCtx.cpp)

    set(VULKAN_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/third-party/vulkan/vk_layer.h
                       ${CMAKE_CURRENT_SOURCE_DIR}/third-party/vulkan/vk_platform.h
                       ${CMAKE_CURRENT_SOURCE_DIR}/third-party/vulkan/vk_sdk_platform.h
                       ${CMAKE_CURRENT_SOURCE_DIR}/third-party/vulkan/vulkan.h
                       ${CMAKE_CURRENT_SOURCE_DIR}/third-party/vulkan/vulkan_core.h)

    list(APPEND REN_SOURCE_FILES ${VULKAN_HEADERS})
    source_group("src/vulkan" FILES ${VULKAN_HEADERS})

    set(LIBS ${LIBS} SPIRV-Reflect)
endif()

add_subdirectory(SW)
add_subdirectory(third-party/SPIRV-Reflect)

list(APPEND REN_SOURCE_FILES ${SOURCE_FILES})
source_group("src" FILES ${SOURCE_FILES})

if(NOT CMAKE_GENERATOR_PLATFORM MATCHES "ARM64")
    if(MSVC)
        if(NOT CMAKE_CL_64)
            set_source_files_properties(Utils_SSE2.cpp PROPERTIES COMPILE_FLAGS /arch:SSE2)
        endif()
        if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            set_source_files_properties(Utils_SSE2.cpp PROPERTIES COMPILE_FLAGS -msse4.1)
            set_source_files_properties(Utils_AVX.cpp PROPERTIES COMPILE_FLAGS "-mavx2 -mfma")
        else()
            set_source_files_properties(Utils_AVX.cpp PROPERTIES COMPILE_FLAGS /arch:AVX)
        endif()
        list(APPEND REN_SOURCE_FILES _CustomTypes.natvis)
    else(MSVC)
        set_source_files_properties(Utils_SSE2.cpp PROPERTIES COMPILE_FLAGS -msse2)
        set_source_files_properties(Utils_AVX.cpp PROPERTIES COMPILE_FLAGS "-mavx2 -mfma")
    endif(MSVC)
endif(NOT CMAKE_GENERATOR_PLATFORM MATCHES "ARM64")

set_source_files_properties(GLExt.cpp PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)
set_source_files_properties(VKExt.cpp PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)
#set_source_files_properties(VKCtx.cpp PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)

add_library(Ren STATIC ${REN_SOURCE_FILES})
target_compile_definitions(Ren PUBLIC ENABLE_OBJ_LABELS)
if(${RENDERER} STREQUAL "GL")
    target_compile_definitions(Ren PUBLIC USE_GL_RENDER)
elseif(${RENDERER} STREQUAL "VK")
    target_compile_definitions(Ren PUBLIC USE_VK_RENDER)
    if(WIN32)
        target_compile_definitions(Ren PUBLIC VK_USE_PLATFORM_WIN32_KHR)
    else(WIN32)
        if(APPLE)
            target_compile_definitions(Ren PUBLIC VK_USE_PLATFORM_MACOS_MVK)
        else(APPLE)
            target_compile_definitions(Ren PUBLIC VK_USE_PLATFORM_XLIB_KHR)
        endif(APPLE)
    endif(WIN32)
elseif(${RENDERER} STREQUAL "SW")
    target_compile_definitions(Ren PUBLIC USE_SW_RENDER)
endif()
target_link_libraries(Ren SW ${LIBS})
target_include_directories(Ren PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third-party)

add_subdirectory(tests)
