#pragma once

#include <Ren/Camera.h>

#include "../scene/SceneData.h"
#include "Renderer_Structs.h"

namespace Eng {
struct LightSource;

template <typename T> struct DynArray {
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

    static_assert(std::is_default_constructible<T>::value && std::is_trivially_copyable<T>::value,
                  "DynArray is intended to be used with simple data types!");
};

struct ShadReg {
    int ls_index = -1, reg_index = -1;
    int pos[2], size[2];
    float cam_near, cam_far; // for debugging
    uint32_t last_update, last_visible;
};

struct EnvironmentWeak {
    Ren::Vec3f sun_dir, sun_col;
    float sun_angle = 0.0f;
    Ren::Vec3f wind_vec;
    float wind_turbulence = 0.0f;
    Ren::Vec2f prev_wind_scroll_lf, prev_wind_scroll_hf;
    Ren::Vec2f curr_wind_scroll_lf, curr_wind_scroll_hf;
    Ren::WeakTex2DRef env_map;
    Ren::WeakTex2DRef lm_direct, lm_indir, lm_indir_sh[4];
    float sun_shadow_bias[2] = {4.0f, 8.0f};
    Ren::Vec3f ambient_hack;

    Ren::String env_map_name, env_map_name_pt;

    EnvironmentWeak() = default;
    explicit EnvironmentWeak(const Environment &env) {
        sun_dir = env.sun_dir;
        sun_col = env.sun_col;
        sun_angle = env.sun_angle;
        wind_vec = env.wind_vec;
        wind_turbulence = env.wind_turbulence;
        prev_wind_scroll_lf = env.prev_wind_scroll_lf;
        prev_wind_scroll_hf = env.prev_wind_scroll_hf;
        curr_wind_scroll_lf = env.curr_wind_scroll_lf;
        curr_wind_scroll_hf = env.curr_wind_scroll_hf;
        env_map = env.env_map;
        lm_direct = env.lm_direct;
        lm_indir = env.lm_indir;
        for (int i = 0; i < 4; i++) {
            lm_indir_sh[i] = env.lm_indir_sh[i];
        }
        sun_shadow_bias[0] = env.sun_shadow_bias[0];
        sun_shadow_bias[1] = env.sun_shadow_bias[1];
        ambient_hack = env.ambient_hack;
        env_map_name = env.env_map_name;
        env_map_name_pt = env.env_map_name_pt;
    }
};
// static_assert(sizeof(EnvironmentWeak) == sizeof(Environment), "!");

// TODO: use persistent mapping of stage buffers
struct DrawList {
    render_settings_t render_settings = {};
    uint32_t frame_index = 0;
    Ren::Camera draw_cam, ext_cam;
    EnvironmentWeak env;
    FrontendInfo frontend_info;
    std::vector<Ren::Vec2i> instance_indices;
    mutable Ren::BufferRef instance_indices_stage_buf;
    std::vector<BasicDrawBatch> shadow_batches;
    std::vector<uint32_t> shadow_batch_indices;
    DynArray<ShadowList> shadow_lists;
    DynArray<ShadowMapRegion> shadow_regions;
    std::vector<BasicDrawBatch> basic_batches;
    std::vector<uint32_t> basic_batch_indices;
    std::vector<CustomDrawBatch> custom_batches;
    std::vector<uint32_t> custom_batch_indices;
    int alpha_blend_start_index = -1;
    std::vector<SkinTransform> skin_transforms;
    mutable Ren::BufferRef skin_transforms_stage_buf;
    std::vector<SkinRegion> skin_regions;
    DynArray<ShapeKeyData> shape_keys_data;
    mutable Ren::BufferRef shape_keys_stage_buf;
    uint32_t skin_vertices_count = 0;
    std::vector<LightItem> lights;
    mutable Ren::BufferRef lights_stage_buf;
    std::vector<DecalItem> decals;
    mutable Ren::BufferRef decals_stage_buf;
    std::vector<ProbeItem> probes;
    std::vector<EllipsItem> ellipsoids;
    DynArray<CellData> cells, rt_cells;
    mutable Ren::BufferRef cells_stage_buf, rt_cells_stage_buf;
    DynArray<ItemData> items, rt_items;
    mutable Ren::BufferRef items_stage_buf, rt_items_stage_buf;

    DynArray<RTGeoInstance> rt_geo_instances;
    DynArray<RTObjInstance> rt_obj_instances[int(eTLASIndex::_Count)];
    mutable Ren::BufferRef rt_obj_instances_stage_buf[int(eTLASIndex::_Count)];
    struct {
        mutable Ren::BufferRef rt_tlas_nodes_stage_buf[int(eTLASIndex::_Count)];
    } swrt;

    mutable Ren::BufferRef shared_data_stage_buf;

    std::vector<TexEntry> visible_textures;
    std::vector<TexEntry> desired_textures;

    const Ren::MaterialStorage *materials = nullptr;
    const Ren::TextureAtlas *decals_atlas = nullptr;
    const Ren::ProbeStorage *probe_storage = nullptr;

    DynArray<ShadReg> cached_shadow_regions;

    // for debugging only, backend does not require nodes for drawing
    std::vector<bvh_node_t> temp_nodes;
    uint32_t root_index = 0xffffffff;
    // culling depth buffer
    int depth_w, depth_h;
    std::vector<uint8_t> depth_pixels;

    void Init(Ren::BufferRef shared_data_stage_buf, Ren::BufferRef instances_stage_buf,
              Ren::BufferRef instance_indices_stage_buf, Ren::BufferRef skin_transforms_stage_buf,
              Ren::BufferRef shape_keys_stage_buf, Ren::BufferRef cells_stage_buf, Ren::BufferRef rt_cells_stage_buf,
              Ren::BufferRef items_stage_buf, Ren::BufferRef rt_items_stage_buf, Ren::BufferRef lights_stage_buf,
              Ren::BufferRef decals_stage_buf, Ren::BufferRef rt_obj_instances_stage_buf,
              Ren::BufferRef rt_sh_obj_instances_stage_buf, Ren::BufferRef rt_tlas_nodes_stage_buf,
              Ren::BufferRef rt_sh_tlas_nodes_stage_buf);
    void Clear();
};
} // namespace Eng