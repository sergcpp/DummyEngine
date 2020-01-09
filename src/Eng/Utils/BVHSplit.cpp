#include "BVHSplit.h"

#include <algorithm>

struct bbox_t {
    Ren::Vec3f min = Ren::Vec3f{ std::numeric_limits<float>::max() },
               max = Ren::Vec3f{ std::numeric_limits<float>::lowest() };
    bbox_t() {}
    bbox_t(const Ren::Vec3f &_min, const Ren::Vec3f &_max) : min(_min), max(_max) {}

    float surface_area() const {
        return surface_area(min, max);
    }

    static float surface_area(const Ren::Vec3f &min, const Ren::Vec3f &max) {
        Ren::Vec3f d = max - min;
        return 2 * (d[0] + d[1] + d[2]);
        //return d[0] * d[1] + d[0] * d[2] + d[1] * d[2];
    }
};

split_data_t SplitPrimitives_SAH(const prim_t *primitives, const uint32_t *tri_indices, uint32_t tris_count,
                                 const Ren::Vec3f &bbox_min, const Ren::Vec3f &bbox_max,
                                 const Ren::Vec3f &root_min, const Ren::Vec3f &root_max, const split_settings_t &s) {
    bbox_t whole_box = { bbox_min, bbox_max };

    std::vector<uint32_t> axis_lists[3];
    for (int axis = 0; axis < 3; axis++) {
        axis_lists[axis].reserve(tris_count);
    }

    for (uint32_t i = 0; i < tris_count; i++) {
        axis_lists[0].push_back(i);
        axis_lists[1].push_back(i);
        axis_lists[2].push_back(i);
    }

    std::vector<bbox_t> new_prim_bounds;
    std::vector<bbox_t> right_bounds(tris_count);

    float res_sah = s.oversplit_threshold * whole_box.surface_area() * tris_count;
    int div_axis = -1;
    uint32_t div_index = 0;
    bbox_t res_left_bounds, res_right_bounds;

    for (int axis = 0; axis < 3; axis++) {
        std::vector<uint32_t> &list = axis_lists[axis];

        if (new_prim_bounds.empty()) {
            std::sort(list.begin(), list.end(),
            [axis, primitives, &tri_indices](uint32_t p1, uint32_t p2) -> bool {
                return primitives[tri_indices[p1]].bbox_max[axis] < primitives[tri_indices[p2]].bbox_max[axis];
            });
        } else {
            std::sort(list.begin(), list.end(),
            [axis, &new_prim_bounds](uint32_t p1, uint32_t p2) -> bool {
                return new_prim_bounds[p1].max[axis] < new_prim_bounds[p2].max[axis];
            });
        }

        bbox_t cur_right_bounds;
        if (new_prim_bounds.empty()) {
            for (size_t i = list.size() - 1; i > 0; i--) {
                cur_right_bounds.min = Min(cur_right_bounds.min, primitives[tri_indices[list[i]]].bbox_min);
                cur_right_bounds.max = Max(cur_right_bounds.max, primitives[tri_indices[list[i]]].bbox_max);
                right_bounds[i - 1] = cur_right_bounds;
            }
        } else {
            for (size_t i = list.size() - 1; i > 0; i--) {
                cur_right_bounds.min = Min(cur_right_bounds.min, new_prim_bounds[list[i]].min);
                cur_right_bounds.max = Max(cur_right_bounds.max, new_prim_bounds[list[i]].max);
                right_bounds[i - 1] = cur_right_bounds;
            }
        }

        bbox_t left_bounds;
        for (size_t i = 1; i < list.size(); i++) {
            if (new_prim_bounds.empty()) {
                left_bounds.min = Min(left_bounds.min, primitives[tri_indices[list[i - 1]]].bbox_min);
                left_bounds.max = Max(left_bounds.max, primitives[tri_indices[list[i - 1]]].bbox_max);
            } else {
                left_bounds.min = Min(left_bounds.min, new_prim_bounds[list[i - 1]].min);
                left_bounds.max = Max(left_bounds.max, new_prim_bounds[list[i - 1]].max);
            }

            float sah = s.node_traversal_cost * whole_box.surface_area() + left_bounds.surface_area() * i + right_bounds[i - 1].surface_area() * (list.size() - i);
            if (sah < res_sah) {
                res_sah = sah;
                div_axis = axis;
                div_index = (uint32_t)i;
                res_left_bounds = left_bounds;
                res_right_bounds = right_bounds[i - 1];
            }
        }
    }

    std::vector<uint32_t> left_indices, right_indices;
    if (div_axis != -1) {
        left_indices.reserve((size_t)div_index);
        right_indices.reserve(tris_count - div_index);
        for (size_t i = 0; i < div_index; i++) {
            left_indices.push_back(tri_indices[axis_lists[div_axis][i]]);
        }
        for (size_t i = div_index; i < axis_lists[div_axis].size(); i++) {
            right_indices.push_back(tri_indices[axis_lists[div_axis][i]]);
        }
    } else {
        left_indices.insert(left_indices.end(), tri_indices, tri_indices + tris_count);
        res_left_bounds = whole_box;
    }

    return { std::move(left_indices), std::move(right_indices), { res_left_bounds.min, res_left_bounds.max }, { res_right_bounds.min, res_right_bounds.max } };
}
