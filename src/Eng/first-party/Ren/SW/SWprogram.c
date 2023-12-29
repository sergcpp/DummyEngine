#include "SWprogram.h"

#include <assert.h>
#include <string.h>

#include "SWcontext.h"
#include "SWframebuffer.h"
#include "SWrasterize.h"
#include "SWzbuffer.h"

sw_inline SWint _swTypeSize(const SWenum type) {
    SWint size = 0;
    switch (type) {
    case SW_UNSIGNED_BYTE:
        size = sizeof(SWubyte);
        break;
    case SW_UNSIGNED_SHORT:
        size = sizeof(SWushort);
        break;
    case SW_UNSIGNED_INT:
        size = sizeof(SWuint);
        break;
    case SW_INT:
        size = sizeof(SWint);
        break;
    case SW_FLOAT:
        size = sizeof(SWfloat);
        break;
    case SW_VEC2:
        size = 2 * sizeof(SWfloat);
        break;
    case SW_VEC3:
        size = 3 * sizeof(SWfloat);
        break;
    case SW_VEC4:
        size = 4 * sizeof(SWfloat);
        break;
    case SW_MAT3:
        size = 9 * sizeof(SWfloat);
        break;
    case SW_MAT4:
        size = 16 * sizeof(SWfloat);
        break;
    default:
        assert(0);
    }
    return size;
}

sw_inline SWuint _swGetIndex(const SWenum index_type, const SWuint j,
                             const void *indices) {
    SWuint ndx = 0;
    if (index_type == SW_UNSIGNED_BYTE) {
        ndx = (SWuint) * ((SWubyte *)indices + j);
    } else if (index_type == SW_UNSIGNED_SHORT) {
        ndx = (SWuint) * ((SWushort *)indices + j);
    } else if (index_type == SW_UNSIGNED_INT) {
        ndx = *((SWuint *)indices + j);
    } else {
        assert(0);
    }
    return ndx;
}

sw_inline SWfloat _swCurveTolerance(SWcontext *ctx, SWframebuffer *f) {
    return (ctx->curve_tolerance / f->w) * (ctx->curve_tolerance / f->h);
}

/**************************************************************************************************/

void swProgInit(SWprogram *p, SWubyte *uniform_buf, vtx_shader_proc v_proc,
                frag_shader_proc f_proc, SWint v_out_floats) {
    memset(p, 0, sizeof(SWprogram));
    p->uniform_buf = uniform_buf;
    p->v_proc = v_proc;
    p->f_proc = f_proc;
    p->v_out_size = 4 + v_out_floats;
    memset(p->vertex_attributes, 0, SW_MAX_VTX_ATTRIBS * sizeof(SWvtx_attribute));
}

void swProgDestroy(SWprogram *p) { memset(p, 0, sizeof(SWprogram)); }

void swProgSetVtxAttribPointer(SWprogram *p, SWcontext *ctx, const SWuint index,
                               const SWint size, const SWint stride,
                               const void *pointer) {
    assert(index < sizeof(p->vertex_attributes) / sizeof(SWvtx_attribute));
    const SWint binded_arr_buf = ctx->binded_buffers[0];
    SWvtx_attribute *v = &p->vertex_attributes[index];
    v->size = size;
    v->stride = stride ? stride : size;
    if (binded_arr_buf != -1) {
        SWbuffer *b = &ctx->buffers[binded_arr_buf];
        v->data = (char *)b->data + (uintptr_t)pointer;
    } else {
        v->data = (void *)pointer;
    }
}

void swProgDisableVtxAttrib(SWprogram *p, const SWuint index) {
    SWvtx_attribute *v = &p->vertex_attributes[index];
    v->data = NULL;
}

void swProgRegUniform(SWprogram *p, const SWint index, const SWenum type) {
    SWuniform *u = &p->uniforms[index];
    const SWint size = _swTypeSize(type);
    u->type = type;
    assert(!u->data);
    u->data = &p->uniform_buf[p->unifrom_buf_size];
    p->unifrom_buf_size += size;
}

