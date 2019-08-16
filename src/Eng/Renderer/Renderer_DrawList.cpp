#include "Renderer_DrawList.h"

void DrawList::Init(Ren::BufferRef _shared_data_stage_buf, Ren::BufferRef _instatnces_stage_buf,
                    Ren::BufferRef _skin_transforms_stage_buf, Ren::BufferRef _shape_keys_stage_buf,
                    Ren::BufferRef _cells_stage_buf, Ren::BufferRef _items_stage_buf, Ren::BufferRef _lights_stage_buf,
                    Ren::BufferRef _decals_stage_buf) {
    instances.realloc(REN_MAX_INSTANCES_TOTAL);
    instatnces_stage_buf = std::move(_instatnces_stage_buf);
    shadow_batches.realloc(REN_MAX_SHADOW_BATCHES);
    shadow_batch_indices.realloc(REN_MAX_SHADOW_BATCHES);
    shadow_lists.realloc(REN_MAX_SHADOWMAPS_TOTAL);
    shadow_regions.realloc(REN_MAX_SHADOWMAPS_TOTAL);
    zfill_batches.realloc(REN_MAX_MAIN_BATCHES);
    zfill_batch_indices.realloc(REN_MAX_MAIN_BATCHES);
    main_batches.realloc(REN_MAX_MAIN_BATCHES);
    main_batch_indices.realloc(REN_MAX_MAIN_BATCHES);
    skin_transforms.realloc(REN_MAX_SKIN_XFORMS_TOTAL);
    skin_transforms_stage_buf = std::move(_skin_transforms_stage_buf);
    skin_regions.realloc(REN_MAX_SKIN_REGIONS_TOTAL);
    shape_keys_data.realloc(REN_MAX_SHAPE_KEYS_TOTAL);
    shape_keys_stage_buf = std::move(_shape_keys_stage_buf);
    light_sources.realloc(REN_MAX_LIGHTS_TOTAL);
    lights_stage_buf = std::move(_lights_stage_buf);
    decals.realloc(REN_MAX_DECALS_TOTAL);
    decals_stage_buf = std::move(_decals_stage_buf);
    probes.realloc(REN_MAX_PROBES_TOTAL);
    ellipsoids.realloc(REN_MAX_ELLIPSES_TOTAL);

    cells.realloc(REN_CELLS_COUNT);
    cells.count = REN_CELLS_COUNT;
    cells_stage_buf = std::move(_cells_stage_buf);

    items.realloc(REN_MAX_ITEMS_TOTAL);
    items_stage_buf = std::move(_items_stage_buf);

    shared_data_stage_buf = std::move(_shared_data_stage_buf);

    visible_textures.realloc(REN_MAX_TEX_COUNT);
    desired_textures.realloc(REN_MAX_TEX_COUNT);

    cached_shadow_regions.realloc(REN_MAX_SHADOWMAPS_TOTAL);
}

void DrawList::Clear() {
    instances.count = 0;
    shadow_batches.count = 0;
    shadow_batch_indices.count = 0;
    shadow_lists.count = 0;
    shadow_regions.count = 0;
    zfill_batches.count = 0;
    zfill_batch_indices.count = 0;
    main_batches.count = 0;
    main_batch_indices.count = 0;
    skin_transforms.count = 0;
    skin_regions.count = 0;
    shape_keys_data.count = 0;
    light_sources.count = 0;
    decals.count = 0;
    probes.count = 0;
    ellipsoids.count = 0;

    items.count = 0;

    visible_textures.count = 0;
    desired_textures.count = 0;

    materials = nullptr;
    decals_atlas = nullptr;
    probe_storage = nullptr;

    cached_shadow_regions.count = 0;
}