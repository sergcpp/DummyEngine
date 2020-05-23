cmake_minimum_required(VERSION 2.8.4)
project(Eng)

add_definitions(-DSHINY_IS_COMPILED=FALSE)

IF(WIN32)

ELSE(WIN32)
    set(CMAKE_CXX_STANDARD  11)
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
                        Gui/Utils.cpp
                        )

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

set(RENDERER_SOURCES    Renderer/FrameBuf.h
                        Renderer/Renderer.h
                        Renderer/Renderer.cpp
                        Renderer/Renderer_DrawList.h
                        Renderer/Renderer_DrawList.cpp
                        Renderer/Renderer_Frontend.cpp
                        Renderer/Renderer_Gen.cpp
                        Renderer/Renderer_Structs.h
                        Renderer/__brdf_lut.inl
                        Renderer/__cone_rt_lut.inl
                        Renderer/__noise.inl
                        Renderer/__skydome_mesh.inl
                        Renderer/__sphere_mesh.inl)

set(SCENE_SOURCES   Scene/SceneData.h
                    Scene/SceneData.cpp
                    Scene/SceneManager.h
                    Scene/SceneManager.cpp
                    Scene/SceneManager_ASS.cpp
                    Scene/SceneManager_ASS_Font.cpp
                    Scene/SceneManager_ASS_Shader.cpp
                    Scene/SceneManager_ASS_Tex.cpp
                    Scene/SceneManager_BVH.cpp
                    Scene/SceneManager_PT.cpp
                    Scene/ProbeStorage.h)

if(${RENDERER} STREQUAL "GL")
    set(RENDERER_SOURCES ${RENDERER_SOURCES}
                        Renderer/FrameBufGL.cpp
                        Renderer/Renderer_Backend_GLES3.cpp
                        Renderer/Renderer_GL_Defines.inl
                        Renderer/Renderer_GL_Shaders.inl)
    set(SCENE_SOURCES ${SCENE_SOURCES}
                        Scene/ProbeStorageGL.cpp)
elseif(${RENDERER} STREQUAL "SW")
    set(RENDERER_SOURCES ${RENDERER_SOURCES}
                        Renderer/RendererSW.cpp)
endif()

set(SHADERS_SOURCES Renderer/Shaders/_common.glsl
                    Renderer/Shaders/_fs_common.glsl
                    Renderer/Shaders/_vs_common.glsl
                    Renderer/Shaders/blit.frag.glsl
                    Renderer/Shaders/blit.vert.glsl
                    Renderer/Shaders/blit_bilateral.frag.glsl
                    Renderer/Shaders/blit_combine.frag.glsl
                    Renderer/Shaders/blit_debug.frag.glsl
                    Renderer/Shaders/blit_debug_bvh.frag.glsl
                    Renderer/Shaders/blit_depth.frag.glsl
                    Renderer/Shaders/blit_dof_init_coc.frag.glsl
                    Renderer/Shaders/blit_dof_bilateral.frag.glsl
                    Renderer/Shaders/blit_dof_calc_near.frag.glsl
                    Renderer/Shaders/blit_dof_small_blur.frag.glsl
                    Renderer/Shaders/blit_dof_combine.frag.glsl
                    Renderer/Shaders/blit_down.frag.glsl
                    Renderer/Shaders/blit_down_depth.frag.glsl
                    Renderer/Shaders/blit_down_ms.frag.glsl
                    Renderer/Shaders/blit_fxaa.frag.glsl
                    Renderer/Shaders/blit_gauss.frag.glsl
                    Renderer/Shaders/blit_gauss_sep.frag.glsl
                    Renderer/Shaders/blit_mipmap.frag.glsl
                    Renderer/Shaders/blit_ms.frag.glsl
                    Renderer/Shaders/blit_ms_resolve.frag.glsl
                    Renderer/Shaders/blit_multiply.frag.glsl
					Renderer/Shaders/blit_prefilter.frag.glsl
                    Renderer/Shaders/blit_project_sh.frag.glsl
                    Renderer/Shaders/blit_reduced.frag.glsl
                    Renderer/Shaders/blit_rgbm.frag.glsl
                    Renderer/Shaders/blit_ssao.frag.glsl
                    Renderer/Shaders/blit_ssr.frag.glsl
                    Renderer/Shaders/blit_ssr_compose.frag.glsl
                    Renderer/Shaders/blit_ssr_dilate.frag.glsl
                    Renderer/Shaders/blit_static_vel.frag.glsl
                    Renderer/Shaders/blit_taa.frag.glsl
                    Renderer/Shaders/blit_transparent_compose.frag.glsl
                    Renderer/Shaders/blit_transparent_init.frag.glsl
                    Renderer/Shaders/blit_upscale.frag.glsl
                    Renderer/Shaders/ellipsoid.vert.glsl
                    Renderer/Shaders/ellipsoid.frag.glsl
                    Renderer/Shaders/fillz.vert.glsl
                    Renderer/Shaders/fillz_vege.vert.glsl
                    Renderer/Shaders/fillz.frag.glsl
                    Renderer/Shaders/probe.vert.glsl
                    Renderer/Shaders/probe.frag.glsl
                    Renderer/Shaders/shadow.vert.glsl
                    Renderer/Shaders/shadow_vege.vert.glsl
                    Renderer/Shaders/shadow.frag.glsl
                    Renderer/Shaders/skinning.comp.glsl
                    Renderer/Shaders/skydome.frag.glsl
                    Renderer/Shaders/skydome.vert.glsl)

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
				  Utils/ScriptedSequence.cpp)