void swProgRegUniformv(SWprogram *p, const SWint index, const SWenum type,
                       const SWint num) {
    SWuniform *u = &p->uniforms[index];
    const SWint size = _swTypeSize(type);
    u->type = type;
    assert(!u->data);
    u->data = &p->uniform_buf[p->unifrom_buf_size];
    p->unifrom_buf_size += size * num;
}

void swProgSetProgramUniform(SWprogram *p, const SWint index, const SWenum type,
                             const void *data) {
    SWuniform *u = &p->uniforms[index];
    const SWint size = _swTypeSize(type);
    assert(u->type == type);
    memcpy(u->data, data, (size_t)size);
}

void swProgSetProgramUniformv(SWprogram *p, const SWint index, const SWenum type,
                              const SWint num, const void *data) {
    SWuniform *u = &p->uniforms[index];
    const SWint size = _swTypeSize(type);
    assert(u->type == type);
    memcpy(u->data, data, (size_t)size * num);
}

void swProgDrawLinesArray(SWprogram *p, SWcontext *ctx, const SWuint first,
                          const SWuint count) {
    SWframebuffer *f = &ctx->framebuffers[ctx->cur_framebuffer];
    SWuint j;

    const SWint b_depth_test = f->zbuf && (ctx->render_flags & DEPTH_TEST_ENABLED);
    const SWint b_depth_write = f->zbuf && (ctx->render_flags & DEPTH_WRITE_ENABLED);
    const SWint interp_mode =
        (ctx->render_flags & PERSPECTIVE_CORRECTION_ENABLED)
            ? ((ctx->render_flags & FAST_PERSPECTIVE_CORRECTION) ? 2 : 1)
            : 0;
    const SWint num_corr_attrs = (interp_mode == 0) ? 3 : p->v_out_size;
    SWfloat vs_out[2][SW_MAX_VTX_ATTRIBS];

    for (j = first; j < first + count; j += 2) {
        /* transform vertices */
        (*p->v_proc)(p->vertex_attributes, j, p->uniforms, vs_out[0]);
        (*p->v_proc)(p->vertex_attributes, j + 1, p->uniforms, vs_out[1]);

        if (!_swClipAndTransformToNDC_Line(vs_out, vs_out, p->v_out_size,
                                           num_corr_attrs)) {
            continue;
        }
        _swProcessLine(p, f, vs_out[0], vs_out[1], b_depth_test, b_depth_write);
    }
}

void swProgDrawLineStripArray(SWprogram *p, SWcontext *ctx, const SWuint first,
                              const SWuint count) {
    SWframebuffer *f = &ctx->framebuffers[ctx->cur_framebuffer];
    SWuint j;

    SWint b_depth_test = f->zbuf && (ctx->render_flags & DEPTH_TEST_ENABLED);
    SWint b_depth_write = f->zbuf && (ctx->render_flags & DEPTH_WRITE_ENABLED);
    const SWint interp_mode =
        (ctx->render_flags & PERSPECTIVE_CORRECTION_ENABLED)
            ? ((ctx->render_flags & FAST_PERSPECTIVE_CORRECTION) ? 2 : 1)
            : 0;
    const SWint num_corr_attrs = (interp_mode == 0) ? 3 : p->v_out_size;

    SWfloat vs_out[2][SW_MAX_VTX_ATTRIBS];
    SWint _1 = 1, _2 = 0;
    (*p->v_proc)(p->vertex_attributes, first, p->uniforms, vs_out[0]);
    for (j = first; j < first + count - 1; j++) {
        sw_swap(_1, _2, SWint);
        /* transform vertices */
        (*p->v_proc)(p->vertex_attributes, j + 1, p->uniforms, vs_out[_2]);

        SWfloat vs_out2[2][SW_MAX_VTX_ATTRIBS];
        if (!_swClipAndTransformToNDC_Line(vs_out, vs_out2, p->v_out_size,
                                           num_corr_attrs))
            continue;
        _swProcessLine(p, f, vs_out2[_1], vs_out2[_2], b_depth_test, b_depth_write);
    }
}

