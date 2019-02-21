#include "SWcore.h"

#include <assert.h>
#include <stdlib.h>

#include "SWcontext.h"
#include "SWcpu.h"

/*************************************************************************************************/

SWcontext *sw_cur_context = NULL;

SWcontext *swCreateContext(SWint w, SWint h) {
    SWcontext *ctx = (SWcontext *)calloc(1, sizeof(SWcontext));
    swCtxInit(ctx, w, h);
    if (!sw_cur_context) {
        swMakeCurrent(ctx);
    }
    return ctx;
}

void swMakeCurrent(SWcontext *ctx) {
    sw_cur_context = ctx;
}

void swDeleteContext(SWcontext *ctx) {
    if (ctx == sw_cur_context) {
        sw_cur_context = NULL;
    }
    swCtxDestroy(ctx);
    free(ctx);
}

/*************************************************************************************************/

SWint swCreateBuffer() {
    return swCtxCreateBuffer(sw_cur_context);
}

void swDeleteBuffer(SWint buf) {
    swCtxDeleteBuffer(sw_cur_context, buf);
}

void swBindBuffer(SWenum type, SWint buf) {
    swCtxBindBuffer(sw_cur_context, type, buf);
}

void swBufferData(SWenum type, SWuint size, const void *data) {
    swCtxBufferData(sw_cur_context, type, size, data);
}

void swBufferSubData(SWenum type, SWuint offset, SWuint size, const void *data) {
    swCtxBufferSubData(sw_cur_context, type, offset, size, data);
}

void swGetBufferSubData(SWenum type, SWuint offset, SWuint size, void *data) {
    swCtxGetBufferSubData(sw_cur_context, type, offset, size, data);
}

/*************************************************************************************************/

SWint swCreateFramebuffer(SWenum type, SWint w, SWint h, SWint with_depth) {
    return swCtxCreateFramebuffer(sw_cur_context, type, w, h, with_depth);
}

void swDeleteFramebuffer(SWint i) {
    swCtxDeleteFramebuffer(sw_cur_context, i);
}

void swBindFramebuffer(SWint i) {
    swCtxBindFramebuffer(sw_cur_context, i);
}

SWint swGetCurFramebuffer() {
    return swCtxGetCurFramebuffer(sw_cur_context);
}

const void *swGetPixelDataRef(SWint i) {
    return swCtxGetPixelDataRef(sw_cur_context, i);
}

const void *swGetDepthDataRef(SWint i) {
    return swCtxGetDepthDataRef(sw_cur_context, i);
}

void swBlitPixels(SWint x, SWint y, SWint pitch, SWenum type, SWenum mode, SWint w, SWint h, const void *pixels, SWfloat scale) {
    swCtxBlitPixels(sw_cur_context, x, y, pitch, type, mode, w, h, pixels, scale);
}

void swBlitTexture(SWint x, SWint y, SWfloat scale) {
    swCtxBlitTexture(sw_cur_context, x, y, scale);
}

/*************************************************************************************************/

SWint swCreateTexture() {
    return swCtxCreateTexture(sw_cur_context);
}

void swDeleteTexture(SWint tex) {
    swCtxDeleteTexture(sw_cur_context, tex);
}

void swActiveTexture(SWint slot) {
    swCtxActiveTexture(sw_cur_context, slot);
}

void swBindTexture(SWint tex) {
    swCtxBindTexture(sw_cur_context, tex);
}

void swTexImage2D(SWenum mode, SWenum type, SWint w, SWint h, const void *pixels) {
    swCtxTexImage2D(sw_cur_context, mode, type, w, h, pixels);
}

void swTexImage2DMove_malloced(SWenum mode, SWenum type, SWint w, SWint h, void *pixels) {
    swCtxTexImage2DMove_malloced(sw_cur_context, mode, type, w, h, pixels);
}

void swTexImage2DConst(SWenum mode, SWenum type, SWint w, SWint h, void *pixels) {
    swCtxTexImage2DConst(sw_cur_context, mode, type, w, h, pixels);
}

void swTexture(SWint slot, const SWfloat *uv, SWfloat *col) {
    SWtexture *t = &sw_cur_context->textures[sw_cur_context->binded_textures[slot]];
    swTexGetColorFloat_RGBA(t, uv[0], uv[1], col);
}

/*************************************************************************************************/

SWint swCreateProgram() {
    return swCtxCreateProgram(sw_cur_context);
}

