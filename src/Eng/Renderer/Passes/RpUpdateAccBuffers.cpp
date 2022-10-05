#include "RpUpdateAccBuffers.h"

#include <Ren/Context.h>

void RpUpdateAccBuffersExecutor::Execute(RpBuilder &builder) {
    if (builder.ctx().capabilities.raytracing || builder.ctx().capabilities.ray_query) {
        Execute_HWRT(builder);
    } else {
        Execute_SWRT(builder);
    }
}

void RpUpdateAccBuffersExecutor::Execute_SWRT(RpBuilder &builder) {}