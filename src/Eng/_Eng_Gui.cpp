#define _CRT_SECURE_NO_WARNINGS

#include "Gui/BaseElement.cpp"
#include "Gui/BitmapFont.cpp"
#include "Gui/ButtonBase.cpp"
#include "Gui/ButtonImage.cpp"
#include "Gui/ButtonText.cpp"
//#include "Gui/Cursor.cpp"
#include "Gui/EditBox.cpp"
#include "Gui/Image.cpp"
#include "Gui/Image9Patch.cpp"
#include "Gui/LinearLayout.cpp"
#include "Gui/Renderer.cpp"
#include "Gui/TypeMesh.cpp"
#include "Gui/Utils.cpp"

#if defined(USE_GL_RENDER)
#include "Gui/RendererGL.cpp"
#elif defined(USE_VK_RENDER)
#include "Gui/RendererVK.cpp"
#elif defined(USE_SW_RENDER)
#include "Gui/RendererSW.cpp"
#endif
