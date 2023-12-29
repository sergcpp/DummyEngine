#include "SWdraw.h"

#include <assert.h>

#include "SWcontext.h"

extern SWcontext *sw_cur_context;

void swVertexAttribPointer(const SWuint index, const SWint size, const SWuint stride,
                           const void *pointer) {
    SWprogram *p = &sw_cur_context->programs[sw_cur_context->cur_program];
    swProgSetVtxAttribPointer(p, sw_cur_context, index, size, stride, pointer);
}

void swRegisterUniform(const SWint index, const SWenum type) {
    swCtxRegisterUniform(sw_cur_context, index, type);
}

void swRegisterUniformv(const SWint index, const SWenum type, const SWint num) {
    swCtxRegisterUniformv(sw_cur_context, index, type, num);
}

void swSetUniform(const SWint index, const SWenum type, const void *data) {
    swCtxSetUniform(sw_cur_context, index, type, data);
}

void swSetUniformv(const SWint index, const SWenum type, const SWint num,
                   const void *data) {
    swCtxSetUniformv(sw_cur_context, index, type, num, data);
}

void swDrawArrays(const SWenum prim_type, const SWuint first, const SWuint count) {
    SWprogram *p = &sw_cur_context->programs[sw_cur_context->cur_program];
    if (prim_type == SW_LINES) {
        swProgDrawLinesArray(p, sw_cur_context, first, count);
    } else if (prim_type == SW_LINE_STRIP) {
        swProgDrawLineStripArray(p, sw_cur_context, first, count);
    } else if (prim_type == SW_CURVES) {
        swProgDrawCurvesArray(p, sw_cur_context, first, count);
    } else if (prim_type == SW_CURVE_STRIP) {
        swProgDrawCurveStripArray(p, sw_cur_context, first, count);
    } else if (prim_type == SW_TRIANGLES) {
        swProgDrawTrianglesArray(p, sw_cur_context, first, count);
    } else if (prim_type == SW_TRIANGLE_STRIP) {
        swProgDrawTriangleStripArray(p, sw_cur_context, first, count);
    } else {
        assert(0);
    }
}

void swDrawElements(const SWenum prim_type, const SWuint count, const SWenum type,
                    const void *indices) {
    SWprogram *p = &sw_cur_context->programs[sw_cur_context->cur_program];
    if (prim_type == SW_LINES) {
        swProgDrawLinesIndexed(p, sw_cur_context, count, type, indices);
    } else if (prim_type == SW_LINE_STRIP) {
        swProgDrawLineStripIndexed(p, sw_cur_context, count, type, indices);
    } else if (prim_type == SW_CURVES) {
        swProgDrawCurvesIndexed(p, sw_cur_context, count, type, indices);
    } else if (prim_type == SW_CURVE_STRIP) {
        swProgDrawCurveStripIndexed(p, sw_cur_context, count, type, indices);
    } else if (prim_type == SW_TRIANGLES) {
        swProgDrawTrianglesIndexed(p, sw_cur_context, count, type, indices);
    } else if (prim_type == SW_TRIANGLE_STRIP) {
        swProgDrawTriangleStripIndexed(p, sw_cur_context, count, type, indices);
    } else {
        assert(0);
    }
}