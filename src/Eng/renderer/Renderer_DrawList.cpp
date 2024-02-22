#include "Renderer_DrawList.h"

void Eng::DrawList::Init(Ren::BufferRef _shared_data_stage_buf, Ren::BufferRef _instances_stage_buf,
                         Ren::BufferRef _instance_indices_stage_buf, Ren::BufferRef _skin_transforms_stage_buf,
                         Ren::BufferRef _shape_keys_stage_buf, Ren::BufferRef _cells_stage_buf,
                         Ren::BufferRef _rt_cells_stage_buf, Ren::BufferRef _items_stage_buf,
                         Ren::BufferRef _rt_items_stage_buf, Ren::BufferRef _lights_stage_buf,
                         Ren::BufferRef _decals_stage_buf, Ren::BufferRef _rt_obj_instances_stage_buf,
                         Ren::BufferRef _rt_sh_obj_instances_stage_buf, Ren::BufferRef _rt_tlas_nodes_stage_buf,
                         Ren::BufferRef _rt_sh_tlas_nodes_stage_buf) {
    instance_indices.realloc(MAX_INSTANCES_TOTAL);
    instance_indices_stage_buf = std::move(_instance_indices_stage_buf);
    shadow_batches.realloc(MAX_SHADOW_BATCHES);
    shadow_batch_indices.realloc(MAX_SHADOW_BATCHES);
    shadow_lists.realloc(MAX_SHADOWMAPS_TOTAL);
    shadow_regions.realloc(MAX_SHADOWMAPS_TOTAL);
    basic_batches.realloc(MAX_MAIN_BATCHES);
    basic_batch_indices.realloc(MAX_MAIN_BATCHES);
    custom_batches.realloc(MAX_MAIN_BATCHES);
    custom_batch_indices.realloc(MAX_MAIN_BATCHES);
    skin_transforms.realloc(MAX_SKIN_XFORMS_TOTAL);
    skin_transforms_stage_buf = std::move(_skin_transforms_stage_buf);
    skin_regions.realloc(MAX_SKIN_REGIONS_TOTAL);
    shape_keys_data.realloc(MAX_SHAPE_KEYS_TOTAL);
    shape_keys_stage_buf = std::move(_shape_keys_stage_buf);
    lights_stage_buf = std::move(_lights_stage_buf);
    decals_stage_buf = std::move(_decals_stage_buf);

    cells.realloc(ITEM_CELLS_COUNT);
    cells.count = ITEM_CELLS_COUNT;
    cells_stage_buf = std::move(_cells_stage_buf);
    rt_cells.realloc(ITEM_CELLS_COUNT);
    rt_cells.count = ITEM_CELLS_COUNT;
    rt_cells_stage_buf = std::move(_rt_cells_stage_buf);

    items.realloc(MAX_ITEMS_TOTAL);
    items_stage_buf = std::move(_items_stage_buf);
    rt_items.realloc(MAX_ITEMS_TOTAL);
    rt_items_stage_buf = std::move(_rt_items_stage_buf);

    rt_geo_instances.realloc(MAX_RT_GEO_INSTANCES);
    for (int i = 0; i < 2; ++i) {
        rt_obj_instances[i].realloc(MAX_RT_OBJ_INSTANCES);
    }
    rt_obj_instances_stage_buf[0] = std::move(_rt_obj_instances_stage_buf);
    rt_obj_instances_stage_buf[1] = std::move(_rt_sh_obj_instances_stage_buf);
    swrt.rt_tlas_nodes_stage_buf[0] = std::move(_rt_tlas_nodes_stage_buf);
    swrt.rt_tlas_nodes_stage_buf[1] = std::move(_rt_sh_tlas_nodes_stage_buf);

    shared_data_stage_buf = std::move(_shared_data_stage_buf);

    visible_textures.realloc(MAX_TEX_COUNT);
    desired_textures.realloc(MAX_TEX_COUNT);

    cached_shadow_regions.realloc(MAX_SHADOWMAPS_TOTAL);
}

void Eng::DrawList::Clear() {
    instance_indices.count = 0;
    shadow_batches.count = 0;
    shadow_batch_indices.count = 0;
    shadow_lists.count = 0;
    shadow_regions.count = 0;
    basic_batches.count = 0;
    basic_batch_indices.count = 0;
    custom_batches.count = 0;
    custom_batch_indices.count = 0;
    skin_transforms.count = 0;
    skin_regions.count = 0;
    shape_keys_data.count = 0;
    lights.clear();
    decals.clear();
    probes.clear();
    ellipsoids.clear();

    items.count = 0;
    rt_items.count = 0;

    visible_textures.count = 0;
    desired_textures.count = 0;

    materials = nullptr;
    decals_atlas = nullptr;
    probe_storage = nullptr;

    cached_shadow_regions.count = 0;
}