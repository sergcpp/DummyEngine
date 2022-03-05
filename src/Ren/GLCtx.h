#pragma once

#include "Common.h"
#include "SmallVector.h"
#include "Fence.h"

namespace Ren {
struct ApiContext {
    SmallVector<SyncFence, MaxFramesInFlight> in_flight_fences;

    int active_present_image = 0;

    int backend_frame = 0;
    SmallVector<Tex2DRef, MaxFramesInFlight> present_image_refs;

    //VkQueryPool query_pools[MaxFramesInFlight] = {};
    uint32_t query_counts[MaxFramesInFlight] = {};
    uint64_t query_results[MaxFramesInFlight][MaxTimestampQueries] = {};
};

} // namespace Ren