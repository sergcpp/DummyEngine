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
    Ren::Vec3f sun_dir, sun_col, env_col;
    float env_map_rot = 0.0f;
    float sun_angle = 0.0f;
    Ren::Vec3f wind_vec;
    float wind_turbulence = 0.0f;
    Ren::Vec2f prev_wind_scroll_lf, prev_wind_scroll_hf;
    Ren::Vec2f curr_wind_scroll_lf, curr_wind_scroll_hf;
    Ren::WeakTexRef env_map;
    Ren::WeakTexRef lm_direct, lm_indir, lm_indir_sh[4];
    float sun_shadow_bias[2] = {4.0f, 8.0f};
    uint32_t generation = 0xffffffff;

    Ren::String env_map_name;
    atmosphere_params_t atmosphere;
    volume_params_t fog;

    EnvironmentWeak() = default;
    explicit EnvironmentWeak(const Environment &env) {
        sun_dir = env.sun_dir;
        sun_col = env.sun_col;
        env_col = env.env_col;
        env_map_rot = env.env_map_rot;
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
        env_map_name = env.env_map_name;
        atmosphere = env.atmosphere;
        fog = env.fog;
        generation = env.generation;
    }
};
// static_assert(sizeof(EnvironmentWeak) == sizeof(Environment), "!");

// TODO: use persistent mapping of stage buffers
struct DrawList {
    render_settings_t render_settings = {};
    uint32_t frame_index = 0;
    Ren::Camera draw_cam, ext_cam;
    EnvironmentWeak env;
    frontend_info_t frontend_info;
    Ren::Vec4f sun_shadow_bounds;
    std::vector<Ren::Vec2i> instance_indices;
    mutable Ren::BufRef instance_indices_stage_buf;
    std::vector<basic_draw_batch_t> shadow_batches;
    std::vector<uint32_t> shadow_batch_indices;
    DynArray<shadow_list_t> shadow_lists;
    DynArray<shadow_map_region_t> shadow_regions;
    std::vector<basic_draw_batch_t> basic_batches;
    std::vector<uint32_t> basic_batch_indices;
    std::vector<custom_draw_batch_t> custom_batches;
    std::vector<uint32_t> custom_batch_indices;
    int alpha_blend_start_index = -1;
    int emissive_start_index = -1;
    std::vector<skin_transform_t> skin_transforms;
    mutable Ren::BufRef skin_transforms_stage_buf;
    std::vector<skin_region_t> skin_regions;
    DynArray<shape_key_data_t> shape_keys_data;
    mutable Ren::BufRef shape_keys_stage_buf;
    uint32_t skin_vertices_count = 0;
    std::vector<light_item_t> lights;
    mutable Ren::BufRef lights_stage_buf;
    std::vector<decal_item_t> decals;
    mutable Ren::BufRef decals_stage_buf;
    std::vector<probe_item_t> probes;
    std::vector<ellipse_item_t> ellipsoids;
    std::vector<uint32_t> portals;
    DynArray<cell_data_t> cells, rt_cells;
    mutable Ren::BufRef cells_stage_buf, rt_cells_stage_buf;
    DynArray<item_data_t> items, rt_items;
    mutable Ren::BufRef items_stage_buf, rt_items_stage_buf;

    DynArray<rt_geo_instance_t> rt_geo_instances[int(eTLASIndex::_Count)];
    mutable Ren::BufRef rt_geo_instances_stage_buf[int(eTLASIndex::_Count)];
    DynArray<rt_obj_instance_t> rt_obj_instances[int(eTLASIndex::_Count)];
    mutable Ren::BufRef rt_obj_instances_stage_buf[int(eTLASIndex::_Count)];
    struct {
        mutable Ren::BufRef rt_tlas_nodes_stage_buf[int(eTLASIndex::_Count)];
    } swrt;

    mutable Ren::BufRef shared_data_stage_buf;

    int volume_to_update = -1;
    Ren::Vec3f bbox_min, bbox_max;

    std::vector<TexEntry> visible_textures;
    std::vector<TexEntry> desired_textures;

    const Ren::MaterialStorage *materials = nullptr;
    const Ren::TextureAtlas *decals_atlas = nullptr;

    DynArray<ShadReg> cached_shadow_regions;

    // for debugging only, backend does not require nodes for drawing
    std::vector<bvh_node_t> temp_nodes;
    uint32_t root_index = 0xffffffff;
    // culling depth buffer
    int depth_w, depth_h;
    std::vector<uint8_t> depth_pixels;

    void Init(Ren::BufRef shared_data_stage_buf, Ren::BufRef instance_indices_stage_buf,
              Ren::BufRef skin_transforms_stage_buf, Ren::BufRef shape_keys_stage_buf, Ren::BufRef cells_stage_buf,
              Ren::BufRef rt_cells_stage_buf, Ren::BufRef items_stage_buf, Ren::BufRef rt_items_stage_buf,
              Ren::BufRef lights_stage_buf, Ren::BufRef decals_stage_buf, Ren::BufRef rt_geo_instances_stage_buf,
              Ren::BufRef rt_sh_geo_instances_stage_buf, Ren::BufRef rt_obj_instances_stage_buf,
              Ren::BufRef rt_sh_obj_instances_stage_buf, Ren::BufRef rt_tlas_nodes_stage_buf,
              Ren::BufRef rt_sh_tlas_nodes_stage_buf);
    void Clear();
};
} // namespace Eng