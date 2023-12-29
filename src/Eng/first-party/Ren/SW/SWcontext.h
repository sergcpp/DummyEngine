#ifndef SW_CONTEXT_H
#define SW_CONTEXT_H

#include "SWbuffer.h"
#include "SWcore.h"
#include "SWcpu.h"
#include "SWframebuffer.h"
#include "SWprogram.h"
#include "SWtexture.h"

/* render flags */
#define DEPTH_TEST_ENABLED (1 << 0)
#define DEPTH_WRITE_ENABLED (1 << 1)
#define BLEND_ENABLED (1 << 2)
#define PERSPECTIVE_CORRECTION_ENABLED (1 << 3)
#define FAST_PERSPECTIVE_CORRECTION (1 << 4)

#define DEFAULT_RENDER_FLAGS                                                             \
    (DEPTH_TEST_ENABLED | DEPTH_WRITE_ENABLED | PERSPECTIVE_CORRECTION_ENABLED)

#define SW_UNIFORM_BUF_SIZE 2048

struct SWcontext {
    SWubyte uniform_buf[SW_UNIFORM_BUF_SIZE];

    SWprogram programs[32];
    SWint num_programs, cur_program;

    SWtexture textures[256];
    SWint num_textures, binded_textures[8], active_tex_slot;

    SWbuffer buffers[256];
    SWint num_buffers, binded_buffers[2];

    SWframebuffer framebuffers[8];
    SWint num_framebuffers, cur_framebuffer;

    SWuint render_flags;

    SWfloat curve_tolerance;

    SWcpu_info cpu_info;
};

void swCtxInit(SWcontext *ctx, SWint w, SWint h);
void swCtxDestroy(SWcontext *ctx);

/* Vertex buffer operations */
SWint swCtxCreateBuffer(SWcontext *ctx);
void swCtxDeleteBuffer(SWcontext *ctx, SWint buf);
void swCtxBindBuffer(SWcontext *ctx, SWenum type, SWint buf);
void swCtxBufferData(SWcontext *ctx, SWenum type, SWuint size, const void *data);
void swCtxBufferSubData(SWcontext *ctx, SWenum type, SWuint offset, SWuint size,
                        const void *data);
void swCtxGetBufferSubData(SWcontext *ctx, SWenum type, SWuint offset, SWuint size,
                           void *data);

/* Framebuffer operations */
SWint swCtxCreateFramebuffer(SWcontext *ctx, SWenum type, SWint w, SWint h,
                             SWint with_depth);
void swCtxDeleteFramebuffer(SWcontext *ctx, SWint i);
void swCtxBindFramebuffer(SWcontext *ctx, SWint i);
SWint swCtxGetCurFramebuffer(SWcontext *ctx);
const void *swCtxGetPixelDataRef(SWcontext *ctx, SWint i);
const void *swCtxGetDepthDataRef(SWcontext *ctx, SWint i);

void swCtxBlitPixels(SWcontext *ctx, SWint x, SWint y, SWint pitch, SWenum type,
                     SWenum mode, SWint w, SWint h, const void *pixels, SWfloat scale);
void swCtxBlitTexture(SWcontext *ctx, SWint x, SWint y, SWfloat scale);

/* Texture operations */
SWint swCtxCreateTexture(SWcontext *ctx);
void swCtxDeleteTexture(SWcontext *ctx, SWint tex);
void swCtxActiveTexture(SWcontext *ctx, SWint slot);
void swCtxBindTexture(SWcontext *ctx, SWint tex);
void swCtxTexImage2D(SWcontext *ctx, SWenum mode, SWenum type, SWint w, SWint h,
                     const void *pixels);
void swCtxTexImage2DMove_malloced(SWcontext *ctx, SWenum mode, SWenum type, SWint w,
                                  SWint h, void *pixels);
void swCtxTexImage2DConst(SWcontext *ctx, SWenum mode, SWenum type, SWint w, SWint h,
                          void *pixels);

/* Program operations */
SWint swCtxCreateProgram(SWcontext *ctx);
void swCtxInitProgram(SWcontext *ctx, vtx_shader_proc v_proc, frag_shader_proc f_proc,
                      SWint v_out_floats);
void swCtxDeleteProgram(SWcontext *ctx, SWint program);
void swCtxUseProgram(SWcontext *ctx, SWint program);
void swCtxRegisterUniform(SWcontext *ctx, SWint index, SWenum type);
void swCtxRegisterUniformv(SWcontext *ctx, SWint index, SWenum type, SWint num);
void swCtxSetUniform(SWcontext *ctx, SWint index, SWenum type, const void *data);
void swCtxSetUniformv(SWcontext *ctx, SWint index, SWenum type, SWint num,
                      const void *data);

#endif /* SW_CONTEXT_H */
