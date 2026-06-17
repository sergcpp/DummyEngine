#ifndef CLEAR_BUFFER_INTERFACE_H
#define CLEAR_BUFFER_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(ClearBuffer)

struct Params {
    uint data_len;
};

const uint GRP_SIZE_X = 256;

const uint OUT_BUF_SLOT = 0;

INTERFACE_END

#endif // CLEAR_BUFFER_INTERFACE_H