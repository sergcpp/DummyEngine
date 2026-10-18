cmake_minimum_required(VERSION 3.10)
project(Ren)

OPTION(REN_ENABLE_GPU_DEBUG "Enable GPU debug markers" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_definitions(-D_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR)

find_library(METAL_LIB Metal)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release;RelWithDebInfo;Asan" CACHE STRING "" FORCE)

IF(MSVC)
    if (NOT DEFINED CMAKE_CXX_FLAGS_ASAN)
        set(CMAKE_CXX_FLAGS_ASAN "/MTd /Zi /Od /RTC1 /fsanitize=address /analyze /WX")
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
                 Anim.h
                 Anim.cpp
                 ApiContext.h
                 Bindless.h
                 BlendFactor.inl
                 BlendOp.inl
                 Buffer.h
                 Buffer.cpp
                 Camera.h
                 Camera.cpp
                 Common.h
                 CompareOp.inl
                 Config.h
                 Context.h
                 Context.cpp
                 DebugMarker.h
                 DescriptorPool.h
                 DrawCall.h
                 MemoryAllocator.h
                 Fence.h
                 Filter.inl
                 Fixed.h
                 Format.inl
                 Framebuffer.h
                 Fwd.h
                 Image.h
                 Image.cpp
                 ImageAtlas.h
                 ImageAtlas.cpp
                 ImageParams.h
                 ImageParams.cpp
                 ImageRegion.h
                 ImageRegion.cpp
                 Log.h
                 Material.h
                 Material.cpp
                 Mesh.h
                 Mesh.cpp
                 Pipeline.h
                 Program.h
                 RastState.h
                 RastState.cpp
                 RenderPass.h
                 RenderPass.cpp
                 ResizableBuffer.h
                 ResizableBuffer.cpp
                 Resource.h
                 Resource.cpp
                 Sampler.h
                 SamplingParams.h
                 Shader.h
                 StencilOp.inl
                 Types.inl
                 VertexInput.h
                 Wrap.inl)

set(MATH_SOURCE_FILES math/Mat.h
                      math/Quat.h
                      math/Vec.h)

set(UTILS_SOURCE_FILES utils/AlignedAlloc.h
                       utils/Bitmask.h
                       utils/CPUFeatures.h
                       utils/CPUFeatures.cpp
                       utils/FreelistAlloc.h
                       utils/FreelistAlloc.cpp
                       utils/HashMap32.h
                       utils/HashSet32.h
                       utils/ImageSplitter.h
                       utils/ImageSplitter.cpp
                       utils/RingBuffer.h
                       utils/SmallVector.h
                       utils/Span.h
                       utils/SparseArray.h
                       utils/SparseStorage.h
                       utils/String.h
                       utils/Utils.h
                       utils/Utils.cpp
                       utils/Utils_AVX.cpp
                       utils/Utils_SSE2.cpp
                       utils/Utils_NEON.cpp)

set(GL_SOURCE_FILES Gl/BufferGL.cpp
                    Gl/ContextGL.cpp
                    Gl/DebugMarkerGL.cpp
                    Gl/DescriptorPoolGL.cpp
                    Gl/DrawCallGL.cpp
                    Gl/FenceGL.cpp
                    Gl/FramebufferGL.cpp
                    Gl/GL.h
                    Gl/GLCtx.h
                    Gl/GLCtx.cpp
                    Gl/GLExt.cpp
                    Gl/GLExtDSAEmu.h
                    Gl/GLExtDSAEmu.cpp
                    Gl/ImageGL.h
                    Gl/ImageGL.cpp
                    Gl/ImageAtlasGL.cpp
                    Gl/MemoryAllocatorGL.cpp
                    Gl/PipelineGL.cpp
                    Gl/ProgramGL.h
                    Gl/ProgramGL.cpp
                    Gl/RastStateGL.cpp
                    Gl/RenderPassGL.cpp
                    Gl/ResourceGL.cpp
                    Gl/SamplerGL.h
                    Gl/SamplerGL.cpp
                    Gl/ShaderGL.h
                    Gl/ShaderGL.cpp
                    Gl/VertexInputGL.cpp)

