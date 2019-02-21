#ifndef SW_BUFFER_H
#define SW_BUFFER_H

#include "SWcore.h"

typedef struct SWbuffer {
    SWuint size;
    void *data;
} SWbuffer;

void swBufInit(SWbuffer *b, SWuint size, const void *data);
void swBufDestroy(SWbuffer *b);

void swBufSetData(SWbuffer *b, SWuint offset, SWuint size, const void *data);
void swBufGetData(SWbuffer *b, SWuint offset, SWuint size, void *data);

#endif /* SW_BUFFER_H */