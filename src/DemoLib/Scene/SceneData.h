#pragma once

#include <cstdint>
#include <chrono>

#include <Ren/Mesh.h>
#include <Ren/MMat.h>
#include <Ren/Storage.h>

struct Transform : public Ren::RefCounter {
    Ren::Mat4f mat;
    Ren::Vec3f bbox_min_ws, bbox_max_ws;

    void UpdateBBox(const Ren::Vec3f &bbox_min, const Ren::Vec3f &bbox_max) {
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
};

struct Decal : public Ren::RefCounter {
    Ren::Mat4f view, proj;
    Ren::Vec4f diff, norm;
};

enum eObjectFlags {
    HasTransform    = (1 << 0),
    HasMesh         = (1 << 1),
    HasOccluder     = (1 << 2),
    UseLightmap     = (1 << 3),
    HasLightSource  = (1 << 4),
    HasDecal        = (1 << 5)
};

const int LIGHTS_PER_OBJECT = 16;
const int DECALS_PER_OBJECT = 16;

struct SceneObject {
    uint32_t flags;
    Ren::StorageRef<Transform> tr;
    Ren::MeshRef mesh, occ_mesh;
    Ren::Texture2DRef lm_dir_tex, lm_indir_tex, lm_indir_sh_tex[4];
    uint32_t lm_res, pt_mi;
    Ren::StorageRef<LightSource> ls[LIGHTS_PER_OBJECT];
    Ren::StorageRef<Decal> de[DECALS_PER_OBJECT];
};

using TimePoint = std::chrono::time_point<std::chrono::high_resolution_clock>;
using TimingInfo = std::pair<TimePoint, TimePoint>;

struct RenderInfo {
    uint32_t lights_count = 0, decals_count = 0;
    uint32_t lights_data_size = 0,
             decals_data_size = 0,
             cells_data_size = 0,
             items_data_size = 0;
};

struct BackendInfo {
    uint32_t shadow_time_us = 0,
             depth_pass_time_us = 0,
             ao_pass_time_us = 0,
             opaque_pass_time_us = 0,
             refl_pass_time_us = 0;
};

struct bvh_node_t {
    uint32_t prim_index, prim_count,
             left_child, right_child, parent,
             space_axis; // axis with maximal child's centroids distance
    Ren::Vec3f bbox[2];
};
static_assert(sizeof(bvh_node_t) == 48, "!");

struct Environment {
    Ren::Vec3f sun_dir, sun_col, sky_col;
    float sun_softness = 0.0f;
};

struct BBox {
    Ren::Vec3f bmin, bmax;
};