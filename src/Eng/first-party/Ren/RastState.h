#pragma once

#include <cstdint>
#include <cstring>

#include <string_view>

#include "MVec.h"

#undef Always

namespace Ren {
#define X(_0, ...) _0,
enum class eCompareOp : uint8_t {
#include "CompareOp.inl"
    _Count
};
#undef X

#define X(_0, ...) _0,
enum class eStencilOp : uint8_t {
#include "StencilOp.inl"
    _Count
};
#undef X

#define X(_0, ...) _0,
enum class eBlendFactor : uint8_t {
#include "BlendFactor.inl"
    _Count
};
#undef X

#define X(_0, ...) _0,
enum class eBlendOp : uint8_t {
#include "BlendOp.inl"
    _Count
};
#undef X

enum class eCullFace : uint8_t { None, Front, Back, _Count };
enum class ePolygonMode : uint8_t { Fill, Line, _Count };
enum class eDepthBiasMode : uint8_t { Disabled, Static, Dynamic, _Count };
enum class eDepthRangeMode : uint8_t { ZeroToOne, NegOneToOne, _Count };

std::string_view CullFaceName(eCullFace face);
eCullFace CullFace(std::string_view name);

std::string_view CompareOpName(eCompareOp op);
eCompareOp CompareOp(std::string_view name);

std::string_view BlendFactorName(eBlendFactor op);
eBlendFactor BlendFactor(std::string_view name);

std::string_view StencilOpName(eStencilOp op);
eStencilOp StencilOp(std::string_view name);

std::string_view PolygonModeName(ePolygonMode mode);
ePolygonMode PolygonMode(std::string_view name);

std::string_view DepthBiasModeName(eDepthBiasMode mode);
eDepthBiasMode DepthBiasMode(std::string_view name);

std::string_view DepthRangeModeName(eDepthRangeMode mode);
eDepthRangeMode DepthRangeMode(std::string_view name);

union PolyState {
    struct {
        uint8_t cull : 2;
        uint8_t mode : 1;
        uint8_t depth_bias_mode : 2;
        uint8_t multisample : 1;
        uint8_t _unused : 2;
    };
    uint8_t bits;

    PolyState() : bits(0) {
        cull = uint8_t(eCullFace::None);
        mode = uint8_t(ePolygonMode::Fill);
        depth_bias_mode = uint8_t(eDepthBiasMode::Disabled);
        multisample = 1;
    }
};
static_assert(sizeof(PolyState) == 1);

inline bool operator==(const PolyState &lhs, const PolyState &rhs) { return lhs.bits == rhs.bits; }
inline bool operator!=(const PolyState &lhs, const PolyState &rhs) { return lhs.bits != rhs.bits; }
inline bool operator<(const PolyState &lhs, const PolyState &rhs) { return lhs.bits < rhs.bits; }

union DepthState {
    struct {
        uint8_t test_enabled : 1;
        uint8_t write_enabled : 1;
        uint8_t range_mode : 1;
        uint8_t compare_op : 5;
    };
    uint8_t bits;

    DepthState() : bits(0) {
        test_enabled = 0;
        write_enabled = 1;
        range_mode = uint8_t(eDepthRangeMode::ZeroToOne);
        compare_op = uint8_t(eCompareOp::Always);
    }
};
static_assert(sizeof(DepthState) == 1);

inline bool operator==(const DepthState &lhs, const DepthState &rhs) { return lhs.bits == rhs.bits; }
inline bool operator!=(const DepthState &lhs, const DepthState &rhs) { return lhs.bits != rhs.bits; }
inline bool operator<(const DepthState &lhs, const DepthState &rhs) { return lhs.bits < rhs.bits; }

union BlendState {
    struct {
        uint8_t enabled : 1;
        uint8_t color_op : 3;
        uint8_t src_color : 4;
        uint8_t dst_color : 4;
        uint8_t _unused : 1;
        uint8_t alpha_op : 3;
        uint8_t src_alpha : 4;
        uint8_t dst_alpha : 4;
    };
    uint8_t bits[3];

    BlendState() : bits{0, 0, 0} {
        enabled = 0;
        color_op = alpha_op = uint8_t(eBlendOp::Add);
        src_color = dst_color = uint8_t(eBlendFactor::Zero);
        src_alpha = dst_alpha = uint8_t(eBlendFactor::Zero);
    }
};
static_assert(sizeof(BlendState) == 3);

inline bool operator==(const BlendState &lhs, const BlendState &rhs) {
    return lhs.bits[0] == rhs.bits[0] && lhs.bits[1] == rhs.bits[1] && lhs.bits[2] == rhs.bits[2];
}
inline bool operator!=(const BlendState &lhs, const BlendState &rhs) {
    return lhs.bits[0] != rhs.bits[0] || lhs.bits[1] != rhs.bits[1] || lhs.bits[2] != rhs.bits[2];
}
inline bool operator<(const BlendState &lhs, const BlendState &rhs) {
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

    StencilState() : bits{0, 0, 0} {
        enabled = 0;
        stencil_fail = uint8_t(eStencilOp::Keep);
        depth_fail = uint8_t(eStencilOp::Keep);
        pass = uint8_t(eStencilOp::Keep);
        compare_op = uint8_t(eCompareOp::Always);
        reference = 0;
        write_mask = 0xff;
        compare_mask = 0xff;
    }
};
static_assert(sizeof(StencilState) == 6);

inline bool operator==(const StencilState &lhs, const StencilState &rhs) {
    return lhs.bits[0] == rhs.bits[0] && lhs.bits[1] == rhs.bits[1] && lhs.bits[2] == rhs.bits[2];
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

#if defined(REN_GL_BACKEND)
    void Apply() const { Apply(nullptr); }
    void ApplyChanged(const RastState &ref) const { Apply(&ref); }
#endif

  private:
#if defined(REN_GL_BACKEND)
    void Apply(const RastState *ref) const;
#endif
};

inline bool operator==(const RastState &lhs, const RastState &rhs) {
    return lhs.poly == rhs.poly && lhs.depth == rhs.depth && lhs.blend == rhs.blend && lhs.stencil == rhs.stencil &&
           lhs.depth_bias == rhs.depth_bias /*&& lhs.viewport == rhs.viewport &&
           lhs.scissor.enabled == rhs.scissor.enabled && lhs.scissor.rect == rhs.scissor.rect*/
        ;
}
inline bool operator!=(const RastState &lhs, const RastState &rhs) { return !operator==(lhs, rhs); }
inline bool operator<(const RastState &lhs, const RastState &rhs) {
    if (lhs.poly < rhs.poly) {
        return true;
    } else if (lhs.poly == rhs.poly) {
        if (lhs.depth < rhs.depth) {
            return true;
        } else if (lhs.depth == rhs.depth) {
            if (lhs.blend < rhs.blend) {
                return true;
            } else if (lhs.blend == rhs.blend) {
                if (lhs.stencil < rhs.stencil) {
                    return true;
                } else if (lhs.stencil == rhs.stencil) {
                    return lhs.depth_bias < rhs.depth_bias;
                }
            }
        }
    }
    return false;
}

} // namespace Ren