#pragma once

#include <Ren/Camera.h>

#include "Renderer_Structs.h"
#include "../Scene/SceneData.h"

struct LightSource;
class TextureAtlas;

template <typename T>
struct DynArray {
    T *data;
    uint32_t count, capacity;

    DynArray() : data(nullptr), count(0), capacity(0) {}
    ~DynArray() { delete[] data; }

    DynArray(const DynArray &rhs) = delete;

    void realloc(uint32_t _capacity) {
        const T *old_data = data;

        data = new T[_capacity];
        capacity = _capacity;
        count = std::min(count, capacity);

        if (old_data && count) {
            memcpy(data, old_data, count * sizeof(T));
            delete[] old_data;
        }
    }

    static_assert(std::is_default_constructible<T>::value &&
                  std::is_trivially_copyable<T>::value, "DynArray is intended to be used with simple data types!");
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
    uint32_t                    skin_vertices_count = 0;
    DynArray<LightSourceItem>   light_sources;
    DynArray<DecalItem>         decals;
    DynArray<ProbeItem>         probes;
    DynArray<CellData>          cells;
    DynArray<ItemData>          items;
    const Ren::TextureAtlas     *decals_atlas = nullptr;
    const ProbeStorage          *probe_storage = nullptr;

    DynArray<ShadReg>           cached_shadow_regions;

    // for debugging only, backend does not require nodes for drawing
    std::vector<bvh_node_t>     temp_nodes;
    uint32_t                    root_index = 0xffffffff;

    DrawList();
};
