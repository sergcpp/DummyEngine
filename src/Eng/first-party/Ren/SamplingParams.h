#pragma once

#include <cassert>
#include <cstdint>
#undef Always

#include "Fixed.h"

namespace Ren {
using Fixed8 = Fixed<int8_t, 3>;

#define X(_0, _1, _2, _3, _4) _0,
enum class eTexFilter : uint8_t {
#include "TextureFilter.inl"
};
#undef X

#define X(_0, _1, _2) _0,
enum class eTexWrap : uint8_t {
#include "TextureWrap.inl"
};
#undef X

#define X(_0, _1, _2) _0,
enum class eTexCompare : uint8_t {
#include "TextureCompare.inl"
};
#undef X

struct SamplingParams {
    eTexFilter filter = eTexFilter::Nearest;
    eTexWrap wrap = eTexWrap::Repeat;
    eTexCompare compare = eTexCompare::None;
    Fixed8 lod_bias;
    Fixed8 min_lod = Fixed8::lowest(), max_lod = Fixed8::max();

    SamplingParams() = default;
    SamplingParams(const eTexFilter _filter, const eTexWrap _wrap, const eTexCompare _compare, const Fixed8 _lod_bias,
                   const Fixed8 _min_lod, const Fixed8 _max_lod)
        : filter(_filter), wrap(_wrap), compare(_compare), lod_bias(_lod_bias), min_lod(_min_lod), max_lod(_max_lod) {}
};
static_assert(sizeof(SamplingParams) == 6, "!");

inline bool operator==(const SamplingParams lhs, const SamplingParams rhs) {
    return lhs.filter == rhs.filter && lhs.wrap == rhs.wrap && lhs.compare == rhs.compare &&
           lhs.lod_bias == rhs.lod_bias && lhs.min_lod == rhs.min_lod && lhs.max_lod == rhs.max_lod;
}

struct SamplingParamsCompact {
    uint8_t filter : 2;
    uint8_t wrap : 2;
    uint8_t compare : 4;
    Fixed8 lod_bias;
    Fixed8 min_lod = Fixed8::lowest(), max_lod = Fixed8::max();

    SamplingParamsCompact() = default;
    SamplingParamsCompact(const SamplingParams &p) {
        assert(uint8_t(p.filter) < 4);
        assert(uint8_t(p.wrap) < 4);
        assert(uint8_t(p.compare) < 16);
        filter = uint8_t(p.filter);
        wrap = uint8_t(p.wrap);
        compare = uint8_t(p.compare);
        lod_bias = p.lod_bias;
        min_lod = p.min_lod;
        max_lod = p.max_lod;
    }

    operator SamplingParams() const {
        return SamplingParams{eTexFilter(filter), eTexWrap(wrap), eTexCompare(compare), lod_bias, min_lod, max_lod};
    }
};
static_assert(sizeof(SamplingParamsCompact) == 4, "!");

inline bool operator==(const SamplingParamsCompact lhs, const SamplingParamsCompact rhs) {
    return lhs.filter == rhs.filter && lhs.wrap == rhs.wrap && lhs.compare == rhs.compare &&
           lhs.lod_bias == rhs.lod_bias && lhs.min_lod == rhs.min_lod && lhs.max_lod == rhs.max_lod;
}

enum class eSamplerLoadStatus { Found, Created };
} // namespace Ren