#include "SWdraw.h"

#include <assert.h>

#include "SWcontext.h"

extern SWcontext *sw_cur_context;

void swVertexAttribPointer(SWuint index, SWint size, SWuint stride, const void *pointer) {
    SWprogram *p = &sw_cur_context->programs[sw_cur_context->cur_program];
    swProgSetVtxAttribPointer(p, sw_cur_context, index, size, stride, pointer);
}

void swRegisterUniform(SWint index, SWenum type) {
    swCtxRegisterUniform(sw_cur_context, index, type);
}

void swRegisterUniformv(SWint index, SWenum type, SWint num) {
    swCtxRegisterUniformv(sw_cur_context, index, type, num);
}

void swSetUniform(SWint index, SWenum type, const void *data) {
    swCtxSetUniform(sw_cur_context, index, type, data);
}

void swSetUniformv(SWint index, SWenum type, SWint num, const void *data) {
    swCtxSetUniformv(sw_cur_context, index, type, num, data);
}

void swDrawArrays(SWenum prim_type, SWuint first, SWuint count) {
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

void swDrawElements(SWenum prim_type, SWuint count, SWenum type, const void *indices) {
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