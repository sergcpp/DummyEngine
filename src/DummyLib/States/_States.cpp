
#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

#include "GSCreate.cpp"
#include "GSBaseState.cpp"
#include "GSDrawTest.cpp"
#include "GSPlayTest.cpp"
#include "GSUITest.cpp"
#include "GSUITest2.cpp"
#include "GSUITest3.cpp"
#include "GSUITest4.cpp"

#if defined(USE_GL_RENDER)
#include "GSUITest3GL.cpp"
#elif defined(USE_SW_RENDER)
#endif
