#include "SWcore.h"

#include <assert.h>
#include <stdlib.h>

#include "SWcontext.h"
#include "SWcpu.h"

/***************************************************************************************/

SWcontext *sw_cur_context = NULL;

SWcontext *swCreateContext(const SWint w, const SWint h) {
    SWcontext *ctx = (SWcontext *)calloc(1, sizeof(SWcontext));
    swCtxInit(ctx, w, h);
    if (!sw_cur_context) {
        swMakeCurrent(ctx);
    }
    return ctx;
}

void swMakeCurrent(SWcontext *ctx) { sw_cur_context = ctx; }

void swDeleteContext(SWcontext *ctx) {
    if (ctx == sw_cur_context) {
        sw_cur_context = NULL;
    }
    swCtxDestroy(ctx);
    free(ctx);
}

/***************************************************************************************/

SWint swCreateBuffer() { return swCtxCreateBuffer(sw_cur_context); }

void swDeleteBuffer(const SWint buf) { swCtxDeleteBuffer(sw_cur_context, buf); }

void swBindBuffer(const SWenum type, const SWint buf) {
    swCtxBindBuffer(sw_cur_context, type, buf);
}

void swBufferData(const SWenum type, const SWuint size, const void *data) {
    swCtxBufferData(sw_cur_context, type, size, data);
}

void swBufferSubData(const SWenum type, const SWuint offset, const SWuint size,
                     const void *data) {
    swCtxBufferSubData(sw_cur_context, type, offset, size, data);
}

void swGetBufferSubData(const SWenum type, const SWuint offset, const SWuint size,
                        void *data) {
    swCtxGetBufferSubData(sw_cur_context, type, offset, size, data);
}

/***************************************************************************************/

SWint swCreateFramebuffer(const SWenum type, const SWint w, const SWint h,
                          const SWint with_depth) {
    return swCtxCreateFramebuffer(sw_cur_context, type, w, h, with_depth);
}

void swDeleteFramebuffer(SWint i) { swCtxDeleteFramebuffer(sw_cur_context, i); }

void swBindFramebuffer(SWint i) { swCtxBindFramebuffer(sw_cur_context, i); }

SWint swGetCurFramebuffer() { return swCtxGetCurFramebuffer(sw_cur_context); }

const void *swGetPixelDataRef(SWint i) { return swCtxGetPixelDataRef(sw_cur_context, i); }

const void *swGetDepthDataRef(SWint i) { return swCtxGetDepthDataRef(sw_cur_context, i); }

void swBlitPixels(const SWint x, const SWint y, const SWint pitch, const SWenum type,
                  const SWenum mode, const SWint w, const SWint h, const void *pixels,
                  const SWfloat scale) {
    swCtxBlitPixels(sw_cur_context, x, y, pitch, type, mode, w, h, pixels, scale);
}

void swBlitTexture(const SWint x, const SWint y, const SWfloat scale) {
    swCtxBlitTexture(sw_cur_context, x, y, scale);
}

/***************************************************************************************/

SWint swCreateTexture() { return swCtxCreateTexture(sw_cur_context); }

void swDeleteTexture(const SWint tex) { swCtxDeleteTexture(sw_cur_context, tex); }

void swActiveTexture(const SWint slot) { swCtxActiveTexture(sw_cur_context, slot); }

void swBindTexture(const SWint tex) { swCtxBindTexture(sw_cur_context, tex); }

void swTexImage2D(const SWenum mode, const SWenum type, const SWint w, const SWint h,
                  const void *pixels) {
    swCtxTexImage2D(sw_cur_context, mode, type, w, h, pixels);
}

void swTexImage2DMove_malloced(const SWenum mode, const SWenum type, const SWint w,
                               const SWint h, void *pixels) {
    swCtxTexImage2DMove_malloced(sw_cur_context, mode, type, w, h, pixels);
}

void swTexImage2DConst(const SWenum mode, const SWenum type, const SWint w, const SWint h,
                       void *pixels) {
    swCtxTexImage2DConst(sw_cur_context, mode, type, w, h, pixels);
}

void swTexture(const SWint slot, const SWfloat *uv, SWfloat *col) {
    SWtexture *t = &sw_cur_context->textures[sw_cur_context->binded_textures[slot]];
    swTexGetColorFloat_RGBA(t, uv[0], uv[1], col);
}

/***************************************************************************************/

SWint swCreateProgram() { return swCtxCreateProgram(sw_cur_context); }

void swInitProgram(vtx_shader_proc v_proc, frag_shader_proc f_proc, SWint v_out_floats) {
    swCtxInitProgram(sw_cur_context, v_proc, f_proc, v_out_floats);
}

void swDeleteProgram(const SWint program) { swCtxDeleteProgram(sw_cur_context, program); }

void swUseProgram(const SWint program) { swCtxUseProgram(sw_cur_context, program); }

/***************************************************************************************/

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

void swDisable(const SWenum func) {
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

SWint swIsEnabled(const SWenum func) {
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

void swClearColor(const SWfloat r, const SWfloat g, const SWfloat b, const SWfloat a) {
    SWframebuffer *f = &sw_cur_context->framebuffers[sw_cur_context->cur_framebuffer];
    swFbufClearColorFloat(f, r, g, b, a);
}

void swClearDepth(const SWfloat val) {
    SWframebuffer *f = &sw_cur_context->framebuffers[sw_cur_context->cur_framebuffer];
    swFbufClearDepth(f, val);
}

SWint swGetInteger(const SWenum what) {
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

SWfloat swGetFloat(const SWenum what) {
    if (what == SW_PHYSICAL_MEMORY) {
        return sw_cur_context->cpu_info.physical_memory;
    } else {
        return 0;
    }
}

const char *swGetString(const SWenum what) {
    if (what == SW_CPU_VENDOR) {
        return sw_cur_context->cpu_info.vendor;
    } else if (what == SW_CPU_MODEL) {
        return sw_cur_context->cpu_info.model;
    } else {
        return "Undefined";
    }
}

void swSetFloat(const SWenum what, const SWfloat val) {
    if (what == SW_CURVE_TOLERANCE) {
        sw_cur_context->curve_tolerance = val;
    }
}