#pragma once

#include "String.h"
#if defined(USE_VK_RENDER)
#include "VK.h"
#endif

namespace Ren {
struct Descr {
    String name;
    int loc = -1;
#if defined(USE_VK_RENDER)
    VkDescriptorType desc_type = VK_DESCRIPTOR_TYPE_MAX_ENUM;
    int set = 0, count = 0;
    bool unbounded_array = false;
    VkFormat format = VK_FORMAT_UNDEFINED;
#endif
};
inline bool operator==(const Descr &lhs, const Descr &rhs) { return lhs.loc == rhs.loc && lhs.name == rhs.name; }

using Attribute = Descr;
using Uniform = Descr;
using UniformBlock = Descr;

enum class eShaderType : uint8_t {
    Vertex,
    Fragment,
    TesselationControl,
    TesselationEvaluation,
    Geometry,
    Compute,
    RayGen,
    Miss,
    ClosestHit,
    AnyHit,
    Intersection,
    _Count
};
enum class eShaderSource : uint8_t { GLSL, SPIRV, _Count };
enum class eShaderLoadStatus { Found, SetToDefault, CreatedFromData, Error };
} // namespace Ren

#if defined(USE_GL_RENDER)
#include "ShaderGL.h"
#elif defined(USE_VK_RENDER)
#include "ShaderVK.h"
#elif defined(USE_SW_RENDER)
#error "TODO"
#endif
