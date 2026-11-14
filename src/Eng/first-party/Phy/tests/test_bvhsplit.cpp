#include "test_common.h"

#include <cfloat>
#include <cstdio>
#include <limits>
#include <vector>

#include "../BVHSplit.h"

namespace {
using namespace Phy;

split_settings_t rebuild_like_settings() {
    split_settings_t s;
    // Exact settings used by Eng SceneManager::RebuildSceneBVH
    s.oversplit_threshold = std::numeric_limits<float>::max();
    s.min_primitives_in_leaf = 1;
    return s;
}

uint32_t next_rand(uint32_t &seed) {
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    return seed;
}

std::vector<prim_t> make_identical_prims(const int count, const Vec3f &bmin, const Vec3f &bmax) {
    std::vector<prim_t> prims;
    prims.reserve(count);
    for (int i = 0; i < count; i++) {
        prims.push_back({0, 0, 0, bmin, bmax});
    }
    return prims;
}

// Zero-z-thickness boxes at deterministic pseudo-random x/y positions
std::vector<prim_t> make_scattered_flat_prims(const int count, uint32_t seed) {
    std::vector<prim_t> prims;
    prims.reserve(count);
    for (int i = 0; i < count; i++) {
        const float x = float(next_rand(seed) % 100000u) * 0.01f - 500.0f;
        const float y = float(next_rand(seed) % 100000u) * 0.01f - 500.0f;
        const Vec3f bmin{x, y, 0.0f};
        prims.push_back({0, 0, 0, bmin, Vec3f{x + 1.0f, y + 1.0f, 0.0f}});
    }
    return prims;
}

void union_bounds(const std::vector<prim_t> &prims, Vec3f &bmin, Vec3f &bmax) {
    bmin = Vec3f{std::numeric_limits<float>::max()};
    bmax = Vec3f{std::numeric_limits<float>::lowest()};
    for (const prim_t &p : prims) {
        for (int a = 0; a < 3; a++) {
            bmin[a] = bmin[a] < p.bbox_min[a] ? bmin[a] : p.bbox_min[a];
            bmax[a] = bmax[a] > p.bbox_max[a] ? bmax[a] : p.bbox_max[a];
        }
    }
}

split_data_t run_split(const std::vector<prim_t> &prims, const split_settings_t &settings) {
    std::vector<uint32_t> indices(prims.size());
    for (size_t i = 0; i < indices.size(); i++) {
        indices[i] = uint32_t(i);
    }
    Vec3f bmin, bmax;
    union_bounds(prims, bmin, bmax);
    return SplitPrimitives_SAH(prims.data(), Span<const uint32_t>(indices.data(), indices.size()), bmin, bmax,
                               settings);
}

// Every input index must appear exactly once across both sides
void require_valid_partition(const split_data_t &res, const std::vector<prim_t> &prims) {
    const size_t total = prims.size();
    require(res.left_indices.size() + res.right_indices.size() == total);

    std::vector<uint8_t> seen(total, 0);
    auto check_side_indices = [&](const std::vector<uint32_t> &side) {
        for (const uint32_t i : side) {
            require(i < total);
            require(seen[i] == 0);
            seen[i] = 1;
        }
    };
    check_side_indices(res.left_indices);
    check_side_indices(res.right_indices);
    for (const uint8_t s : seen) {
        require(s == 1);
    }
}

// Each side's returned bounds must contain its prims' bboxes exactly (no epsilon: bounds are exact merges)
void require_bounds_contain(const split_data_t &res, const std::vector<prim_t> &prims) {
    auto check_side = [&](const std::vector<uint32_t> &side, const Vec3f bounds[2]) {
        for (const uint32_t i : side) {
            for (int a = 0; a < 3; a++) {
                require(bounds[0][a] <= prims[i].bbox_min[a]);
                require(bounds[1][a] >= prims[i].bbox_max[a]);
            }
        }
    };
    check_side(res.left_indices, res.left_bounds);
    check_side(res.right_indices, res.right_bounds);
}

void require_both_sides_nonempty(const split_data_t &res) {
    require(!res.left_indices.empty());
    require(!res.right_indices.empty());
}
} // namespace

