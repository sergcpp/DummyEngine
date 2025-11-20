#include "RastState.h"

namespace Ren {
static const std::string_view g_cull_face_names[] = {
    "None",  // None
    "Front", // Front
    "Back"   // Back
};
static_assert(std::size(g_cull_face_names) == int(eCullFace::_Count));

#define X(_0, _1, _2) #_0,
static const std::string_view g_compare_op_names[] = {
#include "CompareOp.inl"
};
#undef X

static const std::string_view g_blend_factor_names[] = {
    "Zero",             // Zero
    "One",              // One
    "SrcColor",         // SrcColor
    "OneMinusSrcColor", // OneMinusSrcColor
    "DstColor",         // DstColor
    "OneMinusDstColor", // OneMinusDstColor
    "SrcAlpha",         // SrcAlpha
    "OneMinusSrcAlpha", // OneMinusSrcAlpha
    "DstAlpha",         // DstAlpha
    "OneMinusDstAlpha"  // OneMinusDstAlpha
};
static_assert(std::size(g_blend_factor_names) == int(eBlendFactor::_Count));

static const std::string_view g_stencil_op_names[] = {
    "Keep",    // Keep
    "Zero",    // Zero
    "Replace", // Replace
    "Incr",    // Incr
    "Decr",    // Decr
    "Invert"   // Invert
};
static_assert(std::size(g_stencil_op_names) == int(eStencilOp::_Count));

static const std::string_view g_poly_mode_names[] = {
    "Fill", // Fill
    "Line"  // Line
};
static_assert(std::size(g_poly_mode_names) == int(ePolygonMode::_Count));

static const std::string_view g_depth_bias_mode_names[] = {
    "Disabled", // Disabled
    "Static",   // Static
    "Dynamic"   // Dynamic
};
static_assert(std::size(g_depth_bias_mode_names) == int(eDepthBiasMode::_Count));

static const std::string_view g_depth_range_mode_names[] = {
    "ZeroToOne",  // ZeroToOne
    "NegOneToOne" // NegOneToOne
};
static_assert(std::size(g_depth_range_mode_names) == int(eDepthRangeMode::_Count));
} // namespace Ren

std::string_view Ren::CullFaceName(const eCullFace face) { return g_cull_face_names[uint8_t(face)]; }

Ren::eCullFace Ren::CullFace(std::string_view name) {
    for (int i = 0; i < int(eCullFace::_Count); ++i) {
        if (name == g_cull_face_names[i]) {
            return eCullFace(i);
        }
    }
    return eCullFace::None;
}

std::string_view Ren::CompareOpName(const eCompareOp op) { return g_compare_op_names[uint8_t(op)]; }

Ren::eCompareOp Ren::CompareOp(std::string_view name) {
    for (int i = 0; i < int(eCompareOp::_Count); ++i) {
        if (name == g_compare_op_names[i]) {
            return eCompareOp(i);
        }
    }
    return eCompareOp::Always;
}

std::string_view Ren::BlendFactorName(const eBlendFactor op) { return g_blend_factor_names[uint8_t(op)]; }

Ren::eBlendFactor Ren::BlendFactor(std::string_view name) {
    for (int i = 0; i < int(eBlendFactor::_Count); ++i) {
        if (name == g_blend_factor_names[i]) {
            return eBlendFactor(i);
        }
    }
    return eBlendFactor::Zero;
}

std::string_view Ren::StencilOpName(const eStencilOp op) { return g_stencil_op_names[uint8_t(op)]; }

Ren::eStencilOp Ren::StencilOp(std::string_view name) {
    for (int i = 0; i < int(eStencilOp::_Count); ++i) {
        if (name == g_stencil_op_names[i]) {
            return eStencilOp(i);
        }
    }
    return eStencilOp::Keep;
}

std::string_view Ren::PolygonModeName(const ePolygonMode mode) { return g_poly_mode_names[uint8_t(mode)]; }

Ren::ePolygonMode Ren::PolygonMode(std::string_view name) {
    for (int i = 0; i < int(ePolygonMode::_Count); ++i) {
        if (name == g_poly_mode_names[i]) {
            return ePolygonMode(i);
        }
    }
    return ePolygonMode::Fill;
}

std::string_view Ren::DepthBiasModeName(const eDepthBiasMode mode) { return g_depth_bias_mode_names[uint8_t(mode)]; }

Ren::eDepthBiasMode Ren::DepthBiasMode(std::string_view name) {
    for (int i = 0; i < int(eDepthBiasMode::_Count); ++i) {
        if (name == g_depth_bias_mode_names[i]) {
            return eDepthBiasMode(i);
        }
    }
    return eDepthBiasMode::Disabled;
}

std::string_view Ren::DepthRangeModeName(const eDepthRangeMode mode) { return g_depth_range_mode_names[uint8_t(mode)]; }

Ren::eDepthRangeMode Ren::DepthRangeMode(std::string_view name) {
    for (int i = 0; i < int(eDepthRangeMode::_Count); ++i) {
        if (name == g_depth_range_mode_names[i]) {
            return eDepthRangeMode(i);
        }
    }
    return eDepthRangeMode::ZeroToOne;
}
