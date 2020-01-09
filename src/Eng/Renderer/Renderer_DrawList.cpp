#include "Renderer_DrawList.h"

DrawList::DrawList() {
    skin_transforms.realloc(REN_MAX_SKIN_XFORMS_TOTAL);
    skin_regions.realloc(REN_MAX_SKIN_REGIONS_TOTAL);
    instances.realloc(REN_MAX_INSTANCES_TOTAL);
    shadow_batches.realloc(REN_MAX_SHADOW_BATCHES);
    shadow_batch_indices.realloc(REN_MAX_SHADOW_BATCHES);
    shadow_lists.realloc(REN_MAX_SHADOWMAPS_TOTAL);
    shadow_regions.realloc(REN_MAX_SHADOWMAPS_TOTAL);
    zfill_batches.realloc(REN_MAX_MAIN_BATCHES);
    zfill_batch_indices.realloc(REN_MAX_MAIN_BATCHES);
    main_batches.realloc(REN_MAX_MAIN_BATCHES);
    main_batch_indices.realloc(REN_MAX_MAIN_BATCHES);
    light_sources.realloc(REN_MAX_LIGHTS_TOTAL);
    decals.realloc(REN_MAX_DECALS_TOTAL);
    probes.realloc(REN_MAX_PROBES_TOTAL);

    cells.realloc(REN_CELLS_COUNT);
    cells.count = REN_CELLS_COUNT;

    items.realloc(REN_MAX_ITEMS_TOTAL);

    cached_shadow_regions.realloc(REN_MAX_SHADOWMAPS_TOTAL);
}