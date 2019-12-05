#define _CRT_SECURE_NO_WARNINGS

#include "BaseElement.cpp"
#include "BitmapFont.cpp"
#include "ButtonBase.cpp"
#include "ButtonImage.cpp"
#include "ButtonText.cpp"
//#include "Cursor.cpp"
#include "EditBox.cpp"
#include "Image.cpp"
#include "ImageNinePatch.cpp"
#include "LinearLayout.cpp"
#include "TypeMesh.cpp"
#include "Utils.cpp"

#if defined(USE_GL_RENDER)
#include "RendererGL.cpp"
#elif defined(USE_SW_RENDER)
#include "RendererSW.cpp"
#endif
