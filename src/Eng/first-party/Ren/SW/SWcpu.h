#ifndef SWCPU_H
#define SWCPU_H

#include "SWtypes.h"

typedef struct SWcpu_info {
    char        *vendor;
    char        *model;
    SWint       num_cpus;
    SWfloat     physical_memory;
    // features
    unsigned    sse2_supported : 1;
    unsigned    sse3_supported : 1;
    unsigned    ssse3_supported : 1;
    unsigned    sse41_supported : 1;
    unsigned    avx_supported : 1;
    unsigned    avx2_supported : 1;
    unsigned    avx512_supported : 1;
} SWcpu_info;

void swCPUInfoInit(SWcpu_info *info);
void swCPUInfoDestroy(SWcpu_info *info);

#endif // SWCPU_H
