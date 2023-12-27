
#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

#include "GSBaseState.cpp"
#include "GSDrawTest.cpp"
#include "GSPhyTest.cpp"
#include "GSPlayTest.cpp"
#include "GSUITest.cpp"
#include "GSUITest2.cpp"
#include "GSUITest3.cpp"
#include "GSUITest4.cpp"
#include "GSVideoTest.cpp"

#if defined(USE_GL_RENDER)
#include "GSUITest3GL.cpp"
#include "GSVideoTestGL.cpp"
#elif defined(USE_VK_RENDER)
#include "GSUITest3VK.cpp"
#include "GSVideoTestVK.cpp"
#elif defined(USE_SW_RENDER)
#endif
