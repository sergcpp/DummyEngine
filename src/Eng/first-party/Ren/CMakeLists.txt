cmake_minimum_required(VERSION 3.5)
project(Ren)

OPTION(ENABLE_GPU_DEBUG "Enable GPU debug markers" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_definitions(-D_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR)

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
if(${REN_BACKEND} STREQUAL "GL")
    set(LIBS GL)
elseif(${REN_BACKEND} STREQUAL "VK")
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

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Config.h.in ${CMAKE_CURRENT_SOURCE_DIR}/Config.h)

set(SOURCE_FILES AccStructure.h
                 AccStructure.cpp
                 AlignedAlloc.h
                 Anim.h
                 Anim.cpp
                 ApiContext.h
                 Bitmask.h
                 Buffer.h
                 Buffer.cpp
                 Camera.h
                 Camera.cpp
                 Common.h
                 Config.h
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
                 RastState.cpp
                 RenderPass.h
                 RenderPass.cpp
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
                 TextureCompare.inl
                 TextureFilter.inl
                 TextureFormat.inl
                 TextureParams.h
                 TextureParams.cpp
                 TextureRegion.h
                 TextureRegion.cpp
                 TextureSplitter.h
                 TextureSplitter.cpp
                 TextureWrap.inl
                 Types.inl
                 Utils.h
                 Utils.cpp
                 Utils_AVX.cpp
                 Utils_SSE2.cpp
                 Utils_NEON.cpp
                 VertexInput.h)

if(${REN_BACKEND} STREQUAL "GL")
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
elseif(${REN_BACKEND} STREQUAL "SW")
    set(SOURCE_FILES ${SOURCE_FILES}
                     BufferSW.cpp
                     ContextSW.cpp
                     ProgramSW.h
                     ProgramSW.cpp
                     TextureSW.h
                     TextureSW.cpp)
elseif(${REN_BACKEND} STREQUAL "VK")
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
if(${REN_BACKEND} STREQUAL "GL")
    target_compile_definitions(Ren PUBLIC REN_GL_BACKEND)
elseif(${REN_BACKEND} STREQUAL "VK")
    target_compile_definitions(Ren PUBLIC REN_VK_BACKEND)
    if(WIN32)
        target_compile_definitions(Ren PUBLIC VK_USE_PLATFORM_WIN32_KHR)
    else(WIN32)
        if(APPLE)
            target_compile_definitions(Ren PUBLIC VK_USE_PLATFORM_MACOS_MVK)
        else(APPLE)
            target_compile_definitions(Ren PUBLIC VK_USE_PLATFORM_XLIB_KHR)
        endif(APPLE)
    endif(WIN32)
elseif(${REN_BACKEND} STREQUAL "SW")
    target_compile_definitions(Ren PUBLIC REN_SW_BACKEND)
endif()
target_link_libraries(Ren SW ${LIBS})
target_include_directories(Ren PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third-party)

add_subdirectory(tests)
