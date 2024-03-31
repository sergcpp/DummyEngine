cmake_minimum_required(VERSION 3.1)
project(Eng)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release;RelWithDebInfo;Asan;Tsan" CACHE STRING "" FORCE)
set_property(GLOBAL PROPERTY DEBUG_CONFIGURATIONS "Debug;Asan")

if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
    if (NOT DEFINED CMAKE_CXX_FLAGS_ASAN OR CMAKE_CXX_FLAGS_ASAN STREQUAL "")
        set(CMAKE_CXX_FLAGS_ASAN "/MP /MDd /Zi /Od /RTC1 /fsanitize=address")
        set(CMAKE_C_FLAGS_ASAN "/MP /MDd /Zi /Od /RTC1 /fsanitize=address")
        set(CMAKE_EXE_LINKER_FLAGS_ASAN "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /INCREMENTAL:NO")
    endif()
    if (NOT DEFINED CMAKE_CXX_FLAGS_TSAN OR CMAKE_CXX_FLAGS_TSAN STREQUAL "")
        set(CMAKE_CXX_FLAGS_TSAN "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
        set(CMAKE_C_FLAGS_TSAN "${CMAKE_C_FLAGS_RELWITHDEBINFO}")
        set(CMAKE_EXE_LINKER_FLAGS_TSAN "${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO}")
    endif()
else(MSVC)
    if (NOT DEFINED CMAKE_CXX_FLAGS_TSAN OR CMAKE_CXX_FLAGS_TSAN STREQUAL "")
        set(CMAKE_CXX_FLAGS_TSAN "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -fsanitize=address")
        set(CMAKE_C_FLAGS_TSAN "${CMAKE_C_FLAGS_RELWITHDEBINFO} -fsanitize=address")
        set(CMAKE_EXE_LINKER_FLAGS_TSAN "${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO} -fsanitize=address")
    endif()
endif(MSVC)

IF(WIN32)
    set(VPX_LIB "vpxmd")
    set(ITT_LIB "libittnotify")
    set(GLSLANG_LIBS debug GenericCodeGend                  optimized GenericCodeGen
                     debug glslangd                         optimized glslang
                     debug glslang-default-resource-limitsd optimized glslang-default-resource-limits
                     debug MachineIndependentd              optimized MachineIndependent
                     debug OGLCompilerd                     optimized OGLCompiler
                     debug OSDependentd                     optimized OSDependent
                     debug SPIRVd                           optimized SPIRV
                     debug SPIRV-Toolsd                     optimized SPIRV-Tools
                     debug SPIRV-Tools-optd                 optimized SPIRV-Tools-opt)
ELSE(WIN32)
    set(VPX_LIB "vpx")
    if(NOT CMAKE_SYSTEM_NAME MATCHES "Android" AND NOT CMAKE_APPLE_SILICON_PROCESSOR MATCHES "arm64")
        set(ITT_LIB "ittnotify")
    endif()
    set(GLSLANG_LIBS glslang
                     glslang-default-resource-limits
                     MachineIndependent
                     GenericCodeGen
                     OGLCompiler
                     OSDependent
                     SPIRV
                     SPIRV-Tools-opt
                     SPIRV-Tools
                     X11)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
ENDIF(WIN32)

set(SOURCE_FILES Config.h
                 Eng.h
                 Eng.cpp
                 FrameInfo.h
                 Log.h
                 Log.cpp
                 ViewerBase.h
                 ViewerBase.cpp
                 ViewerState.h
                 ViewerStateManager.h
                 ViewerStateManager.cpp)

set(GUI_SOURCE_FILES    gui/BaseElement.h
                        gui/BaseElement.cpp
                        gui/BitmapFont.h
                        gui/BitmapFont.cpp
                        gui/ButtonBase.h
                        gui/ButtonBase.cpp
                        gui/ButtonImage.h
                        gui/ButtonImage.cpp
                        gui/ButtonText.h
                        gui/ButtonText.cpp
                        gui/Cursor.h
                        gui/Cursor.cpp
                        gui/EditBox.h
                        gui/EditBox.cpp
                        gui/Image.h
                        gui/Image.cpp
                        gui/Image9Patch.h
                        gui/Image9Patch.cpp
                        gui/LinearLayout.h
                        gui/LinearLayout.cpp
                        gui/Renderer.h
                        gui/Renderer.cpp
                        gui/shaders.inl
                        gui/TypeMesh.h
                        gui/TypeMesh.cpp
                        gui/Utils.h
                        gui/Utils.cpp)

if(${RENDERER} STREQUAL "GL")
    set(GUI_SOURCE_FILES    ${GUI_SOURCE_FILES}
                            gui/RendererGL.cpp
                            )
elseif(${RENDERER} STREQUAL "SW")
    set(GUI_SOURCE_FILES    ${GUI_SOURCE_FILES}
                            gui/RendererSW.cpp
                            )
elseif(${RENDERER} STREQUAL "VK")
    set(GUI_SOURCE_FILES    ${GUI_SOURCE_FILES}
                            gui/RendererVK.cpp)
endif()

set(INPUT_SOURCE_FILES input/InputManager.h
                       input/InputManager.cpp
                       input/Keycode.h)

set(RENDERER_SOURCES    renderer/CDFUtils.h
                        renderer/CDFUtils.cpp
                        renderer/PrimDraw.h
                        renderer/PrimDraw.cpp
                        renderer/Renderer.h
                        renderer/Renderer.cpp
                        renderer/Renderer_BN.cpp
                        renderer/Renderer_DrawList.h
                        renderer/Renderer_DrawList.cpp
                        renderer/Renderer_Frontend.cpp
                        renderer/Renderer_Gen.cpp
                        renderer/Renderer_GIDiffuse.cpp
                        renderer/Renderer_GISpecular.cpp
                        renderer/Renderer_Names.h
                        renderer/Renderer_Passes.cpp
                        renderer/Renderer_Structs.h
                        renderer/Renderer_SunShadows.cpp)

set(SCENE_SOURCES   scene/__cam_rig.inl
                    scene/PhysicsManager.h
                    scene/PhysicsManager.cpp
                    scene/SceneData.h
                    scene/SceneData.cpp
                    scene/SceneManager.h
                    scene/SceneManager.cpp
                    scene/SceneManager_ASS.cpp
                    scene/SceneManager_ASS_Font.cpp
                    scene/SceneManager_ASS_Shader.cpp
                    scene/SceneManager_ASS_Tex.cpp
                    scene/SceneManager_BVH.cpp
                    scene/SceneManager_Tex.cpp
                    scene/TexUpdateFileBuf.h)

set(GRAPH_SOURCES   renderer/graph/GraphBuilder.h
                    renderer/graph/GraphBuilder.cpp
                    renderer/graph/SubPass.h
                    renderer/graph/SubPass.cpp
                    renderer/graph/Resource.h)

set(PASS_SOURCES    renderer/passes/RpBuildAccStructures.h
                    renderer/passes/RpBuildAccStructures.cpp
                    renderer/passes/RpCombine.h
                    renderer/passes/RpCombine.cpp
                    renderer/passes/RpDebugRT.h
                    renderer/passes/RpDebugRT.cpp
                    renderer/passes/RpDepthFill.h
                    renderer/passes/RpDepthFill.cpp
                    renderer/passes/RpDepthHierarchy.h
                    renderer/passes/RpDepthHierarchy.cpp
                    renderer/passes/RpGBufferFill.h
                    renderer/passes/RpGBufferFill.cpp
                    renderer/passes/RpOpaque.h
                    renderer/passes/RpOpaque.cpp
                    renderer/passes/RpReadBrightness.h
                    renderer/passes/RpReadBrightness.cpp
                    renderer/passes/RpRTGI.h
                    renderer/passes/RpRTGI.cpp
                    renderer/passes/RpRTReflections.h
                    renderer/passes/RpRTReflections.cpp
                    renderer/passes/RpRTShadows.h
                    renderer/passes/RpRTShadows.cpp
                    renderer/passes/RpSampleBrightness.h
                    renderer/passes/RpSampleBrightness.cpp
                    renderer/passes/RpShadowMaps.h
                    renderer/passes/RpShadowMaps.cpp
                    renderer/passes/RpSkinning.h
                    renderer/passes/RpSkinning.cpp
                    renderer/passes/RpSkydome.h
                    renderer/passes/RpSkydome.cpp
                    renderer/passes/RpSSRCompose.h
                    renderer/passes/RpSSRCompose.cpp
                    renderer/passes/RpSSRCompose2.h
                    renderer/passes/RpSSRCompose2.cpp
                    renderer/passes/RpTransparent.h
                    renderer/passes/RpTransparent.cpp
                    renderer/passes/RpUpdateAccBuffers.h
                    renderer/passes/RpUpdateAccBuffers.cpp)

if(${RENDERER} STREQUAL "GL")
    set(RENDERER_SOURCES ${RENDERER_SOURCES}
                        renderer/PrimDrawGL.cpp)
    set(SCENE_SOURCES ${SCENE_SOURCES}
                        scene/SceneManagerGL.cpp)
    set(GRAPH_SOURCES ${GRAPH_SOURCES} renderer/graph/GraphBuilderGL.cpp)
    set(PASS_SOURCES ${PASS_SOURCES}
                        renderer/passes/RpDebugRT_GL.cpp
                        renderer/passes/RpDebugProbes.h         # GL-only for now
                        renderer/passes/RpDebugProbes.cpp       # GL-only for now
                        renderer/passes/RpDebugTextures.h       # GL-only for now
                        renderer/passes/RpDebugTextures.cpp     # GL-only for now
                        renderer/passes/RpDebugTexturesGL.cpp
                        renderer/passes/RpDebugEllipsoids.h     # GL-only for now
                        renderer/passes/RpDebugEllipsoids.cpp   # GL-only for now
                        renderer/passes/RpDepthFillGL.cpp
                        renderer/passes/RpDepthHierarchyGL.cpp
                        renderer/passes/RpDOF.h                 # GL-only for now
                        renderer/passes/RpDOF.cpp               # GL-only for now
                        renderer/passes/RpFXAA.h                # GL-only for now
                        renderer/passes/RpFXAA.cpp              # GL-only for now
                        renderer/passes/RpGBufferFillGL.cpp
                        renderer/passes/RpOpaqueGL.cpp
                        renderer/passes/RpRTGI_GL.cpp
                        renderer/passes/RpRTReflections_GL.cpp
                        renderer/passes/RpRTShadows_GL.cpp
                        renderer/passes/RpShadowMapsGL.cpp
                        renderer/passes/RpSkinningGL.cpp
                        renderer/passes/RpSkydomeGL.cpp
                        renderer/passes/RpTransparentGL.cpp
                        renderer/passes/RpUpdateAccBuffersGL.cpp)
elseif(${RENDERER} STREQUAL "VK")
    set(RENDERER_SOURCES ${RENDERER_SOURCES}
                        renderer/PrimDrawVK.cpp)
    set(SCENE_SOURCES ${SCENE_SOURCES}
                        scene/SceneManagerVK.cpp)
    set(GRAPH_SOURCES ${GRAPH_SOURCES} renderer/graph/GraphBuilderVK.cpp)
    set(PASS_SOURCES ${PASS_SOURCES}
                        renderer/passes/RpBuildAccStructuresVK.cpp
                        renderer/passes/RpDebugRT_VK.cpp
                        renderer/passes/RpDepthFillVK.cpp
                        renderer/passes/RpDepthHierarchyVK.cpp
                        renderer/passes/RpGBufferFillVK.cpp
                        renderer/passes/RpOpaqueVK.cpp
                        renderer/passes/RpRTGI_VK.cpp
                        renderer/passes/RpRTReflections_VK.cpp
                        renderer/passes/RpRTShadows_VK.cpp
                        renderer/passes/RpShadowMapsVK.cpp
                        renderer/passes/RpSkinningVK.cpp
                        renderer/passes/RpSkydomeVK.cpp
                        renderer/passes/RpTransparentVK.cpp
                        renderer/passes/RpUpdateAccBuffersVK.cpp)
elseif(${RENDERER} STREQUAL "SW")
    set(RENDERER_SOURCES ${RENDERER_SOURCES}
                        renderer/RendererSW.cpp)
endif()

file(GLOB RENDERER_SHADERS
     "renderer/shaders/*.h"
     "renderer/shaders/*.inl"
     "renderer/shaders/*.glsl"
)

set(SCENE_COMP_SOURCES  scene/comp/Common.h
                        scene/comp/AccStructure.h
                        scene/comp/AccStructure.cpp
                        scene/comp/AnimState.h
                        scene/comp/AnimState.cpp
                        scene/comp/Decal.h
                        scene/comp/Decal.cpp
                        scene/comp/Drawable.h
                        scene/comp/Drawable.cpp
                        scene/comp/Lightmap.h
                        scene/comp/Lightmap.cpp
                        scene/comp/LightProbe.h
                        scene/comp/LightProbe.cpp
                        scene/comp/LightSource.h
                        scene/comp/LightSource.cpp
                        scene/comp/Occluder.h
                        scene/comp/Occluder.cpp
                        scene/comp/Physics.h
                        scene/comp/Physics.cpp
                        scene/comp/SoundSource.h
                        scene/comp/SoundSource.cpp
                        scene/comp/Transform.h
                        scene/comp/Transform.cpp
                        scene/comp/VegState.h
                        scene/comp/VegState.cpp)

set(UTILS_SOURCES utils/BVHSplit.h
                  utils/BVHSplit.cpp
                  utils/Cmdline.h
                  utils/Cmdline.cpp
                  utils/FlowControl.h
                  utils/FlowControl.cpp
                  utils/FreeCamController.h
                  utils/FreeCamController.cpp
                  utils/Load.h
                  utils/Load.cpp
                  utils/Random.h
                  utils/Random.cpp
                  utils/ScriptedDialog.h
                  utils/ScriptedDialog.cpp
				  utils/ScriptedSequence.h
				  utils/ScriptedSequence.cpp
                  utils/ShaderLoader.h
                  utils/ShaderLoader.cpp
                  utils/ShaderLoaderGLSL.cpp
                  utils/ShaderLoaderSPIRV.cpp
                  utils/VideoPlayer.h
                  utils/VideoPlayer.cpp)

list(APPEND ALL_SOURCE_FILES ${SOURCE_FILES})
source_group("src" FILES ${SOURCE_FILES})

list(APPEND ALL_SOURCE_FILES ${GUI_SOURCE_FILES})
source_group("src\\Gui" FILES ${GUI_SOURCE_FILES})

list(APPEND ALL_SOURCE_FILES ${INPUT_SOURCE_FILES})
source_group("src\\input" FILES ${INPUT_SOURCE_FILES})

list(APPEND ALL_SOURCE_FILES ${RENDERER_SOURCES})
source_group("src\\renderer" FILES ${RENDERER_SOURCES})

list(APPEND ALL_SOURCE_FILES ${GRAPH_SOURCES})
source_group("src\\renderer\\graph" FILES ${GRAPH_SOURCES})

list(APPEND ALL_SOURCE_FILES ${PASS_SOURCES})
source_group("src\\renderer\\passes" FILES ${PASS_SOURCES})

list(APPEND ALL_SOURCE_FILES ${RENDERER_SHADERS})
source_group("src\\renderer\\shaders" FILES ${RENDERER_SHADERS})

list(APPEND ALL_SOURCE_FILES ${SCENE_SOURCES})
source_group("src\\scene" FILES ${SCENE_SOURCES})

list(APPEND ALL_SOURCE_FILES ${SCENE_COMP_SOURCES})
source_group("src\\scene\\comp" FILES ${SCENE_COMP_SOURCES})

list(APPEND ALL_SOURCE_FILES ${UTILS_SOURCES})
source_group("src\\utils" FILES ${UTILS_SOURCES})

add_library(Eng STATIC ${ALL_SOURCE_FILES})
target_include_directories(Eng PUBLIC first-party)
target_include_directories(Eng PUBLIC third-party)
target_include_directories(Eng PUBLIC third-party/glslang/include)
target_include_directories(Eng PUBLIC third-party/vpx/include)
if (WIN32)
    target_link_directories(Eng PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third-party/glslang/win)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        target_link_directories(Eng PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third-party/vtune/win/x64)
        target_link_directories(Eng PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third-party/vpx/win/Release/x64)
    else(CMAKE_SIZEOF_VOID_P EQUAL 8)
        target_link_directories(Eng PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third-party/vtune/win/x86)
        target_link_directories(Eng PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third-party/vpx/win/Release/x86)
    endif()
else(WIN32)
    if (APPLE)
        target_link_directories(Eng PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third-party/glslang/macos)
        target_link_directories(Eng PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third-party/vtune/osx/x64)
        if(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "arm64")
            target_link_directories(Eng PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third-party/vpx/osx/arm64)
        else()
            target_link_directories(Eng PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third-party/vpx/osx/x64)
        endif()
    else(APPLE)
        target_link_directories(Eng PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third-party/glslang/linux)
        target_link_directories(Eng PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third-party/vtune/linux/x64)
        target_link_directories(Eng PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third-party/vpx/linux/x64)
    endif(APPLE)
endif(WIN32)
target_link_libraries(Eng Net Phy Ren Snd Sys astc optick glslx_lib ${GLSLANG_LIBS} ${ITT_LIB} ${VPX_LIB})

add_subdirectory(tests)
add_subdirectory(first-party/Net)
add_subdirectory(first-party/Phy)
add_subdirectory(first-party/Ren)
add_subdirectory(first-party/Sys)
add_subdirectory(first-party/Snd)
add_subdirectory(first-party/glslx)
add_subdirectory(third-party/ltc_fit)
add_subdirectory(third-party/astc)
add_subdirectory(third-party/optick)
target_include_directories(optick PUBLIC first-party/Ren/third-party)
set_target_properties(glslx PROPERTIES EXCLUDE_FROM_ALL 1 EXCLUDE_FROM_DEFAULT_BUILD 1)