set(VK_SOURCE_FILES Vk/AccStructureVK.cpp
                    Vk/BufferVK.cpp
                    Vk/ContextVK.cpp
                    Vk/DebugMarkerVK.cpp
                    Vk/DescriptorPoolVK.cpp
                    Vk/DrawCallVK.cpp
                    Vk/FenceVK.cpp
                    Vk/FramebufferVK.cpp
                    Vk/ImageVK.h
                    Vk/ImageVK.cpp
                    Vk/ImageAtlasVK.cpp
                    Vk/MemoryAllocatorVK.cpp
                    Vk/PipelineVK.cpp
                    Vk/ProgramVK.h
                    Vk/ProgramVK.cpp
                    Vk/RenderPassVK.cpp
                    Vk/ResourceVK.cpp
                    Vk/SamplerVK.h
                    Vk/SamplerVK.cpp
                    Vk/ShaderVK.h
                    Vk/ShaderVK.cpp
                    Vk/VK.h
                    Vk/VKCtx.h
                    Vk/VKCtx.cpp
                    Vk/VertexInputVK.cpp)

if(${REN_BACKEND} STREQUAL "GL")
    if(WIN32)
        set(LIBS ${LIBS} opengl32 SPIRV-Reflect)
    else(WIN32)
        set(LIBS ${LIBS} SPIRV-Reflect)
    ENDIF(WIN32)
elseif(${REN_BACKEND} STREQUAL "VK")
    set(LIBS ${LIBS} SPIRV-Reflect)
endif()

add_subdirectory(SW)
add_subdirectory(third-party/SPIRV-Reflect)

list(APPEND REN_SOURCE_FILES ${SOURCE_FILES})
source_group("src" FILES ${SOURCE_FILES})

list(APPEND REN_SOURCE_FILES ${MATH_SOURCE_FILES})
source_group("src\\math" FILES ${MATH_SOURCE_FILES})

list(APPEND REN_SOURCE_FILES ${UTILS_SOURCE_FILES})
source_group("src\\utils" FILES ${UTILS_SOURCE_FILES})

if(${REN_BACKEND} STREQUAL "GL")
    list(APPEND REN_SOURCE_FILES ${GL_SOURCE_FILES})
    source_group("src\\Gl" FILES ${GL_SOURCE_FILES})
elseif(${REN_BACKEND} STREQUAL "VK")
    list(APPEND REN_SOURCE_FILES ${VK_SOURCE_FILES})
    source_group("src\\Vk" FILES ${VK_SOURCE_FILES})
endif()

if(NOT CMAKE_GENERATOR_PLATFORM MATCHES "ARM64")
    if(MSVC)
        if(NOT CMAKE_CL_64)
            set_source_files_properties(utils/Utils_SSE2.cpp PROPERTIES COMPILE_FLAGS /arch:SSE2)
        endif()
        if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            set_source_files_properties(utils/Utils_SSE2.cpp PROPERTIES COMPILE_FLAGS -msse4.1)
            set_source_files_properties(utils/Utils_AVX.cpp PROPERTIES COMPILE_FLAGS "-mavx2 -mfma")
        else()
            set_source_files_properties(utils/Utils_AVX.cpp PROPERTIES COMPILE_FLAGS /arch:AVX)
        endif()
        list(APPEND REN_SOURCE_FILES _CustomTypes.natvis)
    else(MSVC)
        set_source_files_properties(utils/Utils_SSE2.cpp PROPERTIES COMPILE_FLAGS -msse2)
        set_source_files_properties(utils/Utils_AVX.cpp PROPERTIES COMPILE_FLAGS "-mavx2 -mfma")
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
endif()
target_link_libraries(Ren SW ${LIBS})
target_include_directories(Ren PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third-party)

add_subdirectory(tests)
