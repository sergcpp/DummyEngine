#include "SWcontext.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static SWubyte _1byte_tmp_buf[] = {0};
static SWubyte _1px_tmp_tex[] = {0, 200, 200};

sw_inline SWint _swBufferIndex(const SWcontext *ctx, const SWenum type) {
    ((void)ctx);

    const SWint i = type - SW_ARRAY_BUFFER;
    assert(i < sizeof(ctx->binded_buffers) / sizeof(SWint));
    return i;
}

sw_inline SWbuffer *_swBindedBuffer(SWcontext *ctx, const SWenum type) {
    const SWint i = _swBufferIndex(ctx, type);
    const SWint buf = ctx->binded_buffers[i];
    assert(buf >= 0 && buf < sizeof(ctx->buffers));
    return &ctx->buffers[buf];
}

void swCtxInit(SWcontext *ctx, const SWint w, const SWint h) {
    memset(ctx, 0, sizeof(SWcontext));
    ctx->cur_framebuffer = swCtxCreateFramebuffer(ctx, SW_BGRA8888, w, h, 1);
    ctx->render_flags = DEFAULT_RENDER_FLAGS;
    ctx->curve_tolerance = 8.0f;

    ctx->binded_buffers[0] = -1;
    ctx->binded_buffers[1] = -1;

    swCPUInfoInit(&ctx->cpu_info);

    extern SWfloat _sw_ubyte_to_float_table[256];
    if (_sw_ubyte_to_float_table[1] == 0) {
        SWint i;
        for (i = 1; i < 256; i++) {
            _sw_ubyte_to_float_table[i] = (SWfloat)i / 255;
        }
    }
}

void swCtxDestroy(SWcontext *ctx) {
    for (SWint i = 0; i < ctx->num_programs; i++) {
        swCtxDeleteProgram(ctx, i);
    }
    for (SWint i = 0; i < ctx->num_buffers; i++) {
        swCtxDeleteBuffer(ctx, i);
    }
    for (SWint i = 0; i < ctx->num_framebuffers; i++) {
        swFbufDestroy(&ctx->framebuffers[i]);
    }
    for (SWint i = 0; i < ctx->num_textures; i++) {
        swCtxDeleteTexture(ctx, i);
    }
    swCPUInfoDestroy(&ctx->cpu_info);
    memset(ctx, 0, sizeof(SWcontext));
}

/*************************************************************************************************/

SWint swCtxCreateBuffer(SWcontext *ctx) {
    assert(ctx->num_buffers < sizeof(ctx->buffers) / sizeof(SWbuffer) - 1);
    SWint i, index = ctx->num_buffers;
    for (i = 0; i < ctx->num_buffers; i++) {
        if (ctx->buffers[i].data == NULL) {
            index = i;
            break;
        }
    }

    SWbuffer *b = &ctx->buffers[index];
    swBufInit(b, sizeof(SWubyte), &_1byte_tmp_buf);

    if (index == ctx->num_buffers) {
        ctx->num_buffers++;
        return ctx->num_buffers;
    } else {
        return index + 1;
    }
}

void swCtxDeleteBuffer(SWcontext *ctx, SWint buf) {
    buf -= 1;
    SWbuffer *b = &ctx->buffers[buf];
    if (b->data != &_1byte_tmp_buf) {
        swBufDestroy(b);
    }
    if (buf == ctx->num_buffers - 1) {
        ctx->num_buffers--;
    }
}

void swCtxBindBuffer(SWcontext *ctx, const SWenum type, const SWint buf) {
    const SWint i = _swBufferIndex(ctx, type);
    ctx->binded_buffers[i] = buf - 1;
}

void swCtxBufferData(SWcontext *ctx, const SWenum type, const SWuint size,
                     const void *data) {
    SWbuffer *b = _swBindedBuffer(ctx, type);
    swBufInit(b, size, data);
}

void swCtxBufferSubData(SWcontext *ctx, const SWenum type, const SWuint offset,
                        const SWuint size, const void *data) {
    assert(data);
    SWbuffer *b = _swBindedBuffer(ctx, type);
    swBufSetData(b, offset, size, data);
}

