#pragma once

#if defined(USE_GL_RENDER)
#include "SamplerGL.h"
#elif defined(USE_VK_RENDER)
#include "SamplerVK.h"
#elif defined(USE_SW_RENDER)
#endif

namespace Ren {
using SamplerRef = StrongRef<Sampler, SparseArray<Sampler>>;
using WeakSamplerRef = WeakRef<Sampler, SparseArray<Sampler>>;
using SamplerStorage = SparseArray<Sampler>;
} // namespace Ren