void swProgDrawLinesIndexed(SWprogram *p, SWcontext *ctx, const SWuint count,
                            const SWenum index_type, const void *indices) {
    SWframebuffer *f = &ctx->framebuffers[ctx->cur_framebuffer];
    SWuint j;

    SWint b_depth_test = f->zbuf && (ctx->render_flags & DEPTH_TEST_ENABLED);
    SWint b_depth_write = f->zbuf && (ctx->render_flags & DEPTH_WRITE_ENABLED);
    const SWint interp_mode =
        (ctx->render_flags & PERSPECTIVE_CORRECTION_ENABLED)
            ? ((ctx->render_flags & FAST_PERSPECTIVE_CORRECTION) ? 2 : 1)
            : 0;
    const SWint num_corr_attrs = (interp_mode == 0) ? 3 : p->v_out_size;

    SWuint index1 = 0, index2 = 0;
    SWfloat vs_out[2][SW_MAX_VTX_ATTRIBS];

    if (!count)
        return;

    if (ctx->binded_buffers[1] != -1) {
        SWbuffer *b = &ctx->buffers[ctx->binded_buffers[1]];
        indices = (char *)b->data + (uintptr_t)indices;
    }

    for (j = 0; j < count; j += 2) {
        if (index_type == SW_UNSIGNED_BYTE) {
            index1 = (SWuint) * ((SWubyte *)indices + j);
            index2 = (SWuint) * ((SWubyte *)indices + j + 1);
        } else if (index_type == SW_UNSIGNED_SHORT) {
            index1 = (SWuint) * ((SWushort *)indices + j);
            index2 = (SWuint) * ((SWushort *)indices + j + 1);
        } else if (index_type == SW_UNSIGNED_INT) {
            index1 = *((SWuint *)indices + j);
            index2 = *((SWuint *)indices + j + 1);
        }

        (*p->v_proc)(p->vertex_attributes, index1, p->uniforms, vs_out[0]);
        (*p->v_proc)(p->vertex_attributes, index2, p->uniforms, vs_out[1]);

        if (!_swClipAndTransformToNDC_Line(vs_out, vs_out, p->v_out_size,
                                           num_corr_attrs)) {
            continue;
        }
        _swProcessLine(p, f, vs_out[0], vs_out[1], b_depth_test, b_depth_write);
    }
}

void swProgDrawLineStripIndexed(SWprogram *p, SWcontext *ctx, const SWuint count,
                                const SWenum index_type, const void *indices) {
    SWframebuffer *f = &ctx->framebuffers[ctx->cur_framebuffer];

    const SWint b_depth_test = f->zbuf && (ctx->render_flags & DEPTH_TEST_ENABLED);
    const SWint b_depth_write = f->zbuf && (ctx->render_flags & DEPTH_WRITE_ENABLED);
    const SWint interp_mode =
        (ctx->render_flags & PERSPECTIVE_CORRECTION_ENABLED)
            ? ((ctx->render_flags & FAST_PERSPECTIVE_CORRECTION) ? 2 : 1)
            : 0;
    const SWint num_corr_attrs = (interp_mode == 0) ? 3 : p->v_out_size;

    SWfloat vs_out[2][SW_MAX_VTX_ATTRIBS];
    SWint _1 = 1, _2 = 0;
    SWuint j, index1, index2;

    if (!count) {
        return;
    }

    if (ctx->binded_buffers[1] != -1) {
        SWbuffer *b = &ctx->buffers[ctx->binded_buffers[1]];
        indices = (char *)b->data + (uintptr_t)indices;
    }

    index1 = _swGetIndex(index_type, 0, indices);

    (*p->v_proc)(p->vertex_attributes, index1, p->uniforms, vs_out[0]);
    for (j = 0; j < count - 1; j++) {
        sw_swap(_1, _2, SWint);

        index2 = _swGetIndex(index_type, j + 1, indices);

        /* transform vertices */
        (*p->v_proc)(p->vertex_attributes, index2, p->uniforms, vs_out[_2]);

        SWfloat vs_out2[2][SW_MAX_VTX_ATTRIBS];
        if (!_swClipAndTransformToNDC_Line(vs_out, vs_out2, p->v_out_size,
                                           num_corr_attrs))
            continue;
        _swProcessLine(p, f, vs_out2[_1], vs_out2[_2], b_depth_test, b_depth_write);
    }
}

