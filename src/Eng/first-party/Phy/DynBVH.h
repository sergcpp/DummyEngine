#pragma once

#include <cstdint>
#include <vector>

#include "BVHSplit.h"
#include "Span.h"
#include "Vec.h"

#ifdef __GNUC__
#define force_inline __attribute__((always_inline)) inline
#endif
#ifdef _MSC_VER
#define force_inline __forceinline
#endif

namespace Phy {
// Node of the dynamic object-level BVH. Packed words use explicit masks: the old bitfield
// unions aliased prim_count with right_child, hiding a broken leaf-vs-internal test.
struct dyn_bvh_node_t { // NOLINT
    Vec3f bbox_min;
    uint32_t leaf_or_left;   // bit0: leaf flag; bits1-31: prim_id (leaf) | left_child (internal)
    Vec3f bbox_max;
    uint32_t axis_or_right;  // bits0-1: sep axis; bits2-31: prim_count (leaf) | right_child (internal)
    uint32_t parent;         // BVH_NO_NODE for the root
};
static_assert(sizeof(dyn_bvh_node_t) == 36);

const uint32_t BVH_NO_NODE = 0xffffffffu;
const uint32_t BVH_INDEX_MASK = 0x7FFFFFFFu;
const uint32_t BVH_COUNT_MASK = 0x3FFFFFFFu;
const uint32_t BVH_AXIS_MASK = 3u;

force_inline bool bvh_is_leaf(const dyn_bvh_node_t &n) { return (n.leaf_or_left & 1u) != 0; }
force_inline uint32_t bvh_prim_id(const dyn_bvh_node_t &n) { return (n.leaf_or_left >> 1) & BVH_INDEX_MASK; }
force_inline uint32_t bvh_prim_count(const dyn_bvh_node_t &n) { return (n.axis_or_right >> 2) & BVH_COUNT_MASK; }
force_inline uint32_t bvh_left_child(const dyn_bvh_node_t &n) { return (n.leaf_or_left >> 1) & BVH_INDEX_MASK; }
force_inline uint32_t bvh_right_child(const dyn_bvh_node_t &n) { return (n.axis_or_right >> 2) & BVH_COUNT_MASK; }
force_inline uint32_t bvh_sep_axis(const dyn_bvh_node_t &n) { return n.axis_or_right & BVH_AXIS_MASK; }

force_inline void bvh_set_leaf(dyn_bvh_node_t &n, const uint32_t prim_id, const uint32_t prim_count) {
    n.leaf_or_left = (prim_id << 1) | 1u;
    n.axis_or_right = prim_count << 2;
}
force_inline void bvh_set_internal(dyn_bvh_node_t &n, const uint32_t left, const uint32_t right, const uint32_t axis) {
    n.leaf_or_left = left << 1;
    n.axis_or_right = (axis & BVH_AXIS_MASK) | (right << 2);
}
// Child-slot updates preserving the other child and the sep axis bits
force_inline void bvh_set_left_child(dyn_bvh_node_t &n, const uint32_t left) { n.leaf_or_left = left << 1; }
force_inline void bvh_set_right_child(dyn_bvh_node_t &n, const uint32_t right) {
    n.axis_or_right = (n.axis_or_right & BVH_AXIS_MASK) | (right << 2);
}

// Leaf node of the tree <-> caller-side primitive. prim_id is opaque to the tree
// (Eng passes object indices); it must be unique across the built set.
struct dyn_bvh_prim_t {
    Vec3f bbox_min, bbox_max;
    uint32_t prim_id;
};

struct dyn_bvh_settings_t {
    // Per-side hysteresis as a fraction of the prim extent (bounds are stored padded by
    // margin * extent; prims may move within that slack before being re-inserted)
    float bounds_margin = 0.2f;
    split_settings_t split;
};

namespace detail {
struct bvh_insert_candidate_t {
    uint32_t node_index;
    float direct_cost;
};
} // namespace detail

// Self-contained dynamic binary BVH: bulk Build, then RemoveLeaf + InsertPrim (SAH branch &
// bound + upward refit with rotations) or in-place RefitFromLeaf. Deterministic per sequence.
class DynBVH {
  public:
    void Build(Span<const dyn_bvh_prim_t> prims, const dyn_bvh_settings_t &settings);
    void Clear();

    uint32_t root() const { return root_; }
    Span<const dyn_bvh_node_t> nodes() const { return Span<const dyn_bvh_node_t>(nodes_.data(), nodes_.size()); }
    size_t node_capacity() const { return nodes_.size(); }
    size_t free_count() const { return free_nodes_.size(); }
    uint32_t insertions_since_build() const { return insertions_since_build_; }

    // Diagnostics for tests and engine debug hooks
    uint32_t bnb_expansions() const { return bnb_expansions_; }
    uint32_t rotations_applied() const { return rotations_applied_; }

    // All bbox arguments are raw (unpadded) prim bounds; the tree stores and compares padded ones
    bool LeafFits(uint32_t leaf_index, const Vec3f &bbox_min, const Vec3f &bbox_max) const;
    void RefitFromLeaf(uint32_t leaf_index, const Vec3f &bbox_min, const Vec3f &bbox_max);
    // Only leaf indices are accepted. Removing the root leaf empties the tree (root() == BVH_NO_NODE)
    void RemoveLeaf(uint32_t leaf_index);
    // Takes a free-list slot when available; caller must RemoveLeaf the old leaf first
    uint32_t InsertPrim(const Vec3f &bbox_min, const Vec3f &bbox_max, uint32_t prim_id);

    // Full structural check: parent/child symmetry, exact bbox consistency, reachability
    // exactly once, free-list bookkeeping, unique leaf prim_ids
    bool ValidateBVH() const;

  private:
    uint32_t alloc_node();
    void apply_margin(const Vec3f &bbox_min, const Vec3f &bbox_max, Vec3f &padded_min, Vec3f &padded_max) const;

    std::vector<dyn_bvh_node_t> nodes_;
    std::vector<uint32_t> free_nodes_;
    uint32_t root_ = BVH_NO_NODE;
    dyn_bvh_settings_t settings_;
    uint32_t insertions_since_build_ = 0;
    uint32_t bnb_expansions_ = 0;
    uint32_t rotations_applied_ = 0;
    std::vector<detail::bvh_insert_candidate_t> candidates_;
};

#undef force_inline
} // namespace Phy
