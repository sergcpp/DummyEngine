#ifndef SWCPU_H
#define SWCPU_H

#include "SWtypes.h"

typedef struct SWcpu_info {
    char        *vendor;
    char        *model;
    SWint       num_cpus;
    SWfloat     physical_memory;
} SWcpu_info;

void swCInfoInit(SWcpu_info *info);
void swCInfoDestroy(SWcpu_info *info);

#endif // SWCPU_H