void swProgDrawCurvesArray(SWprogram *p, SWcontext *ctx, const SWuint first,
                           const SWuint count) {
    SWframebuffer *f = &ctx->framebuffers[ctx->cur_framebuffer];
    SWuint j;

    const SWint b_depth_test = f->zbuf && (ctx->render_flags & DEPTH_TEST_ENABLED);
    const SWint b_depth_write = f->zbuf && (ctx->render_flags & DEPTH_WRITE_ENABLED);
    const SWfloat curve_tolerance = _swCurveTolerance(ctx, f);
    SWfloat vs_out[4][SW_MAX_VTX_ATTRIBS];

    for (j = first; j < first + count; j += 4) {
        /* transform vertices */
        (*p->v_proc)(p->vertex_attributes, j, p->uniforms, vs_out[0]);
        (*p->v_proc)(p->vertex_attributes, j + 1, p->uniforms, vs_out[1]);
        (*p->v_proc)(p->vertex_attributes, j + 2, p->uniforms, vs_out[2]);
        (*p->v_proc)(p->vertex_attributes, j + 3, p->uniforms, vs_out[3]);

        _swProcessCurve(p, f, vs_out, 0, 1, 2, 3, b_depth_test, b_depth_write,
                        curve_tolerance);
    }
}

void swProgDrawCurveStripArray(SWprogram *p, SWcontext *ctx, const SWuint first,
                               const SWuint count) {
    SWframebuffer *f = &ctx->framebuffers[ctx->cur_framebuffer];
    SWuint j;

    const SWint b_depth_test = f->zbuf && (ctx->render_flags & DEPTH_TEST_ENABLED);
    const SWint b_depth_write = f->zbuf && (ctx->render_flags & DEPTH_WRITE_ENABLED);
    const SWfloat curve_tolerance = _swCurveTolerance(ctx, f);
    SWfloat vs_out[4][SW_MAX_VTX_ATTRIBS];
    SWint _0 = 3, _3 = 0;
    (*p->v_proc)(p->vertex_attributes, first, p->uniforms, vs_out[0]);
    for (j = first; j < first + count - 3; j += 3) {
        sw_swap(_0, _3, SWint);
        /* transform vertices */
        (*p->v_proc)(p->vertex_attributes, j + 1, p->uniforms, vs_out[1]);
        (*p->v_proc)(p->vertex_attributes, j + 2, p->uniforms, vs_out[2]);
        (*p->v_proc)(p->vertex_attributes, j + 3, p->uniforms, vs_out[_3]);

        _swProcessCurve(p, f, vs_out, _0, 1, 2, _3, b_depth_test, b_depth_write,
                        curve_tolerance);
    }
}

