
#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

#include "Scene/SceneData.cpp"
#include "Scene/SceneManager.cpp"
#include "Scene/SceneManager_ASS.cpp"
#include "Scene/SceneManager_ASS_Font.cpp"
#include "Scene/SceneManager_ASS_Shader.cpp"
#include "Scene/SceneManager_ASS_Tex.cpp"
#include "Scene/SceneManager_BVH.cpp"
#include "Scene/SceneManager_PT.cpp"

#include "Scene/Comp/AnimState.cpp"
#include "Scene/Comp/Decal.cpp"
#include "Scene/Comp/Drawable.cpp"
#include "Scene/Comp/Lightmap.cpp"
#include "Scene/Comp/LightProbe.cpp"
#include "Scene/Comp/LightSource.cpp"
#include "Scene/Comp/Occluder.cpp"
#include "Scene/Comp/SoundSource.cpp"
#include "Scene/Comp/Transform.cpp"
#include "Scene/Comp/VegState.cpp"

#if defined(USE_GL_RENDER)
#include "Scene/ProbeStorageGL.cpp"
#endif
