#ifndef SW_CORE_H
#define SW_CORE_H

#include "SWtypes.h"

typedef enum SWenum {
    /* primitive types */
    SW_LINES,
    SW_LINE_STRIP,
    SW_CURVES,
    SW_CURVE_STRIP,
    SW_TRIANGLES,
    SW_TRIANGLE_STRIP,

    /* data types */
    SW_UNSIGNED_BYTE,
    SW_UNSIGNED_SHORT,
    SW_UNSIGNED_INT,
    /* unifom types */
    SW_INT,
    SW_FLOAT,
    SW_VEC2,
    SW_VEC3,
    SW_VEC4,
    SW_MAT3,
    SW_MAT4,

    /* framebuffer types */
    SW_BGRA8888,
    SW_FRGBA,

    /* texture types */
    SW_RGB,
    SW_RGBA,
    SW_BGRA,

    SW_COMPRESSED,

    /* args for swEnable */
    SW_DEPTH_TEST,
    SW_DEPTH_WRITE,
    SW_BLEND,
    SW_PERSPECTIVE_CORRECTION,
    SW_FAST_PERSPECTIVE_CORRECTION,

    /* buffer types */
    SW_ARRAY_BUFFER,
    SW_INDEX_BUFFER,

    SW_MAX_VERTEX_UNIFORM_VECTORS,

    SW_CURVE_TOLERANCE,

    SW_TEXTURE0 = 0,

    SW_PHYSICAL_MEMORY,
    SW_CPU_VENDOR,
    SW_CPU_MODEL,
    SW_NUM_CPUS,
} SWenum;

#define SW_TILE_SIZE 8
#define SW_INV_TILE_SIZE (((SWfloat)1) / SW_TILE_SIZE)
#define SW_INV_TILE_STEP (((SWfloat)1) / (SW_TILE_SIZE - 1));

typedef struct SWcontext SWcontext;

/* Context operations */
struct SWcontext *swCreateContext(SWint w, SWint h);
void swMakeCurrent(struct SWcontext *ctx);
void swDeleteContext(struct SWcontext *ctx);

/* Vertex buffer operations */
SWint swCreateBuffer();
void swDeleteBuffer(SWint buf);
void swBindBuffer(SWenum type, SWint buf);
void swBufferData(SWenum type, SWuint size, const void *data);
void swBufferSubData(SWenum type, SWuint offset, SWuint size, const void *data);
void swGetBufferSubData(SWenum type, SWuint offset, SWuint size, void *data);

/* Framebuffer operations */
SWint swCreateFramebuffer(SWenum type, SWint w, SWint h, SWint with_depth);
void swDeleteFramebuffer(SWint i);
void swBindFramebuffer(SWint i);
SWint swGetCurFramebuffer();
const void *swGetPixelDataRef(SWint i);
const void *swGetDepthDataRef(SWint i);

void swBlitPixels(SWint x, SWint y, SWint pitch, SWenum type, SWenum mode, SWint w,
                  SWint h, const void *pixels, SWfloat scale);
void swBlitTexture(SWint x, SWint y, SWfloat scale);

/* Texture operations */
SWint swCreateTexture();
void swDeleteTexture(SWint tex);
void swActiveTexture(SWint slot);
void swBindTexture(SWint tex);
void swTexImage2D(SWenum mode, SWenum type, SWint w, SWint h, const void *pixels);
void swTexImage2DMove_malloced(SWenum mode, SWenum type, SWint w, SWint h, void *pixels);
void swTexImage2DConst(SWenum mode, SWenum type, SWint w, SWint h, void *pixels);
void swTexture(SWint slot, const SWfloat *uv, SWfloat *col);

/* SWtexture.h should be included for these */
#define swTexture_RGB888(slot, uv, col)                                                  \
    {                                                                                    \
        extern SWcontext *sw_cur_context;                                                \
        swTex_RGB888_GetColorFloat_RGBA(&sw_cur_context->textures[slot], uv[0], uv[1],   \
                                        col);                                            \
    }

/* Program operations */
SWint swCreateProgram();
void swInitProgram(vtx_shader_proc v_proc, frag_shader_proc f_proc, SWint v_out_floats);
void swDeleteProgram(SWint program);
void swUseProgram(SWint program);

void swEnable(SWenum func);
void swDisable(SWenum func);

SWint swIsEnabled(SWenum func);

void swClearColor(SWfloat r, SWfloat g, SWfloat b, SWfloat a);
void swClearDepth(SWfloat val);

SWint swGetInteger(SWenum what);
SWfloat swGetFloat(SWenum what);
const char *swGetString(SWenum what);

void swSetFloat(SWenum what, SWfloat val);

#endif /* SW_CORE_H */