void swProgDrawCurvesIndexed(SWprogram *p, SWcontext *ctx, const SWuint count,
                             const SWenum index_type, const void *indices) {
    SWframebuffer *f = &ctx->framebuffers[ctx->cur_framebuffer];
    SWuint j;

    const SWint b_depth_test = f->zbuf && (ctx->render_flags & DEPTH_TEST_ENABLED);
    const SWint b_depth_write = f->zbuf && (ctx->render_flags & DEPTH_WRITE_ENABLED);
    const SWfloat curve_tolerance = _swCurveTolerance(ctx, f);

    SWuint index1 = 0, index2 = 0, index3 = 0, index4 = 0;
    SWfloat vs_out[4][SW_MAX_VTX_ATTRIBS];

    if (!count)
        return;

    if (ctx->binded_buffers[1] != -1) {
        SWbuffer *b = &ctx->buffers[ctx->binded_buffers[1]];
        indices = (char *)b->data + (uintptr_t)indices;
    }

    for (j = 0; j < count; j += 4) {
        if (index_type == SW_UNSIGNED_BYTE) {
            index1 = (SWuint) * ((SWubyte *)indices + j);
            index2 = (SWuint) * ((SWubyte *)indices + j + 1);
            index3 = (SWuint) * ((SWubyte *)indices + j + 2);
            index4 = (SWuint) * ((SWubyte *)indices + j + 3);
        } else if (index_type == SW_UNSIGNED_SHORT) {
            index1 = (SWuint) * ((SWushort *)indices + j);
            index2 = (SWuint) * ((SWushort *)indices + j + 1);
            index3 = (SWuint) * ((SWushort *)indices + j + 2);
            index4 = (SWuint) * ((SWushort *)indices + j + 3);
        } else if (index_type == SW_UNSIGNED_INT) {
            index1 = *((SWuint *)indices + j);
            index2 = *((SWuint *)indices + j + 1);
            index3 = *((SWuint *)indices + j + 2);
            index4 = *((SWuint *)indices + j + 3);
        }

        (*p->v_proc)(p->vertex_attributes, index1, p->uniforms, vs_out[0]);
        (*p->v_proc)(p->vertex_attributes, index2, p->uniforms, vs_out[1]);
        (*p->v_proc)(p->vertex_attributes, index3, p->uniforms, vs_out[2]);
        (*p->v_proc)(p->vertex_attributes, index4, p->uniforms, vs_out[3]);

        _swProcessCurve(p, f, vs_out, 0, 1, 2, 3, b_depth_test, b_depth_write,
                        curve_tolerance);
    }
}

void swProgDrawCurveStripIndexed(SWprogram *p, SWcontext *ctx, const SWuint count,
                                 const SWenum index_type, const void *indices) {
    SWframebuffer *f = &ctx->framebuffers[ctx->cur_framebuffer];

    const SWint b_depth_test = f->zbuf && (ctx->render_flags & DEPTH_TEST_ENABLED);
    const SWint b_depth_write = f->zbuf && (ctx->render_flags & DEPTH_WRITE_ENABLED);
    const SWfloat curve_tolerance = _swCurveTolerance(ctx, f);

    SWfloat vs_out[4][SW_MAX_VTX_ATTRIBS];
    SWint _0 = 3, _3 = 0;
    SWuint j, index1, index2, index3, index4;

    if (!count) {
        return;
    }

    if (ctx->binded_buffers[1] != -1) {
        SWbuffer *b = &ctx->buffers[ctx->binded_buffers[1]];
        indices = (char *)b->data + (uintptr_t)indices;
    }

    index1 = _swGetIndex(index_type, 0, indices);

    (*p->v_proc)(p->vertex_attributes, index1, p->uniforms, vs_out[0]);
    for (j = 0; j < count - 1; j += 3) {
        sw_swap(_0, _3, SWint);

        index2 = _swGetIndex(index_type, j + 1, indices);
        index3 = _swGetIndex(index_type, j + 2, indices);
        index4 = _swGetIndex(index_type, j + 3, indices);

        /* transform vertices */
        (*p->v_proc)(p->vertex_attributes, index2, p->uniforms, vs_out[1]);
        (*p->v_proc)(p->vertex_attributes, index3, p->uniforms, vs_out[2]);
        (*p->v_proc)(p->vertex_attributes, index4, p->uniforms, vs_out[_3]);

        _swProcessCurve(p, f, vs_out, _0, 1, 2, _3, b_depth_test, b_depth_write,
                        curve_tolerance);
    }
}

