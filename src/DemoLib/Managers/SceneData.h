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

struct Drawable : public Ren::RefCounter {
    Ren::MeshRef mesh;
};

enum eObjectFlags { HasTransform = 1, HasDrawable = 2, IsOccluder = 4 };

struct SceneObject {
    uint32_t flags;
    Ren::StorageRef<Transform> tr;
    Ren::StorageRef<Drawable> dr;
};

using TimePoint = std::chrono::time_point<std::chrono::high_resolution_clock>;
using TimingInfo = std::pair<TimePoint, TimePoint>;

struct bvh_node_t {
    uint32_t prim_index, prim_count,
             left_child, right_child, parent,
             space_axis; // axis with maximal child's centroids distance
    float bbox[2][3];
};
static_assert(sizeof(bvh_node_t) == 48, "!");