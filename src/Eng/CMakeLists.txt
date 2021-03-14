cmake_minimum_required(VERSION 2.8.4)
project(Eng)

IF(WIN32)
    set(VPX_LIB "vpxmd")
    set(ITT_LIB "libittnotify")
ELSE(WIN32)
    set(VPX_LIB "vpx")
    set(ITT_LIB "ittnotify")
    set(CMAKE_CXX_STANDARD 11)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
ENDIF(WIN32)

set(SOURCE_FILES Config.h
                 FlowControl.h
                 FlowControl.cpp
                 FrameInfo.h
                 GameBase.h
                 GameBase.cpp
                 GameState.h
                 GameStateManager.h
                 GameStateManager.cpp
                 Log.h
                 Log.cpp
                 ObjectPool.h
                 Random.h
                 Random.cpp)

set(GUI_SOURCE_FILES    Gui/BaseElement.h
                        Gui/BaseElement.cpp
                        Gui/BitmapFont.h
                        Gui/BitmapFont.cpp
                        Gui/ButtonBase.h
                        Gui/ButtonBase.cpp
                        Gui/ButtonImage.h
                        Gui/ButtonImage.cpp
                        Gui/ButtonText.h
                        Gui/ButtonText.cpp
                        Gui/Cursor.h
                        Gui/Cursor.cpp
                        Gui/EditBox.h
                        Gui/EditBox.cpp
                        Gui/Image.h
                        Gui/Image.cpp
                        Gui/Image9Patch.h
                        Gui/Image9Patch.cpp
                        Gui/LinearLayout.h
                        Gui/LinearLayout.cpp
                        Gui/Renderer.h
                        Gui/Renderer.cpp
                        Gui/TypeMesh.h
                        Gui/TypeMesh.cpp
                        Gui/Utils.h
                        Gui/Utils.cpp)

if(${RENDERER} STREQUAL "GL")
    set(GUI_SOURCE_FILES    ${GUI_SOURCE_FILES}
                            Gui/RendererGL.cpp
                            )
elseif(${RENDERER} STREQUAL "SW")
    set(GUI_SOURCE_FILES    ${GUI_SOURCE_FILES}
                            Gui/RendererSW.cpp
                            )
elseif(${RENDERER} STREQUAL "VK")
    set(GUI_SOURCE_FILES    ${GUI_SOURCE_FILES}
                            )
endif()

set(INPUT_SOURCE_FILES Input/InputManager.h
                       Input/InputManager.cpp
                       Input/Keycode.h)

set(RENDERER_SOURCES    Renderer/DebugMarker.h
                        Renderer/FrameBuf.h
                        Renderer/PrimDraw.h
                        Renderer/PrimDrawGL.cpp
                        Renderer/Renderer.h
                        Renderer/Renderer.cpp
                        Renderer/Renderer_DrawList.h
                        Renderer/Renderer_DrawList.cpp
                        Renderer/Renderer_Frontend.cpp
                        Renderer/Renderer_Gen.cpp
                        Renderer/Renderer_Names.h
                        Renderer/Renderer_Structs.h
                        Renderer/__brdf_lut.inl
                        Renderer/__cone_rt_lut.inl
                        Renderer/__noise.inl
                        Renderer/__sphere_mesh.inl)

set(SCENE_SOURCES   Scene/__cam_rig.inl
                    Scene/SceneData.h
                    Scene/SceneData.cpp
                    Scene/SceneManager.h
                    Scene/SceneManager.cpp
                    Scene/SceneManager_ASS.cpp
                    Scene/SceneManager_ASS_Font.cpp
                    Scene/SceneManager_ASS_Shader.cpp
                    Scene/SceneManager_ASS_Tex.cpp
                    Scene/SceneManager_BVH.cpp
                    Scene/SceneManager_PT.cpp
                    Scene/SceneManager_Tex.cpp
                    Scene/ProbeStorage.h)

set(GRAPH_SOURCES   Renderer/Graph/GraphBuilder.h
                    Renderer/Graph/GraphBuilder.cpp)

