#include "RpUpdateAccBuffers.h"

#include <Ren/Context.h>

void Eng::RpUpdateAccBuffersExecutor::Execute(RpBuilder &builder) {
    if (builder.ctx().capabilities.raytracing || builder.ctx().capabilities.ray_query) {
        Execute_HWRT(builder);
    } else {
        Execute_SWRT(builder);
    }
}

void Eng::RpUpdateAccBuffersExecutor::Execute_SWRT(RpBuilder &builder) {}