#include "RastState.h"

#include "GL.h"

namespace Ren {
const uint32_t gl_cull_face[] = {
    GL_FRONT, // Front
    GL_BACK,  // Back
};
static_assert(sizeof(gl_cull_face) / sizeof(gl_cull_face[0]) == size_t(eCullFace::_Count),
              "!");

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
static_assert(sizeof(gl_blend_factor) / sizeof(gl_blend_factor[0]) ==
                  size_t(eBlendFactor::_Count),
              "!");

const uint32_t gl_test_func[] = {
    GL_ALWAYS,   // Always
    GL_NEVER,    // Never
    GL_LESS,     // Less
    GL_EQUAL,    // Equal
    GL_GREATER,  // Greater
    GL_LEQUAL,   // LEqual
    GL_NOTEQUAL, // NotEqual
    GL_GEQUAL    // GEqual
};
static_assert(sizeof(gl_test_func) / sizeof(gl_test_func[0]) == size_t(eTestFunc::_Count),
              "!");

const uint32_t gl_stencil_op[] = {
    GL_KEEP,    // Keep
    GL_ZERO,    // Zero
    GL_REPLACE, // Replace
    GL_INCR,    // Incr
    GL_DECR,    // Decr
    GL_INVERT   // Invert
};
static_assert(sizeof(gl_stencil_op) / sizeof(gl_stencil_op[0]) ==
                  size_t(eStencilOp::_Count),
              "!");

#ifndef __ANDROID__
const uint32_t gl_polygon_mode[] = {
    GL_FILL, // Fill
    GL_LINE, // Line
};
static_assert(sizeof(gl_polygon_mode) / sizeof(gl_polygon_mode[0]) ==
                  size_t(ePolygonMode::_Count),
              "!");
#endif

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

eTestFunc test_func_from_gl_enum(GLenum func) {
    if (func == GL_ALWAYS) {
        return eTestFunc::Always;
    } else if (func == GL_NEVER) {
        return eTestFunc::Never;
    } else if (func == GL_LESS) {
        return eTestFunc::Less;
    } else if (func == GL_GREATER) {
        return eTestFunc::Greater;
    } else if (func == GL_LEQUAL) {
        return eTestFunc::LEqual;
    } else if (func == GL_NOTEQUAL) {
        return eTestFunc::NotEqual;
    } else if (func == GL_GEQUAL) {
        return eTestFunc::GEqual;
    }
    return eTestFunc::Always;
}
} // namespace Ren

void Ren::RastState::Apply(const RastState *ref) {
    if (!ref || std::memcmp(&ref->cull_face, &cull_face, sizeof(cull_face)) != 0) {
        if (cull_face.enabled) {
            glEnable(GL_CULL_FACE);
        } else {
            glDisable(GL_CULL_FACE);
        }
        glCullFace(gl_cull_face[int(cull_face.face)]);
    }

    if (!ref || std::memcmp(&ref->depth_test, &depth_test, sizeof(depth_test)) != 0) {
        if (depth_test.enabled) {
            glEnable(GL_DEPTH_TEST);
        } else {
            glDisable(GL_DEPTH_TEST);
        }
        glDepthFunc(gl_test_func[int(depth_test.func)]);
    }

    if (!ref || ref->depth_mask != depth_mask) {
        if (depth_mask) {
            glDepthMask(GL_TRUE);
        } else {
            glDepthMask(GL_FALSE);
        }
    }

    if (!ref || std::memcmp(&ref->blend, &blend, sizeof(blend)) != 0) {
        if (blend.enabled) {
            glEnable(GL_BLEND);
        } else {
            glDisable(GL_BLEND);
        }
        glBlendFunc(gl_blend_factor[int(blend.src)], gl_blend_factor[int(blend.dst)]);
    }

    if (!ref || std::memcmp(&ref->stencil, &stencil, sizeof(stencil)) != 0) {
        if (stencil.enabled) {
            glEnable(GL_STENCIL_TEST);
        } else {
            glDisable(GL_STENCIL_TEST);
        }
        glStencilMask(stencil.mask);
        glStencilOp(gl_stencil_op[int(stencil.stencil_fail)],
                    gl_stencil_op[int(stencil.depth_fail)],
                    gl_stencil_op[int(stencil.pass)]);
        glStencilFunc(gl_test_func[int(stencil.test_func)], stencil.test_ref,
                      stencil.test_mask);
    }

#if !defined(__ANDROID__)
    if (!ref || ref->polygon_mode != polygon_mode) {
        glPolygonMode(GL_FRONT_AND_BACK, gl_polygon_mode[int(polygon_mode)]);
    }
#endif

    if (!ref ||
        std::memcmp(&ref->polygon_offset, &polygon_offset, sizeof(polygon_offset)) != 0) {
        if (polygon_offset.enabled) {
            glEnable(GL_POLYGON_OFFSET_FILL);
        } else {
            glDisable(GL_POLYGON_OFFSET_FILL);
        }
        glPolygonOffset(polygon_offset.factor, polygon_offset.units);
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

    if (!ref || ref->multisample != multisample) {
        if (multisample) {
            glEnable(GL_MULTISAMPLE);
        } else {
            glDisable(GL_MULTISAMPLE);
        }
    }
}
