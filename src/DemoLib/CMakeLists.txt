cmake_minimum_required(VERSION 2.8)
project(DemoLib)

set(SOURCE_FILES    Viewer.h
                    Viewer.cpp)

set(GUI_SOURCES  Gui/FontStorage.h)

set(RENDERER_SOURCES    Renderer/FrameBuf.h
                        Renderer/Renderer.h
                        Renderer/Renderer.cpp
                        Renderer/Renderer_Frontend.cpp
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

set(SHADERS_SOURCES Renderer/Shaders/blit.fs.glsl
                    Renderer/Shaders/blit.vs.glsl
                    Renderer/Shaders/blit_combine.fs.glsl
                    Renderer/Shaders/blit_debug.fs.glsl
                    Renderer/Shaders/blit_debug_bvh.fs.glsl
                    Renderer/Shaders/blit_depth.fs.glsl
                    Renderer/Shaders/blit_down.fs.glsl
                    Renderer/Shaders/blit_down_ms.fs.glsl
                    Renderer/Shaders/blit_fxaa.fs.glsl
                    Renderer/Shaders/blit_gauss.fs.glsl
                    Renderer/Shaders/blit_gauss_sep.fs.glsl
                    Renderer/Shaders/blit_mipmap.fs.glsl
                    Renderer/Shaders/blit_ms.fs.glsl
                    Renderer/Shaders/blit_multiply.fs.glsl
                    Renderer/Shaders/blit_project_sh.fs.glsl
                    Renderer/Shaders/blit_reduced.fs.glsl
                    Renderer/Shaders/blit_rgbm.fs.glsl
                    Renderer/Shaders/blit_ssao.fs.glsl
                    Renderer/Shaders/blit_ssr.fs.glsl
                    Renderer/Shaders/fillz.fs.glsl
                    Renderer/Shaders/fillz.vs.glsl
                    Renderer/Shaders/probe.vs.glsl
                    Renderer/Shaders/probe.fs.glsl
                    Renderer/Shaders/shadow.fs.glsl
                    Renderer/Shaders/shadow.vs.glsl
                    Renderer/Shaders/skinning.cs.glsl
                    Renderer/Shaders/skydome.fs.glsl
                    Renderer/Shaders/skydome.vs.glsl)

list(APPEND ALL_SOURCE_FILES ${SOURCE_FILES})
#set_source_files_properties(${SOURCE_FILES} PROPERTIES HEADER_FILE_ONLY TRUE)
source_group("src" FILES ${SOURCE_FILES})

list(APPEND ALL_SOURCE_FILES ${GUI_SOURCES})
#set_source_files_properties(${GUI_SOURCES} PROPERTIES HEADER_FILE_ONLY TRUE)
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

list(APPEND ALL_SOURCE_FILES Renderer/_Renderer.cpp)
source_group("src\\Renderer" FILES Renderer/_Renderer.cpp)

list(APPEND ALL_SOURCE_FILES Scene/_Scene.cpp)
source_group("src\\Scene" FILES Scene/_Scene.cpp)

list(APPEND ALL_SOURCE_FILES States/_States.cpp)
source_group("src\\States" FILES States/_States.cpp)

list(APPEND ALL_SOURCE_FILES Utils/_Utils.cpp)
source_group("src\\Utils" FILES Utils/_Utils.cpp)

add_library(DemoLib STATIC ${ALL_SOURCE_FILES})
target_link_libraries(DemoLib Eng Ray Ren Sys astc ${LIBS})

