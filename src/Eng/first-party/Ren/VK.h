#pragma once

#define VK_NO_PROTOTYPES
#define VK_ENABLE_BETA_EXTENSIONS
#include <vulkan/vulkan.h>
#undef far
#undef near
#undef max
#undef min
#undef Convex
#undef False
#undef None
#undef Success
#undef True

#define VK_API_VERSION_MAJOR(version) (((uint32_t)(version) >> 22) & 0x7FU)
#define VK_API_VERSION_MINOR(version) (((uint32_t)(version) >> 12) & 0x3FFU)
