#pragma once

#include <vector>

#include <Ren/MMat.h>

struct prim_t {
    Ren::Vec3f bbox_min, bbox_max;
};

struct split_data_t {
    std::vector<uint32_t> left_indices, right_indices;
    Ren::Vec3f left_bounds[2], right_bounds[2];
};

split_data_t SplitPrimitives_SAH(const prim_t *primitives, const std::vector<uint32_t> &prim_indices,
                                 const Ren::Vec3f &bbox_min, const Ren::Vec3f &bbox_max,
                                 const Ren::Vec3f &root_min, const Ren::Vec3f &root_max);
