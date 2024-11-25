#pragma once

#if defined(REN_GL_BACKEND)
#include "SamplerGL.h"
#elif defined(REN_VK_BACKEND)
#include "SamplerVK.h"
#elif defined(REN_SW_BACKEND)
#endif

namespace Ren {
using SamplerRef = StrongRef<Sampler, SparseArray<Sampler>>;
using WeakSamplerRef = WeakRef<Sampler, SparseArray<Sampler>>;
using SamplerStorage = SparseArray<Sampler>;
} // namespace Ren
