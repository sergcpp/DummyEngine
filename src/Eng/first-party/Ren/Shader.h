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
    bool unbounded_array;
    VkFormat format = VK_FORMAT_UNDEFINED;
#endif
};
inline bool operator==(const Descr &lhs, const Descr &rhs) { return lhs.loc == rhs.loc && lhs.name == rhs.name; }
typedef Descr Attribute;
typedef Descr Uniform;
typedef Descr UniformBlock;

enum class eShaderType : uint8_t { Vert, Frag, Tesc, Tese, Comp, RayGen, Miss, ClosestHit, AnyHit, Intersection, _Count };
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