void swInitProgram(vtx_shader_proc v_proc, frag_shader_proc f_proc, SWint v_out_floats) {
    swCtxInitProgram(sw_cur_context, v_proc, f_proc, v_out_floats);
}

void swDeleteProgram(SWint program) {
    swCtxDeleteProgram(sw_cur_context, program);
}

void swUseProgram(SWint program) {
    swCtxUseProgram(sw_cur_context, program);
}

/*************************************************************************************************/

void swEnable(SWenum func) {
    if (func == SW_DEPTH_TEST) {
        sw_cur_context->render_flags |= DEPTH_TEST_ENABLED;
    } else if (func == SW_DEPTH_WRITE) {
        sw_cur_context->render_flags |= DEPTH_WRITE_ENABLED;
    } else if (func == SW_BLEND) {
        sw_cur_context->render_flags |= BLEND_ENABLED;
    } else if (func == SW_PERSPECTIVE_CORRECTION) {
        sw_cur_context->render_flags |= PERSPECTIVE_CORRECTION_ENABLED;
    } else if (func == SW_FAST_PERSPECTIVE_CORRECTION) {
        sw_cur_context->render_flags |= FAST_PERSPECTIVE_CORRECTION;
    }
}

void swDisable(SWenum func) {
    if (func == SW_DEPTH_TEST) {
        sw_cur_context->render_flags &= ~DEPTH_TEST_ENABLED;
    } else if (func == SW_DEPTH_WRITE) {
        sw_cur_context->render_flags &= ~DEPTH_WRITE_ENABLED;
    } else if (func == SW_BLEND) {
        sw_cur_context->render_flags &= ~BLEND_ENABLED;
    } else if (func == SW_PERSPECTIVE_CORRECTION) {
        sw_cur_context->render_flags &= ~PERSPECTIVE_CORRECTION_ENABLED;
    } else if (func == SW_FAST_PERSPECTIVE_CORRECTION) {
        sw_cur_context->render_flags &= ~FAST_PERSPECTIVE_CORRECTION;
    }
}

SWint swIsEnabled(SWenum func) {
    if (func == SW_DEPTH_TEST) {
        return sw_cur_context->render_flags & DEPTH_TEST_ENABLED;
    } else if (func == SW_DEPTH_WRITE) {
        return sw_cur_context->render_flags & DEPTH_WRITE_ENABLED;
    } else if (func == SW_BLEND) {
        return sw_cur_context->render_flags & BLEND_ENABLED;
    } else if (func == SW_PERSPECTIVE_CORRECTION) {
        return sw_cur_context->render_flags & PERSPECTIVE_CORRECTION_ENABLED;
    } else if (func == SW_FAST_PERSPECTIVE_CORRECTION) {
        return sw_cur_context->render_flags & FAST_PERSPECTIVE_CORRECTION;
    }
    return 0;
}

void swClearColor(SWfloat r, SWfloat g, SWfloat b, SWfloat a) {
    SWframebuffer *f = &sw_cur_context->framebuffers[sw_cur_context->cur_framebuffer];
    swFbufClearColorFloat(f, r, g, b, a);
}

void swClearDepth(SWfloat val) {
    SWframebuffer *f = &sw_cur_context->framebuffers[sw_cur_context->cur_framebuffer];
    swFbufClearDepth(f, val);
}

SWint swGetInteger(SWenum what) {
    if (what == SW_MAX_VERTEX_UNIFORM_VECTORS) {
        return SW_UNIFORM_BUF_SIZE / (4 * sizeof(SWfloat));
    } else if (what == SW_NUM_CPUS) {
        return sw_cur_context->cpu_info.num_cpus;
    } else if (what == SW_PHYSICAL_MEMORY) {
        return (SWint)(sw_cur_context->cpu_info.physical_memory * 1024);
    } else {
        return -1;
    }
}

SWfloat swGetFloat(SWenum what) {
    if (what == SW_PHYSICAL_MEMORY) {
        return sw_cur_context->cpu_info.physical_memory;
    } else {
        return 0;
    }
}

const char *swGetString(SWenum what) {
    if (what == SW_CPU_VENDOR) {
        return sw_cur_context->cpu_info.vendor;
    } else if (what == SW_CPU_MODEL) {
        return sw_cur_context->cpu_info.model;
    } else {
        return "Undefined";
    }
}

void swSetFloat(SWenum what, SWfloat val) {
    if (what == SW_CURVE_TOLERANCE) {
        sw_cur_context->curve_tolerance = val;
    }
}