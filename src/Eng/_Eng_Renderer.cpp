
#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

#include "Renderer/Renderer.cpp"
#include "Renderer/Renderer_DrawList.cpp"
#include "Renderer/Renderer_Frontend.cpp"
#include "Renderer/Renderer_Gen.cpp"

#include "Renderer/Graph/GraphBuilder.cpp"
#include "Renderer/Passes/RpBlur.cpp"
#include "Renderer/Passes/RpCombine.cpp"
#include "Renderer/Passes/RpDebugEllipsoids.cpp"
#include "Renderer/Passes/RpDebugProbes.cpp"
#include "Renderer/Passes/RpDebugTextures.cpp"
#include "Renderer/Passes/RpDepthFill.cpp"
#include "Renderer/Passes/RpDOF.cpp"
#include "Renderer/Passes/RpDownColor.cpp"
#include "Renderer/Passes/RpDownDepth.cpp"
#include "Renderer/Passes/RpFXAA.cpp"
#include "Renderer/Passes/RpInsertFence.cpp"
#include "Renderer/Passes/RpOpaque.cpp"
#include "Renderer/Passes/RpReflections.cpp"
#include "Renderer/Passes/RpResolve.cpp"
#include "Renderer/Passes/RpSampleBrightness.cpp"
#include "Renderer/Passes/RpShadowMaps.cpp"
#include "Renderer/Passes/RpSkinning.cpp"
#include "Renderer/Passes/RpSkydome.cpp"
#include "Renderer/Passes/RpSSAO.cpp"
#include "Renderer/Passes/RpTAA.cpp"
#include "Renderer/Passes/RpTransparent.cpp"
#include "Renderer/Passes/RpUpdateBuffers.cpp"

#if defined(USE_GL_RENDER)
#include "Renderer/FrameBufGL.cpp"
#include "Renderer/PrimDrawGL.cpp"
#include "Renderer/Renderer_Backend_GLES3.cpp"
#include "Renderer/Passes/RpDebugTexturesGL.cpp"
#include "Renderer/Passes/RpDepthFillGL.cpp"
#include "Renderer/Passes/RpInsertFenceGL.cpp"
#include "Renderer/Passes/RpOpaqueGL.cpp"
#include "Renderer/Passes/RpSampleBrightnessGL.cpp"
#include "Renderer/Passes/RpShadowMapsGL.cpp"
#include "Renderer/Passes/RpSkinningGL.cpp"
#include "Renderer/Passes/RpSkydomeGL.cpp"
#include "Renderer/Passes/RpTransparentGL.cpp"
#include "Renderer/Passes/RpUpdateBuffersGL.cpp"
#endif