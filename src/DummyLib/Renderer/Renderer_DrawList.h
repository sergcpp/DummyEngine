#pragma once

#include <Ren/Camera.h>

#include "Renderer_Structs.h"
#include "../Scene/SceneData.h"

struct LightSource;
class TextureAtlas;

template <typename T>
struct DynArray {
    std::unique_ptr<T[]> data;
    uint32_t count, capacity;
};

struct ShadReg {
    const LightSource *ls;
    int pos[2], size[2];
    float cam_near, cam_far; // for debugging
    uint32_t last_update, last_visible;
};

struct DrawList {
    uint32_t                    render_flags = 0xffffffff;
    Ren::Camera                 draw_cam;
    Environment                 env;
    FrontendInfo                frontend_info;
    DynArray<InstanceData>      instances;
    DynArray<DepthDrawBatch>    shadow_batches;
    DynArray<uint32_t>          shadow_batch_indices;
    DynArray<ShadowList>        shadow_lists;
    DynArray<ShadowMapRegion>   shadow_regions;
    DynArray<DepthDrawBatch>    zfill_batches;
    DynArray<uint32_t>          zfill_batch_indices;
    DynArray<MainDrawBatch>     main_batches;
    DynArray<uint32_t>          main_batch_indices;
    DynArray<SkinTransform>     skin_transforms;
    DynArray<SkinRegion>        skin_regions;
    uint32_t                    skin_vertices_count;
    DynArray<LightSourceItem>   light_sources;
    DynArray<DecalItem>         decals;
    DynArray<ProbeItem>         probes;
    DynArray<CellData>          cells;
    DynArray<ItemData>          items;
    const Ren::TextureAtlas     *decals_atlas = nullptr;
    const ProbeStorage          *probe_storage = nullptr;

    // for debugging only, backend does not require nodes for drawing
    std::vector<bvh_node_t>     temp_nodes;
    uint32_t                    root_index;
    DynArray<ShadReg>           cached_shadow_regions;

    DrawList();
};