void swCtxGetBufferSubData(SWcontext *ctx, SWenum type, SWuint offset, SWuint size,
                           void *data) {
    SWbuffer *b = _swBindedBuffer(ctx, type);
    swBufGetData(b, offset, size, data);
}

/*************************************************************************************************/

SWint swCtxCreateFramebuffer(SWcontext *ctx, SWenum type, const SWint w, const SWint h,
                             const SWint with_depth) {
    assert(ctx->num_framebuffers < sizeof(ctx->framebuffers) / sizeof(SWframebuffer) - 1);
    SWint i, index = ctx->num_framebuffers;
    for (i = 0; i < ctx->num_framebuffers; i++) {
        if (ctx->framebuffers[i].pixels == NULL) {
            index = i;
            break;
        }
    }

    SWframebuffer *f = &ctx->framebuffers[index];
    swFbufInit(f, type, w, h, with_depth);

    if (index == ctx->num_framebuffers) {
        return ctx->num_framebuffers++;
    } else {
        return index;
    }
}

void swCtxDeleteFramebuffer(SWcontext *ctx, const SWint i) {
    SWframebuffer *f = &ctx->framebuffers[i];
    swFbufDestroy(f);
}

void swCtxBindFramebuffer(SWcontext *ctx, const SWint i) { ctx->cur_framebuffer = i; }

SWint swCtxGetCurFramebuffer(SWcontext *ctx) { return ctx->cur_framebuffer; }

const void *swCtxGetPixelDataRef(SWcontext *ctx, const SWint i) {
    SWframebuffer *f = &ctx->framebuffers[i];
    return f->pixels;
}

const void *swCtxGetDepthDataRef(SWcontext *ctx, const SWint i) {
    SWframebuffer *f = &ctx->framebuffers[i];
    return f->zbuf->depth;
}

void swCtxBlitPixels(SWcontext *ctx, const SWint x, const SWint y, const SWint pitch, const SWenum type,
                     const SWenum mode, const SWint w, const SWint h, const void *pixels, const SWfloat scale) {
    SWframebuffer *f = &ctx->framebuffers[ctx->cur_framebuffer];
    swFbufBlitPixels(f, x, y, pitch, type, mode, w, h, pixels, scale);
}

void swCtxBlitTexture(SWcontext *ctx, const SWint x, const SWint y, const SWfloat scale) {
    SWframebuffer *f = &ctx->framebuffers[ctx->cur_framebuffer];
    SWtexture *t = &ctx->textures[ctx->binded_textures[ctx->active_tex_slot]];
    swFbufBlitTexture(f, x, y, t, scale);
}

/*************************************************************************************************/

SWint swCtxCreateTexture(SWcontext *ctx) {
    assert(ctx->num_textures < sizeof(ctx->textures) / sizeof(SWtexture) - 1);
    SWint i, index = ctx->num_textures;
    for (i = 0; i < ctx->num_textures; i++) {
        if (ctx->textures[i].pixels == NULL) {
            index = i;
            break;
        }
    }

    SWtexture *t = &ctx->textures[index];
    swTexInit(t, SW_RGB, SW_UNSIGNED_BYTE, 1, 1, _1px_tmp_tex);

    if (index == ctx->num_textures) {
        return ctx->num_textures++;
    } else {
        return index;
    }
}

void swCtxDeleteTexture(SWcontext *ctx, const SWint tex) {
    SWtexture *t = &ctx->textures[tex];
    if (t->pixels != _1px_tmp_tex) {
        swTexDestroy(t);
    }
    if (tex == ctx->num_textures - 1) {
        ctx->num_textures--;
    }
}

void swCtxActiveTexture(SWcontext *ctx, const SWint slot) { ctx->active_tex_slot = slot; }

void swCtxBindTexture(SWcontext *ctx, const SWint tex) {
    ctx->binded_textures[ctx->active_tex_slot] = tex;
}

