#pragma once

#include <cstdint>
#include <cstring>

#include "MVec.h"

#undef Always

namespace Ren {
enum class eCullFace : uint8_t { None, Front, Back, _Count };
enum class eCompareOp : uint8_t { Always, Never, Less, Equal, Greater, LEqual, NotEqual, GEqual, _Count };
enum class eBlendFactor : uint8_t {
    Zero,
    One,
    SrcColor,
    OneMinusSrcColor,
    DstColor,
    OneMinusDstColor,
    SrcAlpha,
    OneMinusSrcAlpha,
    DstAlpha,
    OneMinusDstAlpha,
    _Count
};
enum class eStencilOp : uint8_t { Keep, Zero, Replace, Incr, Decr, Invert, _Count };
enum class ePolygonMode : uint8_t { Fill, Line, _Count };
enum class eDepthBiasMode : uint8_t { Disabled, Static, Dynamic };
enum class eDepthRangeMode : uint8_t { ZeroToOne, NegOneToOne, _Count };

union PolyState {
    struct {
        uint8_t cull : 2;
        uint8_t mode : 1;
        uint8_t depth_bias_mode : 2;
        uint8_t multisample : 1;
        uint8_t _unused : 2;
    };
    uint8_t bits;

    PolyState()
        : cull(uint8_t(eCullFace::None)), mode(uint8_t(ePolygonMode::Fill)),
          depth_bias_mode(uint8_t(eDepthBiasMode::Disabled)), multisample(1) {}
};
static_assert(sizeof(PolyState) == 1, "!");

union DepthState {
    struct {
        uint8_t test_enabled : 1;
        uint8_t write_enabled : 1;
        uint8_t range_mode : 1;
        uint8_t compare_op : 5;
    };
    uint8_t bits;

    DepthState()
        : test_enabled(0), write_enabled(1), range_mode(uint8_t(eDepthRangeMode::ZeroToOne)),
          compare_op(uint8_t(eCompareOp::Always)) {}
};
static_assert(sizeof(DepthState) == 1, "!");

inline bool operator==(const DepthState &lhs, const DepthState &rhs) { return lhs.bits == rhs.bits; }
inline bool operator!=(const DepthState &lhs, const DepthState &rhs) { return lhs.bits != rhs.bits; }
inline bool operator<(const DepthState &lhs, const DepthState &rhs) { return lhs.bits < rhs.bits; }

union BlendState {
    struct {
        uint16_t enabled : 1;
        uint16_t src_color : 3;
        uint16_t dst_color : 3;
        uint16_t src_alpha : 3;
        uint16_t dst_alpha : 3;
        uint16_t _unused : 3;
    };
    uint16_t bits;

    BlendState()
        : enabled(0), src_color(uint8_t(eBlendFactor::Zero)), dst_color(uint8_t(eBlendFactor::Zero)),
          src_alpha(uint8_t(eBlendFactor::Zero)), dst_alpha(uint8_t(eBlendFactor::Zero)) {}
};
static_assert(sizeof(BlendState) == 2, "!");

inline bool operator==(const BlendState &lhs, const BlendState &rhs) { return lhs.bits == rhs.bits; }
inline bool operator!=(const BlendState &lhs, const BlendState &rhs) { return lhs.bits != rhs.bits; }
inline bool operator<(const BlendState &lhs, const BlendState &rhs) { return lhs.bits < rhs.bits; }

union StencilState {
    struct {
        uint16_t enabled : 1;
        uint16_t stencil_fail : 3;
        uint16_t depth_fail : 3;
        uint16_t pass : 3;
        uint16_t compare_op : 3;
        uint16_t _unused : 3;
        uint8_t reference;
        uint8_t write_mask;
        uint8_t compare_mask;
        uint8_t _unused2;
    };
    uint16_t bits[3];

    StencilState()
        : enabled(0), stencil_fail(uint8_t(eStencilOp::Keep)), depth_fail(uint8_t(eStencilOp::Keep)),
          pass(uint8_t(eStencilOp::Keep)), compare_op(uint8_t(eCompareOp::Always)), reference(0), write_mask(0xff),
          compare_mask(0xff) {}
};
static_assert(sizeof(StencilState) == 6, "!");

inline bool operator==(const StencilState &lhs, const StencilState &rhs) {
    return lhs.bits[0] == rhs.bits[1] && lhs.bits[1] == rhs.bits[1] && lhs.bits[2] == rhs.bits[2];
}
inline bool operator!=(const StencilState &lhs, const StencilState &rhs) { return !operator==(lhs, rhs); }
inline bool operator<(const StencilState &lhs, const StencilState &rhs) {
    if (lhs.bits[0] < rhs.bits[0]) {
        return true;
    } else if (lhs.bits[0] == rhs.bits[0]) {
        if (lhs.bits[1] < rhs.bits[1]) {
            return true;
        } else if (lhs.bits[1] == rhs.bits[1]) {
            return lhs.bits[2] < rhs.bits[2];
        }
    }
    return false;
}

struct DepthBias {
    float slope_factor = 0;
    float constant_offset = 0;
};

inline bool operator==(const DepthBias &lhs, const DepthBias &rhs) {
    return lhs.slope_factor == rhs.slope_factor && lhs.constant_offset == rhs.constant_offset;
}
inline bool operator!=(const DepthBias &lhs, const DepthBias &rhs) { return !operator==(lhs, rhs); }
inline bool operator<(const DepthBias &lhs, const DepthBias &rhs) {
    if (lhs.slope_factor < rhs.slope_factor) {
        return true;
    } else if (lhs.slope_factor == rhs.slope_factor) {
        return lhs.constant_offset < rhs.constant_offset;
    }
    return false;
}

struct RastState {
    PolyState poly;
    DepthState depth;
    BlendState blend;
    StencilState stencil;
    DepthBias depth_bias;

    // mutable, because they are part of dynamic state
    mutable Vec4i viewport;
    mutable struct {
        bool enabled = false;
        Vec4i rect;
    } scissor;

#if defined(USE_GL_RENDER)
    void Apply() const { Apply(nullptr); }
    void ApplyChanged(const RastState &ref) const { Apply(&ref); }
#endif

  private:
#if defined(USE_GL_RENDER)
    void Apply(const RastState *ref) const;
#endif
};

inline bool operator==(const RastState &lhs, const RastState &rhs) {
    return memcmp(&lhs, &rhs, sizeof(RastState)) == 0;
}

// inline bool operator<(const RastState &lhs, const RastState &rhs) {}

} // namespace Ren