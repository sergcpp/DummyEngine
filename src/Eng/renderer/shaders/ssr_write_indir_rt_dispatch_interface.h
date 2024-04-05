#ifndef SSR_WRITE_INDIR_RT_DISPATCH_INTERFACE_H
#define SSR_WRITE_INDIR_RT_DISPATCH_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(SSRWriteIndirRTDispatch)

struct Params {
    int counter_index;
    float _pad[3];
};

const int RAY_COUNTER_SLOT = 0;
const int INDIR_ARGS_SLOT = 1;

INTERFACE_END

#endif // SSR_WRITE_INDIR_RT_DISPATCH_INTERFACE_H