set(PASS_SOURCES   Renderer/Passes/RpBlur.h
                   Renderer/Passes/RpBlur.cpp
                   Renderer/Passes/RpCombine.h
                   Renderer/Passes/RpCombine.cpp
                   Renderer/Passes/RpDebugEllipsoids.h
                   Renderer/Passes/RpDebugEllipsoids.cpp
                   Renderer/Passes/RpDebugProbes.h
                   Renderer/Passes/RpDebugProbes.cpp
                   Renderer/Passes/RpDebugTextures.h
                   Renderer/Passes/RpDebugTextures.cpp
                   Renderer/Passes/RpDepthFill.h
                   Renderer/Passes/RpDepthFill.cpp
                   Renderer/Passes/RpDOF.h
                   Renderer/Passes/RpDOF.cpp
                   Renderer/Passes/RpDownColor.h
                   Renderer/Passes/RpDownColor.cpp
                   Renderer/Passes/RpDownDepth.h
                   Renderer/Passes/RpDownDepth.cpp
                   Renderer/Passes/RpFXAA.h
                   Renderer/Passes/RpFXAA.cpp
                   Renderer/Passes/RpInsertFence.h
                   Renderer/Passes/RpInsertFence.cpp
                   Renderer/Passes/RpOpaque.h
                   Renderer/Passes/RpOpaque.cpp
                   Renderer/Passes/RpReflections.h
                   Renderer/Passes/RpReflections.cpp
                   Renderer/Passes/RpResolve.h
                   Renderer/Passes/RpResolve.cpp
                   Renderer/Passes/RpSampleBrightness.h
                   Renderer/Passes/RpSampleBrightness.cpp
                   Renderer/Passes/RpShadowMaps.h
                   Renderer/Passes/RpShadowMaps.cpp
                   Renderer/Passes/RpSkinning.h
                   Renderer/Passes/RpSkinning.cpp
                   Renderer/Passes/RpSkydome.h
                   Renderer/Passes/RpSkydome.cpp
                   Renderer/Passes/RpSSAO.h
                   Renderer/Passes/RpSSAO.cpp
                   Renderer/Passes/RpTAA.h
                   Renderer/Passes/RpTAA.cpp
                   Renderer/Passes/RpTransparent.h
                   Renderer/Passes/RpTransparent.cpp
                   Renderer/Passes/__skydome_mesh.inl
                   Renderer/Passes/RpUpdateBuffers.h
                   Renderer/Passes/RpUpdateBuffers.cpp)

if(${RENDERER} STREQUAL "GL")
    set(RENDERER_SOURCES ${RENDERER_SOURCES}
                        Renderer/FrameBufGL.cpp
                        Renderer/Renderer_Backend_GLES3.cpp
                        Renderer/Renderer_GL_Defines.inl)
    set(SCENE_SOURCES ${SCENE_SOURCES}
                        Scene/ProbeStorageGL.cpp)
    set(PASS_SOURCES ${PASS_SOURCES}
                        Renderer/Passes/RpDebugTexturesGL.cpp
                        Renderer/Passes/RpDepthFillGL.cpp
                        Renderer/Passes/RpInsertFenceGL.cpp
                        Renderer/Passes/RpOpaqueGL.cpp
                        Renderer/Passes/RpSampleBrightnessGL.cpp
                        Renderer/Passes/RpShadowMapsGL.cpp
                        Renderer/Passes/RpSkinningGL.cpp
                        Renderer/Passes/RpSkydomeGL.cpp
                        Renderer/Passes/RpTransparentGL.cpp
                        Renderer/Passes/RpUpdateBuffersGL.cpp)
elseif(${RENDERER} STREQUAL "SW")
    set(RENDERER_SOURCES ${RENDERER_SOURCES}
                        Renderer/RendererSW.cpp)
endif()

set(SHADERS_SOURCES Renderer/Shaders/_common.glsl
                    Renderer/Shaders/_fs_common.glsl
                    Renderer/Shaders/_vs_common.glsl)

set(SCENE_COMP_SOURCES
        Scene/Comp/Common.h
        Scene/Comp/AnimState.h
        Scene/Comp/AnimState.cpp
        Scene/Comp/Decal.h
        Scene/Comp/Decal.cpp
        Scene/Comp/Drawable.h
        Scene/Comp/Drawable.cpp
        Scene/Comp/Lightmap.h
        Scene/Comp/Lightmap.cpp
        Scene/Comp/LightProbe.h
        Scene/Comp/LightProbe.cpp
        Scene/Comp/LightSource.h
        Scene/Comp/LightSource.cpp
        Scene/Comp/Occluder.h
        Scene/Comp/Occluder.cpp
        Scene/Comp/SoundSource.h
        Scene/Comp/SoundSource.cpp
        Scene/Comp/Transform.h
        Scene/Comp/Transform.cpp
        Scene/Comp/VegState.h
        Scene/Comp/VegState.cpp)

