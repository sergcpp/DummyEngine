#pragma once

#include "String.h"
#if defined(REN_VK_BACKEND)
#include "VK.h"
#endif

namespace Ren {
struct Descr {
    String name;
    int loc = -1;
#if defined(REN_VK_BACKEND)
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
} // namespace Ren

#if defined(REN_GL_BACKEND)
#include "ShaderGL.h"
#elif defined(REN_VK_BACKEND)
#include "ShaderVK.h"
#elif defined(REN_SW_BACKEND)
#error "TODO"
#endif
