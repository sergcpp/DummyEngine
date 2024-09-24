#include "BVHSplit.h"

#include <cfloat>

#include <algorithm>

namespace Phy {
const int BinningThreshold = 1024;
const int BinsCount = 256;

const float FLT_EPS = 0.0000001f;

struct bbox_t {
    Vec3f min = Vec3f{FLT_MAX}, max = Vec3f{-FLT_MAX};
    bbox_t() = default;
    bbox_t(const Vec3f &_min, const Vec3f &_max) : min(_min), max(_max) {}

    [[nodiscard]] float surface_area() const { return surface_area(min, max); }

    static float surface_area(const Vec3f &min, const Vec3f &max) {
        Vec3f d = max - min;
        return 2 * (d[0] + d[1] + d[2]);
        // return d[0] * d[1] + d[0] * d[2] + d[1] * d[2];
    }
};
} // namespace Phy

Phy::split_data_t Phy::SplitPrimitives_SAH(const prim_t *primitives, Span<const uint32_t> prim_indices,
                                           const Vec3f &bbox_min, const Vec3f &bbox_max, const split_settings_t &s) {
    const int prim_count = int(prim_indices.size());
    const bbox_t whole_box = {bbox_min, bbox_max};

    float res_sah = FLT_MAX;
    if (s.oversplit_threshold > 0) {
        res_sah = s.oversplit_threshold * whole_box.surface_area() * float(prim_count);
    }
    int div_axis = -1;
    uint32_t div_index = 0;
    bbox_t res_left_bounds, res_right_bounds;

    if (prim_count > BinningThreshold) {
        float div_pos = 0;

        for (int axis = 0; axis < 3; ++axis) {
            const float bounds_min = whole_box.min[axis];
            const float bounds_max = whole_box.max[axis];
            if (bounds_max - bounds_min < FLT_EPS) {
                // flat box
                continue;
            }

            const float Scale = float(BinsCount) / (bounds_max - bounds_min);

            struct bin_t {
                bbox_t bounds;
                int prim_count = 0;
            } bins[BinsCount];

            for (int i = 0; i < prim_count; ++i) {
                const prim_t &p = primitives[prim_indices[i]];
                const int bin_ndx = std::min(BinsCount - 1, int((p.bbox_min[axis] - bounds_min) * Scale));

                bins[bin_ndx].prim_count++;
                bins[bin_ndx].bounds.min = Min(bins[bin_ndx].bounds.min, p.bbox_min);
                bins[bin_ndx].bounds.max = Max(bins[bin_ndx].bounds.max, p.bbox_max);
            }

            float area_left[BinsCount - 1], area_right[BinsCount - 1];
            int count_left[BinsCount - 1], count_right[BinsCount - 1];

            bbox_t box_left, box_right;
            int prims_left = 0, prims_right = 0;
            for (int i = 0; i < BinsCount - 1; ++i) {
                prims_left += bins[i].prim_count;
                count_left[i] = prims_left;
                box_left.min = Min(box_left.min, bins[i].bounds.min);
                box_left.max = Max(box_left.max, bins[i].bounds.max);
                area_left[i] = box_left.surface_area();

                prims_right += bins[BinsCount - i - 1].prim_count;
                count_right[BinsCount - i - 2] = prims_right;
                box_right.min = Min(box_right.min, bins[BinsCount - i - 1].bounds.min);
                box_right.max = Max(box_right.max, bins[BinsCount - i - 1].bounds.max);
                area_right[BinsCount - i - 2] = box_right.surface_area();
            }

            const float scale = (bounds_max - bounds_min) / float(BinsCount);
            for (int i = 0; i < BinsCount - 1; ++i) {
                const float sah = float(count_left[i]) * area_left[i] + float(count_right[i]) * area_right[i];
                if (sah < res_sah) {
                    res_sah = sah;
                    div_axis = axis;
                    div_pos = bounds_min + scale * float(i + 1);
                }
            }
        }

        std::vector<uint32_t> left_indices, right_indices;
        if (div_axis != -1) {
            for (int i = 0; i < prim_count; ++i) {
                const prim_t &p = primitives[prim_indices[i]];
                if (p.bbox_min[div_axis] < div_pos) {
                    left_indices.push_back(prim_indices[i]);
                    res_left_bounds.min = Min(res_left_bounds.min, p.bbox_min);
                    res_left_bounds.max = Max(res_left_bounds.max, p.bbox_max);
                } else {
                    right_indices.push_back(prim_indices[i]);
                    res_right_bounds.min = Min(res_right_bounds.min, p.bbox_min);
                    res_right_bounds.max = Max(res_right_bounds.max, p.bbox_max);
                }
            }
        } else {
            left_indices.assign(prim_indices.begin(), prim_indices.end());
            res_left_bounds = whole_box;
        }

        return {std::move(left_indices),
                std::move(right_indices),
                {res_left_bounds.min, res_left_bounds.max},
                {res_right_bounds.min, res_right_bounds.max}};
    } else {
        SmallVector<uint32_t, 1024> axis_lists[3];

        if (prim_count > s.min_primitives_in_leaf) {
            for (int axis = 0; axis < 3; axis++) {
                axis_lists[axis].reserve(prim_count);
            }

            for (int i = 0; i < prim_count; i++) {
                axis_lists[0].push_back(i);
                axis_lists[1].push_back(i);
                axis_lists[2].push_back(i);
            }

            SmallVector<bbox_t, 1024> right_bounds(prim_count);

            for (int axis = 0; axis < 3; axis++) {
                auto &list = axis_lists[axis];

                std::sort(list.begin(), list.end(),
                          [axis, primitives, &prim_indices](uint32_t p1, uint32_t p2) -> bool {
                              return primitives[prim_indices[p1]].bbox_max[axis] <
                                     primitives[prim_indices[p2]].bbox_max[axis];
                          });

                bbox_t cur_right_bounds;
                for (size_t i = list.size() - 1; i > 0; i--) {
                    cur_right_bounds.min = Min(cur_right_bounds.min, primitives[prim_indices[list[i]]].bbox_min);
                    cur_right_bounds.max = Max(cur_right_bounds.max, primitives[prim_indices[list[i]]].bbox_max);
                    right_bounds[i - 1] = cur_right_bounds;
                }

                bbox_t left_bounds;
                for (size_t i = 1; i < list.size(); i++) {
                    left_bounds.min = Min(left_bounds.min, primitives[prim_indices[list[i - 1]]].bbox_min);
                    left_bounds.max = Max(left_bounds.max, primitives[prim_indices[list[i - 1]]].bbox_max);

                    const float sah =
                        left_bounds.surface_area() * float(i) + right_bounds[i - 1].surface_area() * float(list.size() - i);
                    if (sah < res_sah) {
                        res_sah = sah;
                        div_axis = axis;
                        div_index = uint32_t(i);
                        res_left_bounds = left_bounds;
                        res_right_bounds = right_bounds[i - 1];
                    }
                }
            }
        }

        std::vector<uint32_t> left_indices, right_indices;
        if (div_axis != -1) {
            left_indices.reserve(size_t(div_index));
            right_indices.reserve(prim_count - div_index);
            for (size_t i = 0; i < div_index; i++) {
                left_indices.push_back(prim_indices[axis_lists[div_axis][i]]);
            }
            for (size_t i = div_index; i < axis_lists[div_axis].size(); i++) {
                right_indices.push_back(prim_indices[axis_lists[div_axis][i]]);
            }
        } else {
            left_indices.insert(left_indices.end(), prim_indices.begin(), prim_indices.end());
            res_left_bounds = whole_box;
        }

        return {std::move(left_indices),
                std::move(right_indices),
                {res_left_bounds.min, res_left_bounds.max},
                {res_right_bounds.min, res_right_bounds.max}};
    }
}
