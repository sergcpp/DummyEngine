#include "RpRTGICache.h"

#include <Ren/Context.h>

#include "../graph/GraphBuilder.h"

void Eng::RpRTGICache::Execute(RpBuilder &builder) {
    LazyInit(builder.ctx(), builder.sh());

    if (builder.ctx().capabilities.hwrt) {
        Execute_HWRT_Inline(builder);
    } else {
        Execute_SWRT(builder);
    }
}
