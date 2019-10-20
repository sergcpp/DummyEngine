cmake_minimum_required(VERSION 2.8)
project(DummyLib)

if(WIN32)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Ob1")
endif(WIN32)

set(SOURCE_FILES    Viewer.h
                    Viewer.cpp)

set(GUI_SOURCES Gui/DebugInfoUI.h
                Gui/DebugInfoUI.cpp 
                Gui/FontStorage.h)

set(RENDERER_SOURCES    Renderer/FrameBuf.h
                        Renderer/Renderer.h
                        Renderer/Renderer.cpp
                        Renderer/Renderer_DrawList.h
                        Renderer/Renderer_DrawList.cpp
                        Renderer/Renderer_Frontend.cpp
                        Renderer/Renderer_Structs.h
                        Renderer/__brdf_lut.inl
                        Renderer/__skydome_mesh.inl
                        Renderer/__sphere_mesh.inl)

set(SCENE_SOURCES   Scene/SceneData.h
                    Scene/SceneData.cpp
                    Scene/SceneManager.h
                    Scene/SceneManager.cpp
                    Scene/SceneManager_ASS.cpp
                    Scene/SceneManager_BVH.cpp
                    Scene/SceneManager_PT.cpp
                    Scene/ProbeStorage.h)

set(SCENE_COMP_SOURCES   Scene/Comp/Common.h
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
                         Scene/Comp/Transform.cpp)

set(STATES_SOURCES  States/GSCreate.h
                    States/GSCreate.cpp
                    States/GSDrawTest.h
                    States/GSDrawTest.cpp)

set(UTILS_SOURCES Utils/BVHSplit.h
                  Utils/BVHSplit.cpp
                  Utils/Cmdline.h
                  Utils/Cmdline.cpp
                  Utils/Load.h
                  Utils/Load.cpp)

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

set(SHADERS_SOURCES Renderer/Shaders/blit.frag.glsl
                    Renderer/Shaders/blit.vert.glsl
                    Renderer/Shaders/blit_bilateral.frag.glsl
                    Renderer/Shaders/blit_combine.frag.glsl
                    Renderer/Shaders/blit_debug.frag.glsl
                    Renderer/Shaders/blit_debug_bvh.frag.glsl
                    Renderer/Shaders/blit_depth.frag.glsl
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
                    Renderer/Shaders/blit_transparent_compose.frag.glsl
                    Renderer/Shaders/blit_transparent_init.frag.glsl
                    Renderer/Shaders/blit_upscale.frag.glsl
                    Renderer/Shaders/fillz.vert.glsl
                    Renderer/Shaders/fillz.frag.glsl
                    Renderer/Shaders/probe.vert.glsl
                    Renderer/Shaders/probe.frag.glsl
                    Renderer/Shaders/shadow.vert.glsl
                    Renderer/Shaders/shadow.frag.glsl
                    Renderer/Shaders/skinning.comp.glsl
                    Renderer/Shaders/skydome.frag.glsl
                    Renderer/Shaders/skydome.vert.glsl)

list(APPEND ALL_SOURCE_FILES ${SOURCE_FILES})
#set_source_files_properties(${SOURCE_FILES} PROPERTIES HEADER_FILE_ONLY TRUE)
source_group("src" FILES ${SOURCE_FILES})

list(APPEND ALL_SOURCE_FILES ${GUI_SOURCES})
set_source_files_properties(${GUI_SOURCES} PROPERTIES HEADER_FILE_ONLY TRUE)
source_group("src\\Gui" FILES ${GUI_SOURCES})

list(APPEND ALL_SOURCE_FILES ${RENDERER_SOURCES})
set_source_files_properties(${RENDERER_SOURCES} PROPERTIES HEADER_FILE_ONLY TRUE)
source_group("src\\Renderer" FILES ${RENDERER_SOURCES})

list(APPEND ALL_SOURCE_FILES ${SCENE_SOURCES})
set_source_files_properties(${SCENE_SOURCES} PROPERTIES HEADER_FILE_ONLY TRUE)
source_group("src\\Scene" FILES ${SCENE_SOURCES})

list(APPEND ALL_SOURCE_FILES ${SCENE_COMP_SOURCES})
set_source_files_properties(${SCENE_COMP_SOURCES} PROPERTIES HEADER_FILE_ONLY TRUE)
source_group("src\\Scene\\Comp" FILES ${SCENE_COMP_SOURCES})

list(APPEND ALL_SOURCE_FILES ${STATES_SOURCES})
set_source_files_properties(${STATES_SOURCES} PROPERTIES HEADER_FILE_ONLY TRUE)
source_group("src\\States" FILES ${STATES_SOURCES})

list(APPEND ALL_SOURCE_FILES ${UTILS_SOURCES})
set_source_files_properties(${UTILS_SOURCES} PROPERTIES HEADER_FILE_ONLY TRUE)
source_group("src\\Utils" FILES ${UTILS_SOURCES})

list(APPEND ALL_SOURCE_FILES ${SHADERS_SOURCES})
set_source_files_properties(${SHADERS_SOURCES} PROPERTIES HEADER_FILE_ONLY TRUE)
source_group("src\\Renderer\\Shaders" FILES ${SHADERS_SOURCES})

list(APPEND ALL_SOURCE_FILES Gui/_Gui1.cpp)
source_group("src\\Gui" FILES Gui/_Gui1.cpp)

list(APPEND ALL_SOURCE_FILES Renderer/_Renderer.cpp)
source_group("src\\Renderer" FILES Renderer/_Renderer.cpp)

list(APPEND ALL_SOURCE_FILES Scene/_Scene.cpp)
source_group("src\\Scene" FILES Scene/_Scene.cpp)

list(APPEND ALL_SOURCE_FILES States/_States.cpp)
source_group("src\\States" FILES States/_States.cpp)

list(APPEND ALL_SOURCE_FILES Utils/_Utils.cpp)
source_group("src\\Utils" FILES Utils/_Utils.cpp)

add_library(DummyLib STATIC ${ALL_SOURCE_FILES})
target_link_libraries(DummyLib Eng Ray Ren Sys astc ${LIBS})

add_subdirectory(tests)