void swProgDrawTrianglesArray(SWprogram *p, SWcontext *ctx, const SWuint first,
                              const SWuint count) {
    SWframebuffer *f = &ctx->framebuffers[ctx->cur_framebuffer];
    const SWint b_depth_test = f->zbuf && (ctx->render_flags & DEPTH_TEST_ENABLED);
    const SWint b_depth_write = f->zbuf && (ctx->render_flags & DEPTH_WRITE_ENABLED);
    const SWint b_blend = (ctx->render_flags & BLEND_ENABLED);
    const SWint interp_mode =
        (ctx->render_flags & PERSPECTIVE_CORRECTION_ENABLED)
            ? ((ctx->render_flags & FAST_PERSPECTIVE_CORRECTION) ? 2 : 1)
            : 0;
    const SWint num_corr_attrs = (interp_mode == 0) ? 3 : p->v_out_size;

    SWint i;
    SWuint j;
    SWfloat vs_out[16][SW_MAX_VTX_ATTRIBS];

    if (!count) {
        return;
    }

    for (j = first; j < first + count; j += 3) {
        /* transform vertices */
        (*p->v_proc)(p->vertex_attributes, j, p->uniforms, vs_out[0]);
        (*p->v_proc)(p->vertex_attributes, j + 1, p->uniforms, vs_out[1]);
        (*p->v_proc)(p->vertex_attributes, j + 2, p->uniforms, vs_out[2]);

        /* clipping may result in creating additional vertices */
        SWint out_verts[16];
        const SWint num_verts = _swClipAndTransformToNDC_Tri(
            vs_out, vs_out, 0, 1, 2, out_verts, p->v_out_size, num_corr_attrs);

        for (i = 1; i < num_verts - 1; i++) {
            if (interp_mode == 0) {
                _swProcessTriangle_nocorrect(p, f, vs_out, out_verts[0], out_verts[i],
                                             out_verts[i + 1], b_depth_test,
                                             b_depth_write, b_blend);
            } else if (interp_mode == 1) {
                _swProcessTriangle_correct(p, f, vs_out, out_verts[0], out_verts[i],
                                           out_verts[i + 1], b_depth_test, b_depth_write,
                                           b_blend);
            } else {
                _swProcessTriangle_fast(p, f, vs_out, out_verts[0], out_verts[i],
                                        out_verts[i + 1], b_depth_test, b_depth_write,
                                        b_blend);
            }
        }
    }
}

void swProgDrawTriangleStripArray(SWprogram *p, SWcontext *ctx, const SWuint first,
                                  const SWuint count) {
    SWframebuffer *f = &ctx->framebuffers[ctx->cur_framebuffer];
    const SWint b_depth_test = f->zbuf && (ctx->render_flags & DEPTH_TEST_ENABLED);
    const SWint b_depth_write = f->zbuf && (ctx->render_flags & DEPTH_WRITE_ENABLED);
    const SWint b_blend = (ctx->render_flags & BLEND_ENABLED);
    const SWint interp_mode =
        (ctx->render_flags & PERSPECTIVE_CORRECTION_ENABLED)
            ? ((ctx->render_flags & FAST_PERSPECTIVE_CORRECTION) ? 2 : 1)
            : 0;
    const SWint num_corr_attrs = (interp_mode == 0) ? 3 : p->v_out_size;

    SWfloat vs_out[3][SW_MAX_VTX_ATTRIBS];
    SWint _0 = 0, _1 = 1, _2 = 2, is_odd = 1;
    (*p->v_proc)(p->vertex_attributes, first, p->uniforms, vs_out[0]);
    (*p->v_proc)(p->vertex_attributes, first + 1, p->uniforms, vs_out[1]);

    if (!count) {
        return;
    }

    for (SWuint j = first; j < first + count - 2; j++) {
        is_odd = !is_odd;

        (*p->v_proc)(p->vertex_attributes, j + 2, p->uniforms, vs_out[_2]);

        /* clipping may result in creating additional vertices */
        SWfloat vs_out2[16][SW_MAX_VTX_ATTRIBS];
        SWint out_verts[16];
        const SWint num_verts = _swClipAndTransformToNDC_Tri(
            vs_out, vs_out2, _0, _1, _2, out_verts, p->v_out_size, num_corr_attrs);

        for (SWint i = 1; i < num_verts - 1; i++) {
            if (interp_mode == 0) {
                _swProcessTriangle_nocorrect(p, f, vs_out2, out_verts[0], out_verts[i],
                                             out_verts[i + 1], b_depth_test,
                                             b_depth_write, b_blend);
            } else if (interp_mode == 1) {
                _swProcessTriangle_correct(p, f, vs_out2, out_verts[0], out_verts[i],
                                           out_verts[i + 1], b_depth_test, b_depth_write,
                                           b_blend);
            } else {
                _swProcessTriangle_fast(p, f, vs_out2, out_verts[0], out_verts[i],
                                        out_verts[i + 1], b_depth_test, b_depth_write,
                                        b_blend);
            }
        }

        if (is_odd) {
            sw_swap(_0, _1, SWint);
        }
        sw_rotate_lefti(_0, _1, _2);
        if (!is_odd) {
            sw_swap(_0, _1, SWint);
        }
    }
}

