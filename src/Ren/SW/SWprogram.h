#ifndef SW_PROGRAM_H
#define SW_PROGRAM_H

#include "SWcore.h"

#define SW_MAX_VTX_ATTRIBS      16
#define SW_MAX_UNIFORMS         32

typedef struct SWprogram {
    vtx_shader_proc v_proc;
    SWint v_out_size;
    frag_shader_proc f_proc;
    SWvtx_attribute vertex_attributes[SW_MAX_VTX_ATTRIBS];
    SWuint num_attributes;
    SWuniform uniforms[SW_MAX_UNIFORMS];
    SWubyte *uniform_buf;
    SWuint unifrom_buf_size;
} SWprogram;

void swProgInit(SWprogram *p, SWubyte *uniform_buf, vtx_shader_proc v_proc, frag_shader_proc f_proc, SWint v_out_floats);
void swProgDestroy(SWprogram *p);

void swProgSetVtxAttribPointer(SWprogram *p, SWcontext *ctx, SWuint index, SWint size, SWint stride, const void *pointer);
void swProgDisableVtxAttrib(SWprogram *p, SWuint index);

void swProgRegUniform(SWprogram *p, SWint index, SWenum type);
void swProgRegUniformv(SWprogram *p, SWint index, SWenum type, SWint num);
void swProgSetProgramUniform(SWprogram *p, SWint index, SWenum type, const void *data);
void swProgSetProgramUniformv(SWprogram *p, SWint index, SWenum type, SWint num, const void *data);

void swProgDrawLinesArray(SWprogram *p, SWcontext *ctx, SWuint first, SWuint count);
void swProgDrawLineStripArray(SWprogram *p, SWcontext *ctx, SWuint first, SWuint count);
void swProgDrawLinesIndexed(SWprogram *p, SWcontext *ctx, SWuint count, SWenum index_type, const void *indices);
void swProgDrawLineStripIndexed(SWprogram *p, SWcontext *ctx, SWuint count, SWenum index_type, const void *indices);

void swProgDrawCurvesArray(SWprogram *p, SWcontext *ctx, SWuint first, SWuint count);
void swProgDrawCurveStripArray(SWprogram *p, SWcontext *ctx, SWuint first, SWuint count);
void swProgDrawCurvesIndexed(SWprogram *p, SWcontext *ctx, SWuint count, SWenum index_type, const void *indices);
void swProgDrawCurveStripIndexed(SWprogram *p, SWcontext *ctx, SWuint count, SWenum index_type, const void *indices);

void swProgDrawTrianglesArray(SWprogram *p, SWcontext *ctx, SWuint first, SWuint count);
void swProgDrawTriangleStripArray(SWprogram *p, SWcontext *ctx, SWuint first, SWuint count);
void swProgDrawTrianglesIndexed(SWprogram *p, SWcontext *ctx, SWuint count, SWenum index_type, const void *indices);
void swProgDrawTriangleStripIndexed(SWprogram *p, SWcontext *ctx, SWuint count, SWenum index_type, const void *indices);

#endif /* SW_PROGRAM_H */