void swCtxTexImage2D(SWcontext *ctx, const SWenum mode, const SWenum type, const SWint w, const SWint h,
                     const void *pixels) {
    SWtexture *t = &ctx->textures[ctx->binded_textures[ctx->active_tex_slot]];
    swTexDestroy(t);
    swTexInit(t, mode, type, w, h, pixels);
}

void swCtxTexImage2DMove_malloced(SWcontext *ctx, const SWenum mode, const SWenum type, const SWint w,
                                  const SWint h, void *pixels) {
    SWtexture *t = &ctx->textures[ctx->binded_textures[ctx->active_tex_slot]];
    swTexInitMove_malloced(t, mode, type, w, h, pixels);
}

static void _sw_null_free(void *p) { ((void)p); }
void swCtxTexImage2DConst(SWcontext *ctx, const SWenum mode, const SWenum type, const SWint w, const SWint h,
                          void *pixels) {
    SWtexture *t = &ctx->textures[ctx->binded_textures[ctx->active_tex_slot]];
    swTexInitMove(t, mode, type, w, h, pixels, _sw_null_free);
}

/*************************************************************************************************/

static VSHADER empty_vshader(VS_IN, VS_OUT) {
    ((void)attribs);
    ((void)index);
    ((void)uniforms);
    ((void)out_data);
}
static FSHADER empty_fshader(FS_IN, FS_OUT) {
    ((void)f_in_data);
    ((void)uniforms);
    ((void)f_out_data);
    ((void)b_discard);
}

SWint swCtxCreateProgram(SWcontext *ctx) {
    assert(ctx->num_programs < sizeof(ctx->programs) / sizeof(SWprogram) - 1);
    SWint i, index = ctx->num_programs;
    for (i = 0; i < ctx->num_programs; i++) {
        if (ctx->programs[i].v_proc == NULL) {
            index = i;
            break;
        }
    }
    SWprogram *p = &ctx->programs[index];
    swProgInit(p, ctx->uniform_buf, empty_vshader, empty_fshader, 0);

    if (index == ctx->num_programs) {
        return ctx->num_programs++;
    } else {
        return index;
    }
}

void swCtxInitProgram(SWcontext *ctx, vtx_shader_proc v_proc, frag_shader_proc f_proc,
                      SWint v_out_floats) {
    SWprogram *p = &ctx->programs[ctx->cur_program];
    swProgInit(p, ctx->uniform_buf, v_proc, f_proc, v_out_floats);
}

void swCtxDeleteProgram(SWcontext *ctx, const SWint program) {
    SWprogram *p = &ctx->programs[program];
    swProgDestroy(p);
    if (program == ctx->num_programs - 1) {
        ctx->num_programs--;
    }
}

void swCtxUseProgram(SWcontext *ctx, const SWint program) {
    assert(program < ctx->num_programs);
    ctx->cur_program = program;
}

void swCtxRegisterUniform(SWcontext *ctx, const SWint index, const SWenum type) {
    SWprogram *p = &ctx->programs[ctx->cur_program];
    assert(p->v_proc != empty_vshader && p->f_proc != empty_fshader);
    swProgRegUniform(p, index, type);
}

void swCtxRegisterUniformv(SWcontext *ctx, const SWint index, const SWenum type, const SWint num) {
    SWprogram *p = &ctx->programs[ctx->cur_program];
    assert(p->v_proc != empty_vshader && p->f_proc != empty_fshader);
    swProgRegUniformv(p, index, type, num);
}

void swCtxSetUniform(SWcontext *ctx, const SWint index, const SWenum type, const void *data) {
    SWprogram *p = &ctx->programs[ctx->cur_program];
    assert(p->v_proc != empty_vshader && p->f_proc != empty_fshader);
    swProgSetProgramUniform(p, index, type, data);
}

void swCtxSetUniformv(SWcontext *ctx, const SWint index, const SWenum type, const SWint num,
                      const void *data) {
    SWprogram *p = &ctx->programs[ctx->cur_program];
    assert(p->v_proc != empty_vshader && p->f_proc != empty_fshader);
    swProgSetProgramUniformv(p, index, type, num, data);
}
