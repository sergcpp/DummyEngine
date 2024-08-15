#pragma once

#include <cstdint>
#include <vector>

#include "MMat.h"
#include "Span.h"

namespace Phy {
struct prim_t {
    uint32_t i0, i1, i2;
    Vec3f bbox_min, bbox_max;
};

struct split_data_t {
    std::vector<uint32_t> left_indices, right_indices;
    Vec3f left_bounds[2], right_bounds[2];
};

struct split_settings_t {
    float oversplit_threshold = 0.95f;
    int min_primitives_in_leaf = 8;
};

split_data_t SplitPrimitives_SAH(const prim_t *primitives, Span<const uint32_t> prim_indices,
                                 const Vec3f &bbox_min, const Vec3f &bbox_max, const split_settings_t &s);
} // namespace Eng