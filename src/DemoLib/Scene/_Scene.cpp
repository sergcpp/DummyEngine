
#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

#include "SceneData.cpp"
#include "SceneManager.cpp"
#include "SceneManager_ASS.cpp"
#include "SceneManager_BVH.cpp"
#include "SceneManager_PT.cpp"

#include "Comp/Decal.cpp"
#include "Comp/Drawable.cpp"
#include "Comp/Lightmap.cpp"
#include "Comp/LightProbe.cpp"
#include "Comp/LightSource.cpp"
#include "Comp/Occluder.cpp"
#include "Comp/Transform.cpp"

#if defined(USE_GL_RENDER)
#include "ProbeStorageGL.cpp"
#endif