void swProgDrawTrianglesIndexed(SWprogram *p, SWcontext *ctx, const SWuint count,
                                const SWenum index_type, const void *indices) {
    SWframebuffer *f = &ctx->framebuffers[ctx->cur_framebuffer];

    const SWint b_depth_test = f->zbuf && (ctx->render_flags & DEPTH_TEST_ENABLED);
    const SWint b_depth_write = f->zbuf && (ctx->render_flags & DEPTH_WRITE_ENABLED);
    const SWint b_blend = (ctx->render_flags & BLEND_ENABLED);
    const SWint interp_mode =
        (ctx->render_flags & PERSPECTIVE_CORRECTION_ENABLED)
            ? ((ctx->render_flags & FAST_PERSPECTIVE_CORRECTION) ? 2 : 1)
            : 0;
    const SWint num_corr_attrs = (interp_mode == 0) ? 3 : p->v_out_size;

    SWuint index1 = 0, index2 = 0, index3 = 0;
    SWfloat vs_out[16][SW_MAX_VTX_ATTRIBS];

    if (!count) {
        return;
    }

    if (ctx->binded_buffers[1] != -1) {
        SWbuffer *b = &ctx->buffers[ctx->binded_buffers[1]];
        indices = (char *)b->data + (uintptr_t)indices;
    }

    for (SWuint j = 0; j < count; j += 3) {
        if (index_type == SW_UNSIGNED_BYTE) {
            index1 = (SWuint) * ((SWubyte *)indices + j);
            index2 = (SWuint) * ((SWubyte *)indices + j + 1);
            index3 = (SWuint) * ((SWubyte *)indices + j + 2);
        } else if (index_type == SW_UNSIGNED_SHORT) {
            index1 = (SWuint) * ((SWushort *)indices + j);
            index2 = (SWuint) * ((SWushort *)indices + j + 1);
            index3 = (SWuint) * ((SWushort *)indices + j + 2);
        } else if (index_type == SW_UNSIGNED_INT) {
            index1 = *((SWuint *)indices + j);
            index2 = *((SWuint *)indices + j + 1);
            index3 = *((SWuint *)indices + j + 2);
        }

        /* transform vertices */
        (*p->v_proc)(p->vertex_attributes, index1, p->uniforms, vs_out[0]);
        (*p->v_proc)(p->vertex_attributes, index2, p->uniforms, vs_out[1]);
        (*p->v_proc)(p->vertex_attributes, index3, p->uniforms, vs_out[2]);

        /* clipping may result in creating additional vertices */
        SWint out_verts[16];
        const SWint num_verts = _swClipAndTransformToNDC_Tri(
            vs_out, vs_out, 0, 1, 2, out_verts, p->v_out_size, num_corr_attrs);

        for (SWint i = 1; i < num_verts - 1; i++) {
            if (interp_mode == 0) {
                _swProcessTriangle_nocorrect(p, f, vs_out, out_verts[0], out_verts[i],
                                             out_verts[i + 1], b_depth_test,
                                             b_depth_write, b_blend);
            } else if (interp_mode == 1) {
                _swProcessTriangle_correct(p, f, vs_out, out_verts[0], out_verts[i],
                                           out_verts[i + 1], b_depth_test, b_depth_write,
                                           b_blend);
            } else {
                _swProcessTriangle_fast(p, f, vs_out, out_verts[0], out_verts[i],
                                        out_verts[i + 1], b_depth_test, b_depth_write,
                                        b_blend);
            }
        }
    }
}

