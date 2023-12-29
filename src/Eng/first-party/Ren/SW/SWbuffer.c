#include "SWbuffer.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

void swBufInit(SWbuffer *b, const SWuint size, const void *data) {
    void *_data = malloc(size);
    assert(_data);
    if (data) {
        memcpy(_data, data, size);
    }
    b->size = size;
    b->data = _data;
}

void swBufDestroy(SWbuffer *b) {
    free(b->data);
    memset(b, 0, sizeof(SWbuffer));
}

void swBufSetData(SWbuffer *b, const SWuint offset, const SWuint size, const void *data) {
    assert(b->data);
    memcpy((char *)b->data + offset, data, size);
}

void swBufGetData(const SWbuffer *b, const SWuint offset, const SWuint size, void *data) {
    assert(b->data);
    memcpy(data, (char *)b->data + offset, size);
}
