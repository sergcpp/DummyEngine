#pragma once

#include <cstdint>

#include <Ren/Mesh.h>
#include <Ren/MMat.h>
#include <Ren/Storage.h>
#include <Ren/TextureAtlas.h>

struct JsObject;

struct Transform : public Ren::RefCounter {
    Ren::Mat4f mat;
    Ren::Vec3f bbox_min;
    uint32_t node_index;
    Ren::Vec3f bbox_max;
    Ren::Vec3f bbox_min_ws, bbox_max_ws;

    void Read(const JsObject &js_in);
    void Write(JsObject &js_out);

    void UpdateBBox() {
        bbox_min_ws = bbox_max_ws = Ren::Vec3f(mat[3]);

        for (int j = 0; j < 3; j++) {
            for (int i = 0; i < 3; i++) {
                float a = mat[i][j] * bbox_min[i];
                float b = mat[i][j] * bbox_max[i];

                if (a < b) {
                    bbox_min_ws[j] += a;
                    bbox_max_ws[j] += b;
                } else {
                    bbox_min_ws[j] += b;
                    bbox_max_ws[j] += a;
                }
            }
        }
    }
};

struct LightSource : public Ren::RefCounter {
    Ren::Vec3f offset;
    float radius;
    Ren::Vec3f col;
    float brightness;
    Ren::Vec3f dir;
    float spot;
    float influence;

    Ren::Vec3f bbox_min, bbox_max;

    void Read(const JsObject &js_in);
    void Write(JsObject &js_out);
};

struct Decal : public Ren::RefCounter {
    Ren::Mat4f view, proj;
    Ren::Vec4f diff, norm, spec;

    void Read(const JsObject &js_in);
    void Write(JsObject &js_out);
};

struct LightmapRegion : public Ren::RefCounter {
    int pos[2], size[2];
    Ren::Vec4f xform;
};

enum eObjectComp {
    HasTransform    = (1 << 0),
    HasMesh         = (1 << 1),
    HasOccluder     = (1 << 2),
    HasLightmap     = (1 << 3),
    HasLightSource  = (1 << 4),
    HasDecal        = (1 << 5)
};

enum eObjectChange {
    ChangePosition = (1 << 0),
};

const int LIGHTS_PER_OBJECT = 16;
const int DECALS_PER_OBJECT = 16;

const float LIGHT_ATTEN_CUTOFF = 0.001f;

struct SceneObject {
    uint32_t comp_mask, change_mask;
    Ren::StorageRef<Transform> tr;
    Ren::MeshRef mesh, occ_mesh;
    uint32_t pt_mi;
    Ren::StorageRef<LightmapRegion> lm;
    Ren::StorageRef<LightSource> ls[LIGHTS_PER_OBJECT];
    Ren::StorageRef<Decal> de[DECALS_PER_OBJECT];
};

struct RenderInfo {
    uint32_t lights_count = 0, decals_count = 0;
    uint32_t lights_data_size = 0,
             decals_data_size = 0,
             cells_data_size = 0,
             items_data_size = 0;
};

struct FrontendInfo {
    uint64_t start_timepoint_us = 0,
             end_timepoint_us = 0;
    uint32_t occluders_time_us = 0,
             main_gather_time_us = 0,
             drawables_sort_time_us = 0,
             shadow_gather_time_us = 0,
             items_assignment_time_us = 0;
};

struct BackendInfo {
    uint64_t cpu_start_timepoint_us = 0,
             cpu_end_timepoint_us = 0;
    uint64_t gpu_start_timepoint_us = 0,
             gpu_end_timepoint_us = 0;
    uint32_t shadow_time_us = 0,
             depth_pass_time_us = 0,
             ao_pass_time_us = 0,
             opaque_pass_time_us = 0,
             refl_pass_time_us = 0,
             blur_pass_time_us = 0,
             blit_pass_time_us = 0;
    int64_t gpu_cpu_time_diff_us = 0;

    uint32_t shadow_draw_calls_count = 0,
             depth_fill_draw_calls_count = 0,
             opaque_draw_calls_count = 0;
};

struct bvh_node_t {
    uint32_t prim_index, prim_count,
             left_child, right_child;
    Ren::Vec3f bbox_min;
    uint32_t parent;
    Ren::Vec3f bbox_max;
    uint32_t space_axis; // axis with maximal child's centroids distance
};
static_assert(sizeof(bvh_node_t) == 48, "!");

struct Environment {
    Ren::Vec3f sun_dir, sun_col;
    float sun_softness = 0.0f;
    Ren::Texture2DRef env_map;
    Ren::Texture2DRef lm_direct, lm_indir,
                      lm_indir_sh[4];
};

struct BBox {
    Ren::Vec3f bmin, bmax;
};

struct SceneData {
    Environment env;
    Ren::TextureAtlas decals_atlas;
    Ren::TextureSplitter lm_splitter;

    Ren::Storage<Transform> transforms;
    Ren::Storage<LightmapRegion> lm_regions;
    Ren::Storage<LightSource> lights;
    Ren::Storage<Decal> decals;

    std::vector<SceneObject> objects;

    std::vector<bvh_node_t> nodes;
    std::vector<uint32_t> free_nodes;
    uint32_t root_node = 0;
};