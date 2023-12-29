#ifndef SW_DRAW_H
#define SW_DRAW_H

#include "SWcore.h"

void swVertexAttribPointer(SWuint index, SWint size, SWuint stride, const void *pointer);

void swRegisterUniform(SWint index, SWenum type);
void swRegisterUniformv(SWint index, SWenum type, SWint num);
void swSetUniform(SWint index, SWenum type, const void *data);
void swSetUniformv(SWint index, SWenum type, SWint num, const void *data);

void swDrawArrays(SWenum prim_type, SWuint first, SWuint count);
void swDrawElements(SWenum prim_type, SWuint count, SWenum type, const void *indices);

#endif /* SW_DRAW_H */