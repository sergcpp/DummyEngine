#include "RpRTGICache.h"

#include <Ren/Context.h>

#include "../graph/GraphBuilder.h"

void Eng::RpRTGICache::Execute(RpBuilder &builder) {
    LazyInit(builder.ctx(), builder.sh());

    if (builder.ctx().capabilities.ray_query) {
        Execute_HWRT_Inline(builder);
    } else if (false && builder.ctx().capabilities.raytracing) {
        //Execute_HWRT_Pipeline(builder);
    } else {
        Execute_SWRT(builder);
    }
}
