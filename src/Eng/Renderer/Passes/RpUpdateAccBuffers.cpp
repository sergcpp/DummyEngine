#include "RpUpdateAccBuffers.h"

void RpUpdateAccBuffersExecutor::Execute(RpBuilder &builder) {
#if !defined(USE_GL_RENDER)
    if (builder.ctx().capabilities.raytracing) {
        Execute_HWRT(builder);
    } else
#endif
    {
        Execute_SWRT(builder);
    }
}

void RpUpdateAccBuffersExecutor::Execute_SWRT(RpBuilder &builder) {}