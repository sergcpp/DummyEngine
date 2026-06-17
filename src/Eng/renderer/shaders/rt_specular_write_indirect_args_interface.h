#ifndef RT_SPECULAR_WRITE_INDIRECT_ARGS_INTERFACE_H
#define RT_SPECULAR_WRITE_INDIRECT_ARGS_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(RTSpecularWriteIndirectArgs)

struct Params {
    int counter_index;
    float _pad[3];
};

const uint RAY_COUNTER_SLOT = 0;
const uint INDIR_ARGS_SLOT = 1;

INTERFACE_END

#endif // RT_SPECULAR_WRITE_INDIRECT_ARGS_INTERFACE_H