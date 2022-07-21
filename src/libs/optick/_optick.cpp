
#ifdef _MSC_VER
//#pragma warning(disable : 4996)
#endif

#ifndef RELEASE_FINAL
#define USE_OPTICK 1
#else
#define USE_OPTICK 0
#endif

#define VK_NO_PROTOTYPES

#include "optick_capi.cpp"
#include "optick_core.cpp"
#include "optick_gpu.cpp"
#include "optick_gpu.vulkan.cpp"
#include "optick_message.cpp"
#include "optick_miniz.cpp"
#include "optick_serialization.cpp"
#include "optick_server.cpp"