void swProgDrawTriangleStripIndexed(SWprogram *p, SWcontext *ctx, const SWuint count,
                                    const SWenum index_type, const void *indices) {
    SWframebuffer *f = &ctx->framebuffers[ctx->cur_framebuffer];
    const SWint b_depth_test = f->zbuf && (ctx->render_flags & DEPTH_TEST_ENABLED);
    const SWint b_depth_write = f->zbuf && (ctx->render_flags & DEPTH_WRITE_ENABLED);
    const SWint b_blend = (ctx->render_flags & BLEND_ENABLED);
    const SWint interp_mode =
        (ctx->render_flags & PERSPECTIVE_CORRECTION_ENABLED)
            ? ((ctx->render_flags & FAST_PERSPECTIVE_CORRECTION) ? 2 : 1)
            : 0;
    const SWint num_corr_attrs = (interp_mode == 0) ? 3 : p->v_out_size;

    SWuint index1 = 0, index2 = 0;

    if (!count)
        return;

    if (ctx->binded_buffers[1] != -1) {
        SWbuffer *b = &ctx->buffers[ctx->binded_buffers[1]];
        indices = (char *)b->data + (uintptr_t)indices;
    }

    if (index_type == SW_UNSIGNED_BYTE) {
        index1 = (SWuint) * ((SWubyte *)indices + 0);
        index2 = (SWuint) * ((SWubyte *)indices + 0 + 1);
    } else if (index_type == SW_UNSIGNED_SHORT) {
        index1 = (SWuint) * ((SWushort *)indices + 0);
        index2 = (SWuint) * ((SWushort *)indices + 0 + 1);
    } else if (index_type == SW_UNSIGNED_INT) {
        index1 = *((SWuint *)indices + 0);
        index2 = *((SWuint *)indices + 0 + 1);
    }

    SWfloat vs_out[3][SW_MAX_VTX_ATTRIBS];
    SWint _0 = 0, _1 = 1, _2 = 2, is_odd = 1;

    (*p->v_proc)(p->vertex_attributes, index1, p->uniforms, vs_out[0]);
    (*p->v_proc)(p->vertex_attributes, index2, p->uniforms, vs_out[1]);

    for (SWuint j = 0; j < count - 2; j++) {
        is_odd = !is_odd;
        SWuint index3 = _swGetIndex(index_type, j + 2, indices);

        (*p->v_proc)(p->vertex_attributes, index3, p->uniforms, vs_out[_2]);

        /* clipping may result in creating additional vertices */
        SWfloat vs_out2[16][SW_MAX_VTX_ATTRIBS];
        SWint out_verts[16];
        const SWint num_verts = _swClipAndTransformToNDC_Tri(
            vs_out, vs_out2, _0, _1, _2, out_verts, p->v_out_size, num_corr_attrs);

        for (SWint i = 1; i < num_verts - 1; i++) {
            if (interp_mode == 0) {
                _swProcessTriangle_nocorrect(p, f, vs_out2, out_verts[0], out_verts[i],
                                             out_verts[i + 1], b_depth_test,
                                             b_depth_write, b_blend);
            } else if (interp_mode == 1) {
                _swProcessTriangle_correct(p, f, vs_out2, out_verts[0], out_verts[i],
                                           out_verts[i + 1], b_depth_test, b_depth_write,
                                           b_blend);
            } else {
                _swProcessTriangle_fast(p, f, vs_out2, out_verts[0], out_verts[i],
                                        out_verts[i + 1], b_depth_test, b_depth_write,
                                        b_blend);
            }
        }

        if (is_odd) {
            sw_swap(_0, _1, SWint);
        }
        sw_rotate_lefti(_0, _1, _2);
        if (!is_odd) {
            sw_swap(_0, _1, SWint);
        }
    }
}
