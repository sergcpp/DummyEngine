#include "DynBVH.h"

#include <algorithm>
#include <cassert>
#include <deque>
#include <limits>

#include "Bounds.h"

namespace Phy {
namespace {
using detail::bvh_insert_candidate_t;

float surface_area(const dyn_bvh_node_t &n) {
    const Vec3f d = n.bbox_max - n.bbox_min;
    return d[0] * d[1] + d[0] * d[2] + d[1] * d[2];
}

float surface_area_of_union(const dyn_bvh_node_t &n1, const dyn_bvh_node_t &n2) {
    const Vec3f d = Max(n1.bbox_max, n2.bbox_max) - Min(n1.bbox_min, n2.bbox_min);
    return d[0] * d[1] + d[0] * d[2] + d[1] * d[2];
}

void update_bbox(std::vector<dyn_bvh_node_t> &nodes, dyn_bvh_node_t &node) {
    const dyn_bvh_node_t &ch0 = nodes[bvh_left_child(node)];
    const dyn_bvh_node_t &ch1 = nodes[bvh_right_child(node)];
    node.bbox_min = Min(ch0.bbox_min, ch1.bbox_min);
    node.bbox_max = Max(ch0.bbox_max, ch1.bbox_max);
}

// Separation axis from child centers; also swaps children so left sorts before right on it
void sort_children(std::vector<dyn_bvh_node_t> &nodes, dyn_bvh_node_t &node) {
    const uint32_t ch0 = bvh_left_child(node), ch1 = bvh_right_child(node);
    const Vec3f c_left = (nodes[ch0].bbox_min + nodes[ch0].bbox_max) * Vec3f{0.5f};
    const Vec3f c_right = (nodes[ch1].bbox_min + nodes[ch1].bbox_max) * Vec3f{0.5f};
    const Vec3f dist = Abs(c_left - c_right);

    uint32_t axis = 2;
    if (dist[0] > dist[1] && dist[0] > dist[2]) {
        axis = 0;
    } else if (dist[1] > dist[0] && dist[1] > dist[2]) {
        axis = 1;
    }

    if (c_left[axis] > c_right[axis]) {
        bvh_set_internal(node, ch1, ch0, axis);
    } else {
        node.axis_or_right = (node.axis_or_right & ~BVH_AXIS_MASK) | axis;
    }
}

uint32_t split_sep_axis(const Vec3f left_bounds[2], const Vec3f right_bounds[2]) {
    const Vec3f c_left = (left_bounds[0] + left_bounds[1]) * Vec3f{0.5f};
    const Vec3f c_right = (right_bounds[0] + right_bounds[1]) * Vec3f{0.5f};
    const Vec3f dist = Abs(c_left - c_right);

    if (dist[0] > dist[1] && dist[0] > dist[2]) {
        return 0;
    }
    if (dist[1] > dist[0] && dist[1] > dist[2]) {
        return 1;
    }
    return 2;
}

bool candidate_cost_greater(const bvh_insert_candidate_t &c1, const bvh_insert_candidate_t &c2) {
    return c1.direct_cost > c2.direct_cost;
}

void link_child(std::vector<dyn_bvh_node_t> &nodes, const uint32_t parent, const bool is_right,
                const uint32_t child_index) {
    dyn_bvh_node_t &node = nodes[parent];
    if (is_right) {
        node.axis_or_right = (node.axis_or_right & BVH_AXIS_MASK) | (child_index << 2);
    } else {
        node.leaf_or_left = child_index << 1;
    }
}

void unlink_child(std::vector<dyn_bvh_node_t> &nodes, const uint32_t parent, const uint32_t old_child,
                  const uint32_t new_child) {
    dyn_bvh_node_t &node = nodes[parent];
    if (bvh_left_child(node) == old_child) {
        node.leaf_or_left = new_child << 1;
    } else {
        node.axis_or_right = (node.axis_or_right & BVH_AXIS_MASK) | (new_child << 2);
    }
}
} // namespace

void DynBVH::Clear() {
    nodes_.clear();
    free_nodes_.clear();
    candidates_.clear();
    root_ = BVH_NO_NODE;
    insertions_since_build_ = 0;
    bnb_expansions_ = 0;
    rotations_applied_ = 0;
}

uint32_t DynBVH::alloc_node() {
    if (!free_nodes_.empty()) {
        const uint32_t index = free_nodes_.back();
        free_nodes_.pop_back();
        return index;
    }
    nodes_.emplace_back();
    return uint32_t(nodes_.size() - 1);
}

void DynBVH::apply_margin(const Vec3f &bbox_min, const Vec3f &bbox_max, Vec3f &padded_min, Vec3f &padded_max) const {
    const Vec3f d = (bbox_max - bbox_min) * Vec3f{settings_.bounds_margin};
    padded_min = bbox_min - d;
    padded_max = bbox_max + d;
}

void DynBVH::Build(Span<const dyn_bvh_prim_t> prims, const dyn_bvh_settings_t &settings) {
    Clear();
    settings_ = settings;

    if (prims.empty()) {
        return;
    }

    std::vector<prim_t> padded;
    padded.reserve(prims.size());
    for (const dyn_bvh_prim_t &p : prims) {
        Vec3f pmin, pmax;
        apply_margin(p.bbox_min, p.bbox_max, pmin, pmax);
        padded.push_back({0, 0, 0, pmin, pmax});
    }

    struct collection_t {
        uint32_t parent_index = BVH_NO_NODE;
        bool is_right_child = false;
        std::vector<uint32_t> indices;
        Bounds bounds;
    };

    std::deque<collection_t> work;
    {
        collection_t root_coll;
        root_coll.indices.resize(prims.size());
        for (size_t i = 0; i < size_t(prims.size()); i++) {
            root_coll.indices[i] = uint32_t(i);
            root_coll.bounds.Expand(padded[i].bbox_min);
            root_coll.bounds.Expand(padded[i].bbox_max);
        }
        work.push_back(std::move(root_coll));
    }

    while (!work.empty()) {
        collection_t coll = std::move(work.front());
        work.pop_front();

        const split_data_t split =
            SplitPrimitives_SAH(padded.data(), Span<const uint32_t>(coll.indices.data(), coll.indices.size()),
                                coll.bounds.mins, coll.bounds.maxs, settings_.split);

        const uint32_t my_index = uint32_t(nodes_.size());
        nodes_.emplace_back();
        dyn_bvh_node_t &node = nodes_[my_index];
        node.parent = coll.parent_index;
        if (coll.parent_index == BVH_NO_NODE) {
            root_ = my_index;
        } else {
            link_child(nodes_, coll.parent_index, coll.is_right_child, my_index);
        }

        if (split.right_indices.empty()) {
            assert(split.left_indices.size() <= size_t(settings_.split.min_primitives_in_leaf));
            node.bbox_min = split.left_bounds[0];
            node.bbox_max = split.left_bounds[1];
            bvh_set_leaf(node, prims[split.left_indices[0]].prim_id, uint32_t(split.left_indices.size()));
        } else {
            node.bbox_min = Min(split.left_bounds[0], split.right_bounds[0]);
            node.bbox_max = Max(split.left_bounds[1], split.right_bounds[1]);
            bvh_set_internal(node, 0, 0, split_sep_axis(split.left_bounds, split.right_bounds));

            collection_t left_coll;
            left_coll.parent_index = my_index;
            left_coll.is_right_child = false;
            left_coll.indices = std::move(split.left_indices);
            left_coll.bounds.mins = split.left_bounds[0];
            left_coll.bounds.maxs = split.left_bounds[1];

            collection_t right_coll;
            right_coll.parent_index = my_index;
            right_coll.is_right_child = true;
            right_coll.indices = std::move(split.right_indices);
            right_coll.bounds.mins = split.right_bounds[0];
            right_coll.bounds.maxs = split.right_bounds[1];

            work.push_back(std::move(left_coll));
            work.push_back(std::move(right_coll));
        }
    }
}

bool DynBVH::LeafFits(const uint32_t leaf_index, const Vec3f &bbox_min, const Vec3f &bbox_max) const {
    const dyn_bvh_node_t &leaf = nodes_[leaf_index];
    return bbox_min[0] >= leaf.bbox_min[0] && bbox_min[1] >= leaf.bbox_min[1] && bbox_min[2] >= leaf.bbox_min[2] &&
           bbox_max[0] <= leaf.bbox_max[0] && bbox_max[1] <= leaf.bbox_max[1] && bbox_max[2] <= leaf.bbox_max[2];
}

void DynBVH::RefitFromLeaf(const uint32_t leaf_index, const Vec3f &bbox_min, const Vec3f &bbox_max) {
    dyn_bvh_node_t &leaf = nodes_[leaf_index];
    apply_margin(bbox_min, bbox_max, leaf.bbox_min, leaf.bbox_max);

    uint32_t up_parent = leaf.parent;
    while (up_parent != BVH_NO_NODE) {
        dyn_bvh_node_t &node = nodes_[up_parent];
        const dyn_bvh_node_t &ch0 = nodes_[bvh_left_child(node)];
        const dyn_bvh_node_t &ch1 = nodes_[bvh_right_child(node)];
        node.bbox_min = Min(ch0.bbox_min, ch1.bbox_min);
        node.bbox_max = Max(ch0.bbox_max, ch1.bbox_max);
        up_parent = node.parent;
    }
}

void DynBVH::RemoveLeaf(const uint32_t leaf_index) {
    dyn_bvh_node_t &leaf = nodes_[leaf_index];

    if (leaf.parent != BVH_NO_NODE) {
        const uint32_t parent_index = leaf.parent;
        const uint32_t other_child = (bvh_left_child(nodes_[parent_index]) == leaf_index)
                                         ? bvh_right_child(nodes_[parent_index])
                                         : bvh_left_child(nodes_[parent_index]);
        uint32_t up_parent = nodes_[parent_index].parent;

        if (up_parent != BVH_NO_NODE) {
            unlink_child(nodes_, up_parent, parent_index, other_child);
            nodes_[other_child].parent = up_parent;

            while (up_parent != BVH_NO_NODE) {
                dyn_bvh_node_t &node = nodes_[up_parent];
                update_bbox(nodes_, node);
                sort_children(nodes_, node);
                up_parent = node.parent;
            }
        } else {
            nodes_[other_child].parent = BVH_NO_NODE;
            root_ = other_child;
        }

        free_nodes_.push_back(parent_index);
    } else {
        // The removed node was the root leaf: the tree becomes empty
        root_ = BVH_NO_NODE;
    }

    free_nodes_.push_back(leaf_index);
}

uint32_t DynBVH::InsertPrim(const Vec3f &bbox_min, const Vec3f &bbox_max, const uint32_t prim_id) {
    Vec3f pmin, pmax;
    apply_margin(bbox_min, bbox_max, pmin, pmax);
    insertions_since_build_++;

    const uint32_t leaf_index = alloc_node();
    {
        dyn_bvh_node_t &leaf = nodes_[leaf_index];
        leaf.bbox_min = pmin;
        leaf.bbox_max = pmax;
        bvh_set_leaf(leaf, prim_id, 1);
        leaf.parent = BVH_NO_NODE;
    }

    if (root_ == BVH_NO_NODE) {
        root_ = leaf_index;
        return leaf_index;
    }

    dyn_bvh_node_t new_leaf = nodes_[leaf_index];

    uint32_t best_candidate = root_;
    float best_cost = surface_area_of_union(new_leaf, nodes_[best_candidate]);

    candidates_.clear();
    if (candidates_.capacity() < nodes_.size()) {
        candidates_.reserve(nodes_.size());
    }
    candidates_.push_back({best_candidate, best_cost});

    while (!candidates_.empty()) {
        std::pop_heap(candidates_.begin(), candidates_.end(), candidate_cost_greater);
        const bvh_insert_candidate_t c = candidates_.back();
        candidates_.pop_back();

        float inherited_cost = 0.0f;
        uint32_t parent = nodes_[c.node_index].parent;
        while (parent != BVH_NO_NODE) {
            inherited_cost += surface_area_of_union(nodes_[parent], new_leaf) - surface_area(nodes_[parent]);
            parent = nodes_[parent].parent;
        }

        const float total_cost = c.direct_cost + inherited_cost;
        if (total_cost < best_cost) {
            best_candidate = c.node_index;
            best_cost = total_cost;
        }

        if (!bvh_is_leaf(nodes_[c.node_index])) {
            bnb_expansions_++;
            const float lower_cost_bound = surface_area(new_leaf) + inherited_cost +
                                           surface_area_of_union(nodes_[c.node_index], new_leaf) -
                                           surface_area(nodes_[c.node_index]);

            if (lower_cost_bound < best_cost) {
                const uint32_t ch0 = bvh_left_child(nodes_[c.node_index]);
                const uint32_t ch1 = bvh_right_child(nodes_[c.node_index]);
                candidates_.push_back({ch0, surface_area_of_union(nodes_[ch0], new_leaf)});
                std::push_heap(candidates_.begin(), candidates_.end(), candidate_cost_greater);
                candidates_.push_back({ch1, surface_area_of_union(nodes_[ch1], new_leaf)});
                std::push_heap(candidates_.begin(), candidates_.end(), candidate_cost_greater);
            }
        }
    }

    const uint32_t old_parent = nodes_[best_candidate].parent;
    const uint32_t new_parent = alloc_node();

    bvh_set_internal(nodes_[new_parent], best_candidate, leaf_index, 0);
    nodes_[new_parent].parent = old_parent;
    nodes_[best_candidate].parent = new_parent;
    nodes_[leaf_index].parent = new_parent;

    if (old_parent != BVH_NO_NODE) {
        unlink_child(nodes_, old_parent, best_candidate, new_parent);
    } else {
        root_ = new_parent;
    }

    uint32_t parent = new_parent;
    while (parent != BVH_NO_NODE) {
        dyn_bvh_node_t &par_node = nodes_[parent];
        update_bbox(nodes_, par_node);

        if (!bvh_is_leaf(nodes_[bvh_right_child(par_node)])) {
            const uint32_t r_index = bvh_right_child(par_node);
            const float cost_before = surface_area(nodes_[r_index]);
            const float rotation_costs[2] = {
                surface_area_of_union(nodes_[bvh_left_child(par_node)], nodes_[bvh_right_child(nodes_[r_index])]),
                surface_area_of_union(nodes_[bvh_left_child(par_node)], nodes_[bvh_left_child(nodes_[r_index])])};

            const int best_rot = rotation_costs[0] < rotation_costs[1] ? 0 : 1;
            if (rotation_costs[best_rot] < cost_before) {
                nodes_[bvh_left_child(par_node)].parent = r_index;

                if (best_rot == 0) {
                    nodes_[bvh_left_child(nodes_[r_index])].parent = parent;

                    const uint32_t temp = bvh_left_child(par_node);
                    bvh_set_internal(par_node, bvh_left_child(nodes_[r_index]), r_index, bvh_sep_axis(par_node));
                    bvh_set_left_child(nodes_[r_index], temp);
                } else {
                    nodes_[bvh_right_child(nodes_[r_index])].parent = parent;

                    const uint32_t temp = bvh_left_child(par_node);
                    bvh_set_internal(par_node, bvh_right_child(nodes_[r_index]), r_index, bvh_sep_axis(par_node));
                    bvh_set_right_child(nodes_[r_index], temp);
                }

                update_bbox(nodes_, nodes_[r_index]);
                update_bbox(nodes_, par_node);

                sort_children(nodes_, nodes_[r_index]);
                rotations_applied_++;
            }
        }

        if (!bvh_is_leaf(nodes_[bvh_left_child(par_node)])) {
            const uint32_t l_index = bvh_left_child(par_node);
            const float cost_before = surface_area(nodes_[l_index]);
            const float rotation_costs[2] = {
                surface_area_of_union(nodes_[bvh_right_child(par_node)], nodes_[bvh_right_child(nodes_[l_index])]),
                surface_area_of_union(nodes_[bvh_right_child(par_node)], nodes_[bvh_left_child(nodes_[l_index])])};

            const int best_rot = rotation_costs[0] < rotation_costs[1] ? 0 : 1;
            if (rotation_costs[best_rot] < cost_before) {
                nodes_[bvh_right_child(par_node)].parent = l_index;

                if (best_rot == 0) {
                    nodes_[bvh_left_child(nodes_[l_index])].parent = parent;

                    const uint32_t temp = bvh_right_child(par_node);
                    bvh_set_internal(par_node, l_index, bvh_left_child(nodes_[l_index]), bvh_sep_axis(par_node));
                    bvh_set_left_child(nodes_[l_index], temp);
                } else {
                    nodes_[bvh_right_child(nodes_[l_index])].parent = parent;

                    const uint32_t temp = bvh_right_child(par_node);
                    bvh_set_internal(par_node, l_index, bvh_right_child(nodes_[l_index]), bvh_sep_axis(par_node));
                    bvh_set_right_child(nodes_[l_index], temp);
                }

                update_bbox(nodes_, nodes_[l_index]);
                update_bbox(nodes_, par_node);

                sort_children(nodes_, nodes_[l_index]);
                rotations_applied_++;
            }
        }

        sort_children(nodes_, par_node);
        parent = par_node.parent;
    }

    return leaf_index;
}

uint32_t DynBVH::max_depth() const {
    if (root_ == BVH_NO_NODE) {
        return 0;
    }

    struct dfs_item_t {
        uint32_t index, level;
    };

    std::vector<dfs_item_t> stack;
    stack.reserve(64);
    stack.push_back({root_, 1});

    uint32_t max_level = 1;
    while (!stack.empty()) {
        const dfs_item_t item = stack.back();
        stack.pop_back();
        max_level = std::max(max_level, item.level);

        const dyn_bvh_node_t &node = nodes_[item.index];
        if (!bvh_is_leaf(node)) {
            stack.push_back({bvh_left_child(node), item.level + 1});
            stack.push_back({bvh_right_child(node), item.level + 1});
        }
    }

    return max_level;
}

bool DynBVH::ValidateBVH() const {
    if (root_ == BVH_NO_NODE) {
        // Empty tree: every slot must sit in the free list (stale nodes are allowed)
        std::vector<uint8_t> in_free(nodes_.size(), 0);
        for (const uint32_t f : free_nodes_) {
            if (f >= nodes_.size() || in_free[f]++ != 0) {
                return false;
            }
        }
        return free_nodes_.size() == nodes_.size();
    }
    if (root_ >= nodes_.size()) {
        return false;
    }

    std::vector<uint8_t> reached(nodes_.size(), 0);
    std::vector<uint32_t> prim_ids;
    std::vector<uint32_t> stack;
    stack.push_back(root_);
    size_t reached_count = 0;

    while (!stack.empty()) {
        const uint32_t i = stack.back();
        stack.pop_back();
        if (i >= nodes_.size() || reached[i] != 0) {
            return false;
        }
        reached[i] = 1;
        reached_count++;

        const dyn_bvh_node_t &node = nodes_[i];
        if (node.parent == BVH_NO_NODE) {
            if (i != root_) {
                return false;
            }
        } else if (node.parent >= nodes_.size()) {
            return false;
        }

        if (bvh_is_leaf(node)) {
            prim_ids.push_back(bvh_prim_id(node));
            if (bvh_prim_count(node) == 0) {
                return false;
            }
        } else {
            const uint32_t ch0 = bvh_left_child(node), ch1 = bvh_right_child(node);
            if (ch0 >= nodes_.size() || ch1 >= nodes_.size() || ch0 == ch1) {
                return false;
            }
            if (nodes_[ch0].parent != i || nodes_[ch1].parent != i) {
                return false;
            }
            const Vec3f umin = Min(nodes_[ch0].bbox_min, nodes_[ch1].bbox_min);
            const Vec3f umax = Max(nodes_[ch0].bbox_max, nodes_[ch1].bbox_max);
            for (int a = 0; a < 3; a++) {
                if (node.bbox_min[a] != umin[a] || node.bbox_max[a] != umax[a]) {
                    return false;
                }
            }
            stack.push_back(ch0);
            stack.push_back(ch1);
        }
    }

    std::vector<uint8_t> in_free(nodes_.size(), 0);
    for (const uint32_t f : free_nodes_) {
        if (f >= nodes_.size() || reached[f] != 0 || in_free[f]++ != 0) {
            return false;
        }
    }
    if (reached_count + free_nodes_.size() != nodes_.size()) {
        return false;
    }

    std::sort(prim_ids.begin(), prim_ids.end());
    for (size_t i = 1; i < prim_ids.size(); i++) {
        if (prim_ids[i] == prim_ids[i - 1]) {
            return false;
        }
    }
    return true;
}
} // namespace Phy