void test_bvhsplit() {
    printf("Test bvhsplit           | ");

    { // leaf below min_primitives_in_leaf stays a leaf
        const auto prims = make_identical_prims(8, Vec3f{1.0f, 2.0f, 3.0f}, Vec3f{2.0f, 4.0f, 6.0f});
        const split_settings_t s; // defaults: oversplit 0.95, min_primitives_in_leaf 8
        const split_data_t res = run_split(prims, s);
        require(res.right_indices.empty());
        require(res.left_indices.size() == 8);
        require_bounds_contain(res, prims);
    }
    { // 100 identical boxes (brute path) must split
        const auto prims = make_identical_prims(100, Vec3f{1.0f, 2.0f, 3.0f}, Vec3f{2.0f, 4.0f, 6.0f});
        const split_data_t res = run_split(prims, rebuild_like_settings());
        require_valid_partition(res, prims);
        require_both_sides_nonempty(res);
        require_bounds_contain(res, prims);
    }
    { // 1100 scattered flat boxes (bin path) must split
        const auto prims = make_scattered_flat_prims(1100, 0x9E3779B9u);
        const split_data_t res = run_split(prims, rebuild_like_settings());
        require_valid_partition(res, prims);
        require_both_sides_nonempty(res);
        require_bounds_contain(res, prims);
    }
    { // 1100 identical boxes (bin path) must split
        const auto prims = make_identical_prims(1100, Vec3f{1.0f, 2.0f, 3.0f}, Vec3f{2.0f, 4.0f, 6.0f});
        const split_data_t res = run_split(prims, rebuild_like_settings());
        require_valid_partition(res, prims);
        require_both_sides_nonempty(res);
        require_bounds_contain(res, prims);
    }
    { // 100 identical zero-extent points (brute path) must split
        const auto prims = make_identical_prims(100, Vec3f{1.0f, 2.0f, 3.0f}, Vec3f{1.0f, 2.0f, 3.0f});
        const split_data_t res = run_split(prims, rebuild_like_settings());
        require_valid_partition(res, prims);
        require_both_sides_nonempty(res);
        require_bounds_contain(res, prims);
    }
    { // 1100 identical zero-extent points (bin path) must split
        const auto prims = make_identical_prims(1100, Vec3f{1.0f, 2.0f, 3.0f}, Vec3f{1.0f, 2.0f, 3.0f});
        const split_data_t res = run_split(prims, rebuild_like_settings());
        require_valid_partition(res, prims);
        require_both_sides_nonempty(res);
        require_bounds_contain(res, prims);
    }
    { // 1100 identical axis-degenerate segments (bin path) must split
        const auto prims = make_identical_prims(1100, Vec3f{1.0f, 2.0f, 3.0f}, Vec3f{1.0f, 2.0f, 4.0f});
        const split_data_t res = run_split(prims, rebuild_like_settings());
        require_valid_partition(res, prims);
        require_both_sides_nonempty(res);
        require_bounds_contain(res, prims);
    }
    { // 1000 identical coplanar quads (default settings) must split
        // Same-shape stacked coplanar triangles are the realistic SWRT BLAS case where the SAH stop
        // condition declines any split; leaves must still respect min_primitives_in_leaf via fallback.
        const auto prims = make_identical_prims(1000, Vec3f{0.0f, 0.0f, 5.0f}, Vec3f{1.0f, 1.0f, 5.0f});
        const split_settings_t s; // defaults: oversplit 0.95, min_primitives_in_leaf 8
        const split_data_t res = run_split(prims, s);
        require_valid_partition(res, prims);
        require_both_sides_nonempty(res);
        require_bounds_contain(res, prims);
    }
    { // 9 identical boxes at min_primitives_in_leaf=8 (default settings) must split
        const auto prims = make_identical_prims(9, Vec3f{1.0f, 2.0f, 3.0f}, Vec3f{2.0f, 4.0f, 6.0f});
        const split_settings_t s; // defaults: oversplit 0.95, min_primitives_in_leaf 8
        const split_data_t res = run_split(prims, s);
        require_valid_partition(res, prims);
        require_both_sides_nonempty(res);
        require_bounds_contain(res, prims);
    }
    { // same input must produce identical partitions
        const auto prims = make_identical_prims(1100, Vec3f{1.0f, 2.0f, 3.0f}, Vec3f{2.0f, 4.0f, 6.0f});
        const split_data_t res1 = run_split(prims, rebuild_like_settings());
        const split_data_t res2 = run_split(prims, rebuild_like_settings());
        require(res1.left_indices == res2.left_indices);
        require(res1.right_indices == res2.right_indices);
        for (int side = 0; side < 2; side++) {
            for (int b = 0; b < 2; b++) {
                for (int a = 0; a < 3; a++) {
                    require(res1.left_bounds[side][b] == res2.left_bounds[side][b]);
                    require(res1.right_bounds[side][b] == res2.right_bounds[side][b]);
                }
            }
        }
    }

    printf("OK\n");
}
