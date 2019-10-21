#include "Renderer_DrawList.h"

DrawList::DrawList() {
    skin_transforms.alloc(REN_MAX_SKIN_XFORMS_TOTAL);
    skin_regions.alloc(REN_MAX_SKIN_REGIONS_TOTAL);
    instances.alloc(REN_MAX_INSTANCES_TOTAL);
    shadow_batches.alloc(REN_MAX_SHADOW_BATCHES);
    shadow_batch_indices.alloc(REN_MAX_SHADOW_BATCHES);
    shadow_lists.alloc(REN_MAX_SHADOWMAPS_TOTAL);
    shadow_regions.alloc(REN_MAX_SHADOWMAPS_TOTAL);
    zfill_batches.alloc(REN_MAX_MAIN_BATCHES);
    zfill_batch_indices.alloc(REN_MAX_MAIN_BATCHES);
    main_batches.alloc(REN_MAX_MAIN_BATCHES);
    main_batch_indices.alloc(REN_MAX_MAIN_BATCHES);
    light_sources.alloc(REN_MAX_LIGHTS_TOTAL);
    decals.alloc(REN_MAX_DECALS_TOTAL);
    probes.alloc(REN_MAX_PROBES_TOTAL);

    cells.alloc(REN_CELLS_COUNT);
    cells.count = REN_CELLS_COUNT;

    items.alloc(REN_MAX_ITEMS_TOTAL);

    cached_shadow_regions.alloc(REN_MAX_SHADOWMAPS_TOTAL);
}