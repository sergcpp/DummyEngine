#pragma once

#define VK_NO_PROTOTYPES
#define VK_ENABLE_BETA_EXTENSIONS // needed for VK_KHR_portability_subset on mac
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
