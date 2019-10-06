#include "Renderer_DrawList.h"

DrawList::DrawList() {
    skin_transforms.data.reset(new SkinTransform[REN_MAX_SKIN_XFORMS_TOTAL]);
    skin_transforms.capacity = REN_MAX_SKIN_XFORMS_TOTAL;
    skin_transforms.count = 0;

    skin_regions.data.reset(new SkinRegion[REN_MAX_SKIN_REGIONS_TOTAL]);
    skin_regions.capacity = REN_MAX_SKIN_REGIONS_TOTAL;
    skin_regions.count = 0;

    instances.data.reset(new InstanceData[REN_MAX_INSTANCES_TOTAL]);
    instances.capacity = REN_MAX_INSTANCES_TOTAL;
    instances.count = 0;

    shadow_batches.data.reset(new DepthDrawBatch[REN_MAX_SHADOW_BATCHES]);
    shadow_batches.capacity = REN_MAX_SHADOW_BATCHES;
    shadow_batches.count = 0;

    shadow_batch_indices.data.reset(new uint32_t[REN_MAX_SHADOW_BATCHES]);
    shadow_batch_indices.capacity = REN_MAX_SHADOW_BATCHES;
    shadow_batch_indices.count = 0;

    shadow_lists.data.reset(new ShadowList[REN_MAX_SHADOWMAPS_TOTAL]);
    shadow_lists.capacity = REN_MAX_SHADOWMAPS_TOTAL;
    shadow_lists.count = 0;

    shadow_regions.data.reset(new ShadowMapRegion[REN_MAX_SHADOWMAPS_TOTAL]);
    shadow_regions.capacity = REN_MAX_SHADOWMAPS_TOTAL;
    shadow_regions.count = 0;

    zfill_batches.data.reset(new DepthDrawBatch[REN_MAX_MAIN_BATCHES]);
    zfill_batches.capacity = REN_MAX_MAIN_BATCHES;
    zfill_batches.count = 0;

    zfill_batch_indices.data.reset(new uint32_t[REN_MAX_MAIN_BATCHES]);
    zfill_batch_indices.capacity = REN_MAX_MAIN_BATCHES;
    zfill_batch_indices.count = 0;

    main_batches.data.reset(new MainDrawBatch[REN_MAX_MAIN_BATCHES]);
    main_batches.capacity = REN_MAX_MAIN_BATCHES;
    main_batches.count = 0;

    main_batch_indices.data.reset(new uint32_t[REN_MAX_MAIN_BATCHES]);
    main_batch_indices.capacity = REN_MAX_MAIN_BATCHES;
    main_batch_indices.count = 0;

    light_sources.data.reset(new LightSourceItem[REN_MAX_LIGHTS_TOTAL]);
    light_sources.capacity = REN_MAX_LIGHTS_TOTAL;
    light_sources.count = 0;

    decals.data.reset(new DecalItem[REN_MAX_DECALS_TOTAL]);
    decals.capacity = REN_MAX_DECALS_TOTAL;
    decals.count = 0;

    probes.data.reset(new ProbeItem[REN_MAX_PROBES_TOTAL]);
    probes.capacity = REN_MAX_PROBES_TOTAL;
    probes.count = 0;

    cells.data.reset(new CellData[REN_CELLS_COUNT]);
    cells.capacity = REN_CELLS_COUNT;
    cells.count = REN_CELLS_COUNT;

    items.data.reset(new ItemData[REN_MAX_ITEMS_TOTAL]);
    items.capacity = REN_MAX_ITEMS_TOTAL;
    items.count = 0;

    cached_shadow_regions.data.reset(new ShadReg[REN_MAX_SHADOWMAPS_TOTAL]);
    cached_shadow_regions.capacity = REN_MAX_SHADOWMAPS_TOTAL;
    cached_shadow_regions.count = 0;
}