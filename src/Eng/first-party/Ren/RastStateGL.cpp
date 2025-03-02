#include "RastState.h"

#include <iterator>

#include "GL.h"

namespace Ren {
#define X(_0, _1, _2) _2,
const uint32_t g_compare_op_gl[] = {
#include "CompareOp.inl"
};
#undef X

#define X(_0, _1, _2) _2,
const uint32_t g_stencil_op_gl[] = {
#include "StencilOp.inl"
};
#undef X

#define X(_0, _1, _2) _2,
const uint32_t g_blend_factor_gl[] = {
#include "BlendFactor.inl"
};
#undef X

#define X(_0, _1, _2) _2,
const uint32_t g_blend_op_gl[] = {
#include "BlendOp.inl"
};
#undef X

const uint32_t g_cull_face_gl[] = {
    0xffffffff, // None
    GL_FRONT,   // Front
    GL_BACK,    // Back
};
static_assert(std::size(g_cull_face_gl) == size_t(eCullFace::_Count), "!");

const uint32_t g_polygon_mode_gl[] = {
    GL_FILL, // Fill
    GL_LINE, // Line
};
static_assert(std::size(g_polygon_mode_gl) == size_t(ePolygonMode::_Count), "!");

const uint32_t g_depth_range_mode_gl[] = {
    GL_ZERO_TO_ONE,        // ZeroToOne
    GL_NEGATIVE_ONE_TO_ONE // NegOneToOne
};
static_assert(std::size(g_depth_range_mode_gl) == size_t(eDepthRangeMode::_Count), "!");
} // namespace Ren

void Ren::RastState::Apply(const RastState *ref) const {
    if (!ref || ref->poly.cull != poly.cull) {
        if (eCullFace(poly.cull) != eCullFace::None) {
            glEnable(GL_CULL_FACE);
            glCullFace(g_cull_face_gl[poly.cull]);
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
        glDepthFunc(g_compare_op_gl[depth.compare_op]);
        glClipControl(GL_LOWER_LEFT, g_depth_range_mode_gl[depth.range_mode]);
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
        glBlendEquationSeparate(g_blend_op_gl[blend.color_op], g_blend_op_gl[blend.alpha_op]);
        glBlendFuncSeparate(g_blend_factor_gl[blend.src_color], g_blend_factor_gl[blend.dst_color],
                            g_blend_factor_gl[blend.src_alpha], g_blend_factor_gl[blend.dst_alpha]);
    }

    if (!ref || ref->stencil != stencil) {
        if (stencil.enabled) {
            glEnable(GL_STENCIL_TEST);
        } else {
            glDisable(GL_STENCIL_TEST);
        }
        glStencilMask(stencil.write_mask);
        glStencilOp(g_stencil_op_gl[int(stencil.stencil_fail)], g_stencil_op_gl[int(stencil.depth_fail)],
                    g_stencil_op_gl[int(stencil.pass)]);
        glStencilFunc(g_compare_op_gl[stencil.compare_op], stencil.reference, stencil.compare_mask);
    }

#if !defined(__ANDROID__)
    if (!ref || ref->poly.mode != poly.mode) {
        glPolygonMode(GL_FRONT_AND_BACK, g_polygon_mode_gl[poly.mode]);
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
