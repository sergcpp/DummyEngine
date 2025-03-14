#pragma once

#include "Common.h"
#include "SmallVector.h"
#include "Fence.h"
#include "Storage.h"

namespace Ren {
class Texture;
using TexRef = StrongRef<Texture, NamedStorage<Texture>>;

struct ApiContext {
    SmallVector<SyncFence, MaxFramesInFlight> in_flight_fences;

    int active_present_image = 0;

    int backend_frame = 0;
    SmallVector<TexRef, MaxFramesInFlight> present_image_refs;

    uint32_t queries[MaxFramesInFlight][MaxTimestampQueries] = {};
    uint32_t query_counts[MaxFramesInFlight] = {};
    uint64_t query_results[MaxFramesInFlight][MaxTimestampQueries] = {};

    CommandBuffer BegSingleTimeCommands() { return nullptr; }
    void EndSingleTimeCommands(CommandBuffer command_buf) {}
};

bool ReadbackTimestampQueries(ApiContext *api_ctx, int i);

} // namespace Ren