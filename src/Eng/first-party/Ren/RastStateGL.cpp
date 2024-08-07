#include "RastState.h"

#include <iterator>

#include "GL.h"

namespace Ren {
const uint32_t gl_cull_face[] = {
    0xffffffff, // None
    GL_FRONT,   // Front
    GL_BACK,    // Back
};
static_assert(std::size(gl_cull_face) == size_t(eCullFace::_Count), "!");

const uint32_t gl_blend_factor[] = {
    GL_ZERO,                // Zero
    GL_ONE,                 // One
    GL_SRC_COLOR,           // SrcColor
    GL_ONE_MINUS_SRC_COLOR, // OneMinusSrcColor
    GL_DST_COLOR,           // DstColor
    GL_ONE_MINUS_DST_COLOR, // OneMinusDstColor
    GL_SRC_ALPHA,           // SrcAlpha
    GL_ONE_MINUS_SRC_ALPHA, // OneMinusSrcAlpha
    GL_DST_ALPHA,           // DstAlpha
    GL_ONE_MINUS_DST_ALPHA  // OneMinusDstAlpha
};
static_assert(std::size(gl_blend_factor) == size_t(eBlendFactor::_Count), "!");

const uint32_t gl_compare_op[] = {
    GL_ALWAYS,   // Always
    GL_NEVER,    // Never
    GL_LESS,     // Less
    GL_EQUAL,    // Equal
    GL_GREATER,  // Greater
    GL_LEQUAL,   // LEqual
    GL_NOTEQUAL, // NotEqual
    GL_GEQUAL    // GEqual
};
static_assert(std::size(gl_compare_op) == size_t(eCompareOp::_Count), "!");

const uint32_t gl_stencil_op[] = {
    GL_KEEP,    // Keep
    GL_ZERO,    // Zero
    GL_REPLACE, // Replace
    GL_INCR,    // Incr
    GL_DECR,    // Decr
    GL_INVERT   // Invert
};
static_assert(std::size(gl_stencil_op) == size_t(eStencilOp::_Count), "!");

const uint32_t gl_polygon_mode[] = {
    GL_FILL, // Fill
    GL_LINE, // Line
};
static_assert(std::size(gl_polygon_mode) == size_t(ePolygonMode::_Count), "!");

const uint32_t gl_depth_range_mode[] = {
    GL_ZERO_TO_ONE,        // ZeroToOne
    GL_NEGATIVE_ONE_TO_ONE // NegOneToOne
};
static_assert(std::size(gl_depth_range_mode) == size_t(eDepthRangeMode::_Count), "!");

eCullFace cull_face_from_gl_enum(GLenum face) {
    if (face == GL_FRONT) {
        return eCullFace::Front;
    } else if (face == GL_BACK) {
        return eCullFace::Back;
    }
    return eCullFace::Back;
}

eBlendFactor blend_factor_from_gl_enum(GLenum factor) {
    if (factor == GL_ZERO) {
        return eBlendFactor::Zero;
    } else if (factor == GL_ONE) {
        return eBlendFactor::One;
    } else if (factor == GL_SRC_COLOR) {
        return eBlendFactor::SrcColor;
    } else if (factor == GL_ONE_MINUS_SRC_COLOR) {
        return eBlendFactor::OneMinusSrcColor;
    } else if (factor == GL_DST_COLOR) {
        return eBlendFactor::DstColor;
    } else if (factor == GL_ONE_MINUS_DST_COLOR) {
        return eBlendFactor::OneMinusDstColor;
    } else if (factor == GL_SRC_ALPHA) {
        return eBlendFactor::SrcAlpha;
    } else if (factor == GL_ONE_MINUS_SRC_ALPHA) {
        return eBlendFactor::OneMinusSrcAlpha;
    } else if (factor == GL_DST_ALPHA) {
        return eBlendFactor::DstAlpha;
    } else if (factor == GL_ONE_MINUS_DST_ALPHA) {
        return eBlendFactor::OneMinusDstAlpha;
    }
    return eBlendFactor::Zero;
}

eCompareOp test_func_from_gl_enum(GLenum func) {
    if (func == GL_ALWAYS) {
        return eCompareOp::Always;
    } else if (func == GL_NEVER) {
        return eCompareOp::Never;
    } else if (func == GL_LESS) {
        return eCompareOp::Less;
    } else if (func == GL_GREATER) {
        return eCompareOp::Greater;
    } else if (func == GL_LEQUAL) {
        return eCompareOp::LEqual;
    } else if (func == GL_NOTEQUAL) {
        return eCompareOp::NotEqual;
    } else if (func == GL_GEQUAL) {
        return eCompareOp::GEqual;
    }
    return eCompareOp::Always;
}
} // namespace Ren

