#pragma once

#define VK_NO_PROTOTYPES
// #define VK_ENABLE_BETA_EXTENSIONS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <vulkan/vulkan.h>

#undef far
#undef near
#undef Convex
#undef False
#undef None
#undef Success
#undef True
#undef GetObject

#define VK_API_VERSION_MAJOR(version) (((uint32_t)(version) >> 22) & 0x7FU)
#define VK_API_VERSION_MINOR(version) (((uint32_t)(version) >> 12) & 0x3FFU)
