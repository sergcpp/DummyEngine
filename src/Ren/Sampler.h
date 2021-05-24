#pragma once

#include "Fixed.h"

namespace Ren {
using Fixed8 = Fixed<int8_t, 3>;

enum class eTexFilter : uint8_t {
    NoFilter,
    Bilinear,
    Trilinear,
    BilinearNoMipmap,
    _Count
};
enum class eTexRepeat : uint8_t { Repeat, ClampToEdge, ClampToBorder, WrapModesCount };

enum class eTexCompare : uint8_t {
    None,
    LEqual,
    GEqual,
    Less,
    Greater,
    Equal,
    NotEqual,
    Always,
    Never,
    _Count
};

struct SamplingParams {
    eTexFilter filter = eTexFilter::NoFilter;
    eTexRepeat repeat = eTexRepeat::Repeat;
    eTexCompare compare = eTexCompare::None;
    Fixed8 lod_bias;
    Fixed8 min_lod = Fixed8::lowest(), max_lod = Fixed8::max();
};
static_assert(sizeof(SamplingParams) == 6, "!");

inline bool operator==(const SamplingParams &lhs, const SamplingParams &rhs) {
    return lhs.filter == rhs.filter && lhs.repeat == rhs.repeat &&
           lhs.compare == rhs.compare && lhs.lod_bias == rhs.lod_bias &&
           lhs.min_lod == rhs.min_lod && lhs.max_lod == rhs.max_lod;
}

enum class eSamplerLoadStatus {
    Found,
    Created
};
}

#if defined(USE_GL_RENDER)
#include "SamplerGL.h"
#elif defined(USE_SW_RENDER)
#endif

namespace Ren {
using SamplerRef = StrongRef<Sampler, SparseArray<Sampler>>;
using WeakSamplerRef = WeakRef<Sampler, SparseArray<Sampler>>;
using SamplerStorage = SparseArray<Sampler>;
}