void Ren::RastState::Apply(const RastState *ref) const {
    if (!ref || ref->poly.cull != poly.cull) {
        if (eCullFace(poly.cull) != eCullFace::None) {
            glEnable(GL_CULL_FACE);
            glCullFace(gl_cull_face[poly.cull]);
        } else {
            glDisable(GL_CULL_FACE);
        }
    }

    if (!ref || ref->depth != depth) {
        if (depth.test_enabled) {
            glEnable(GL_DEPTH_TEST);
        } else {
            glDisable(GL_DEPTH_TEST);
        }
        glDepthFunc(gl_compare_op[depth.compare_op]);
        glClipControl(GL_LOWER_LEFT, gl_depth_range_mode[depth.range_mode]);
    }

    if (!ref || ref->depth.write_enabled != depth.write_enabled) {
        if (depth.write_enabled) {
            glDepthMask(GL_TRUE);
        } else {
            glDepthMask(GL_FALSE);
        }
    }

    if (!ref || ref->blend != blend) {
        if (blend.enabled) {
            glEnable(GL_BLEND);
        } else {
            glDisable(GL_BLEND);
        }
        glBlendFuncSeparate(gl_blend_factor[blend.src_color], gl_blend_factor[blend.dst_color],
                            gl_blend_factor[blend.src_alpha], gl_blend_factor[blend.dst_alpha]);
    }

    if (!ref || ref->stencil != stencil) {
        if (stencil.enabled) {
            glEnable(GL_STENCIL_TEST);
        } else {
            glDisable(GL_STENCIL_TEST);
        }
        glStencilMask(stencil.write_mask);
        glStencilOp(gl_stencil_op[int(stencil.stencil_fail)], gl_stencil_op[int(stencil.depth_fail)],
                    gl_stencil_op[int(stencil.pass)]);
        glStencilFunc(gl_compare_op[stencil.compare_op], stencil.reference, stencil.compare_mask);
    }

#if !defined(__ANDROID__)
    if (!ref || ref->poly.mode != poly.mode) {
        glPolygonMode(GL_FRONT_AND_BACK, gl_polygon_mode[poly.mode]);
    }
#endif

    if (!ref || ref->poly.depth_bias_mode != poly.depth_bias_mode) {
        if (eDepthBiasMode(poly.depth_bias_mode) != eDepthBiasMode::Disabled) {
            glEnable(GL_POLYGON_OFFSET_FILL);
            glEnable(GL_POLYGON_OFFSET_LINE);
        } else {
            glDisable(GL_POLYGON_OFFSET_FILL);
            glDisable(GL_POLYGON_OFFSET_LINE);
        }
    }

    if (!ref || ref->depth_bias != depth_bias) {
        glPolygonOffset(depth_bias.slope_factor, depth_bias.constant_offset);
    }

    if (!ref || ref->viewport != viewport) {
        glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    }

    if (!ref || std::memcmp(&ref->scissor, &scissor, sizeof(scissor)) != 0) {
        if (scissor.enabled) {
            glEnable(GL_SCISSOR_TEST);
        } else {
            glDisable(GL_SCISSOR_TEST);
        }
        glScissor(scissor.rect[0], scissor.rect[1], scissor.rect[2], scissor.rect[3]);
    }

    if (!ref || ref->poly.multisample != poly.multisample) {
        if (poly.multisample) {
            glEnable(GL_MULTISAMPLE);
        } else {
            glDisable(GL_MULTISAMPLE);
        }
    }
}