list(APPEND ALL_SOURCE_FILES ${SOURCE_FILES})
set_source_files_properties(${SOURCE_FILES} PROPERTIES HEADER_FILE_ONLY TRUE)
source_group("src" FILES ${SOURCE_FILES})

list(APPEND ALL_SOURCE_FILES ${GUI_SOURCE_FILES})
set_source_files_properties(${GUI_SOURCE_FILES} PROPERTIES HEADER_FILE_ONLY TRUE)
source_group("src\\Gui" FILES ${GUI_SOURCE_FILES})

list(APPEND ALL_SOURCE_FILES ${INPUT_SOURCE_FILES})
set_source_files_properties(${INPUT_SOURCE_FILES} PROPERTIES HEADER_FILE_ONLY TRUE)
source_group("src\\Input" FILES ${INPUT_SOURCE_FILES})

list(APPEND ALL_SOURCE_FILES ${RENDERER_SOURCES})
set_source_files_properties(${RENDERER_SOURCES} PROPERTIES HEADER_FILE_ONLY TRUE)
source_group("src\\Renderer" FILES ${RENDERER_SOURCES})

list(APPEND ALL_SOURCE_FILES ${SHADERS_SOURCES})
set_source_files_properties(${SHADERS_SOURCES} PROPERTIES HEADER_FILE_ONLY TRUE)
source_group("src\\Renderer\\Shaders" FILES ${SHADERS_SOURCES})

list(APPEND ALL_SOURCE_FILES ${SCENE_SOURCES})
set_source_files_properties(${SCENE_SOURCES} PROPERTIES HEADER_FILE_ONLY TRUE)
source_group("src\\Scene" FILES ${SCENE_SOURCES})

list(APPEND ALL_SOURCE_FILES ${SCENE_COMP_SOURCES})
set_source_files_properties(${SCENE_COMP_SOURCES} PROPERTIES HEADER_FILE_ONLY TRUE)
source_group("src\\Scene\\Comp" FILES ${SCENE_COMP_SOURCES})

list(APPEND ALL_SOURCE_FILES ${UTILS_SOURCES})
set_source_files_properties(${UTILS_SOURCES} PROPERTIES HEADER_FILE_ONLY TRUE)
source_group("src\\Utils" FILES ${UTILS_SOURCES})

list(APPEND ALL_SOURCE_FILES _Eng.cpp _Eng_Renderer.cpp _Eng_Gui.cpp _Eng_Scene.cpp _Eng_Utils.cpp)
source_group("src" FILES _Eng.cpp _Eng_Renderer.cpp _Eng_Gui.cpp _Eng_Scene.cpp _Eng_Utils.cpp)

add_library(Eng STATIC ${ALL_SOURCE_FILES})
target_link_libraries(Eng Ren Sys)

add_subdirectory(_tests)