set(UTILS_SOURCES Utils/BVHSplit.h
                  Utils/BVHSplit.cpp
                  Utils/Cmdline.h
                  Utils/Cmdline.cpp
                  Utils/FreeCamController.h
                  Utils/FreeCamController.cpp
                  Utils/Load.h
                  Utils/Load.cpp
                  Utils/ScriptedDialog.h
                  Utils/ScriptedDialog.cpp
				  Utils/ScriptedSequence.h
				  Utils/ScriptedSequence.cpp
                  Utils/ShaderLoader.h
                  Utils/ShaderLoaderGLSL.cpp
                  Utils/VideoPlayer.h
                  Utils/VideoPlayer.cpp)

list(APPEND ALL_SOURCE_FILES ${SOURCE_FILES})
source_group("src" FILES ${SOURCE_FILES})

list(APPEND ALL_SOURCE_FILES ${GUI_SOURCE_FILES})
source_group("src\\Gui" FILES ${GUI_SOURCE_FILES})

list(APPEND ALL_SOURCE_FILES ${INPUT_SOURCE_FILES})
source_group("src\\Input" FILES ${INPUT_SOURCE_FILES})

list(APPEND ALL_SOURCE_FILES ${RENDERER_SOURCES})
source_group("src\\Renderer" FILES ${RENDERER_SOURCES})

list(APPEND ALL_SOURCE_FILES ${GRAPH_SOURCES})
source_group("src\\Renderer\\Graph" FILES ${GRAPH_SOURCES})

list(APPEND ALL_SOURCE_FILES ${PASS_SOURCES})
source_group("src\\Renderer\\Passes" FILES ${PASS_SOURCES})

list(APPEND ALL_SOURCE_FILES ${SHADERS_SOURCES})
source_group("src\\Renderer\\Shaders" FILES ${SHADERS_SOURCES})

list(APPEND ALL_SOURCE_FILES ${SCENE_SOURCES})
source_group("src\\Scene" FILES ${SCENE_SOURCES})

list(APPEND ALL_SOURCE_FILES ${SCENE_COMP_SOURCES})
source_group("src\\Scene\\Comp" FILES ${SCENE_COMP_SOURCES})

list(APPEND ALL_SOURCE_FILES ${UTILS_SOURCES})
source_group("src\\Utils" FILES ${UTILS_SOURCES})

# enable or disable unity build
if(ENABLE_UNITYBUILD)
    set_source_files_properties(${SOURCE_FILES} PROPERTIES HEADER_FILE_ONLY TRUE)
    set_source_files_properties(${GUI_SOURCE_FILES} PROPERTIES HEADER_FILE_ONLY TRUE)
    set_source_files_properties(${INPUT_SOURCE_FILES} PROPERTIES HEADER_FILE_ONLY TRUE)
    set_source_files_properties(${RENDERER_SOURCES} PROPERTIES HEADER_FILE_ONLY TRUE)
    set_source_files_properties(${GRAPH_SOURCES} PROPERTIES HEADER_FILE_ONLY TRUE)
    set_source_files_properties(${PASS_SOURCES} PROPERTIES HEADER_FILE_ONLY TRUE)
    set_source_files_properties(${SHADERS_SOURCES} PROPERTIES HEADER_FILE_ONLY TRUE)
    set_source_files_properties(${SCENE_SOURCES} PROPERTIES HEADER_FILE_ONLY TRUE)
    set_source_files_properties(${SCENE_COMP_SOURCES} PROPERTIES HEADER_FILE_ONLY TRUE)
    set_source_files_properties(${UTILS_SOURCES} PROPERTIES HEADER_FILE_ONLY TRUE)

    list(APPEND ALL_SOURCE_FILES _Eng.cpp _Eng_Renderer.cpp _Eng_Gui.cpp _Eng_Scene.cpp _Eng_Utils.cpp)
    source_group("src" FILES _Eng.cpp _Eng_Renderer.cpp _Eng_Gui.cpp _Eng_Scene.cpp _Eng_Utils.cpp)
endif()

add_library(Eng STATIC ${ALL_SOURCE_FILES})
target_link_libraries(Eng Net Ray Ren Snd Sys astc ${ITT_LIB} ${VPX_LIB})

add_subdirectory(_tests)
