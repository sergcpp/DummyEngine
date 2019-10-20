#include "SceneManager.h"

#include <deque>

#include <Sys/BinaryTree.h>
#include <Sys/MonoAlloc.h>

#include "../Utils/BVHSplit.h"

namespace SceneManagerInternal {
    const float BoundsMargin = 0.2f;

    float surface_area(const bvh_node_t &n) {
        const Ren::Vec3f d = n.bbox_max - n.bbox_min;
        return d[0] * d[1] + d[0] * d[2] + d[1] * d[2];
    }

    float surface_area_of_union(const bvh_node_t &n1, const bvh_node_t &n2) {
        const Ren::Vec3f d = Ren::Max(n1.bbox_max, n2.bbox_max) - Ren::Min(n1.bbox_min, n2.bbox_max);
        return d[0] * d[1] + d[0] * d[2] + d[1] * d[2];
    }

    struct insert_candidate_t {
        uint32_t node_index;
        float direct_cost;
    };

    bool insert_candidate_compare(insert_candidate_t c1, insert_candidate_t c2) {
        return c1.direct_cost > c2.direct_cost;
    }

    void sort_children(const bvh_node_t *nodes, bvh_node_t &node) {
        const uint32_t
            ch0 = node.left_child,
            ch1 = node.right_child;

        uint32_t space_axis = 0;
        const Ren::Vec3f
            c_left = (nodes[ch0].bbox_min + nodes[ch0].bbox_max) / 2,
            c_right = (nodes[ch1].bbox_min + nodes[ch1].bbox_max) / 2;

        const Ren::Vec3f dist = Abs(c_left - c_right);

        if (dist[0] > dist[1] && dist[0] > dist[2]) {
            space_axis = 0;
        } else if (dist[1] > dist[0] && dist[1] > dist[2]) {
            space_axis = 1;
        } else {
            space_axis = 2;
        }

        node.space_axis = space_axis;

        if (c_left[space_axis] > c_right[space_axis]) {
            std::swap(node.left_child, node.right_child);
        }
    }

    void update_bbox(const bvh_node_t *nodes, bvh_node_t &node) {
        node.bbox_min = Ren::Min(nodes[node.left_child].bbox_min, nodes[node.right_child].bbox_min);
        node.bbox_max = Ren::Max(nodes[node.left_child].bbox_max, nodes[node.right_child].bbox_max);
    }
}

void SceneManager::RebuildBVH() {
    using namespace SceneManagerInternal;

    auto *transforms = (Transform *)scene_data_.comp_store[CompTransform]->Get(0);
    assert(scene_data_.comp_store[CompTransform]->IsSequential());

    std::vector<prim_t> primitives;
    primitives.reserve(scene_data_.objects.size());

    for (const SceneObject &obj : scene_data_.objects) {
        if (obj.comp_mask & CompTransformBit) {
            const Transform &tr = transforms[obj.components[CompTransform]];
            const Ren::Vec3f d = tr.bbox_max_ws - tr.bbox_min_ws;
            primitives.push_back({ tr.bbox_min_ws - BoundsMargin * d, tr.bbox_max_ws + BoundsMargin * d });
        }
    }

    scene_data_.nodes.clear();

    struct prims_coll_t {
        std::vector<uint32_t> indices;
        Ren::Vec3f min = Ren::Vec3f{ std::numeric_limits<float>::max() }, max = Ren::Vec3f{ std::numeric_limits<float>::lowest() };
        prims_coll_t() {}
        prims_coll_t(std::vector<uint32_t> &&_indices, const Ren::Vec3f &_min, const Ren::Vec3f &_max)
            : indices(std::move(_indices)), min(_min), max(_max) {
        }
    };

    std::deque<prims_coll_t> prim_lists;
    prim_lists.emplace_back();

    size_t nodes_count = scene_data_.nodes.size();
    auto root_node_index = (uint32_t)nodes_count;

    for (size_t i = 0; i < primitives.size(); i++) {
        prim_lists.back().indices.push_back((uint32_t)i);
        prim_lists.back().min = Min(prim_lists.back().min, primitives[i].bbox_min);
        prim_lists.back().max = Max(prim_lists.back().max, primitives[i].bbox_max);
    }

    const Ren::Vec3f
        root_min = prim_lists.back().min,
        root_max = prim_lists.back().max;

    split_settings_t s;
    s.oversplit_threshold = std::numeric_limits<float>::max();
    s.node_traversal_cost = 0.0f;

    while (!prim_lists.empty()) {
        split_data_t split_data = SplitPrimitives_SAH(&primitives[0], prim_lists.back().indices.data(), (uint32_t)prim_lists.back().indices.size(),
                                                      prim_lists.back().min, prim_lists.back().max, root_min, root_max, s);
        prim_lists.pop_back();

        uint32_t leaf_index = (uint32_t)scene_data_.nodes.size(),
                 parent_index = 0xffffffff;

        if (leaf_index) {
            // skip bound checks in debug mode
            const bvh_node_t *_out_nodes = &scene_data_.nodes[0];
            for (uint32_t i = leaf_index - 1; i >= root_node_index; i--) {
                if (_out_nodes[i].left_child == leaf_index || _out_nodes[i].right_child == leaf_index) {
                    parent_index = (uint32_t)i;
                    break;
                }
            }
        }

        if (split_data.right_indices.empty()) {
            const Ren::Vec3f
                bbox_min = split_data.left_bounds[0],
                bbox_max = split_data.left_bounds[1];

            const uint32_t new_node_index = (uint32_t)scene_data_.nodes.size();

            for (const uint32_t i : split_data.left_indices) {
                Transform &tr = transforms[scene_data_.objects[i].components[CompTransform]];
                tr.node_index = new_node_index;
            }

            assert(split_data.left_indices.size() == 1 && "Wrong split!");

            scene_data_.nodes.push_back({ split_data.left_indices[0], (uint32_t)split_data.left_indices.size(), 0, 0,
                { bbox_min[0], bbox_min[1], bbox_min[2] }, parent_index,
                { bbox_max[0], bbox_max[1], bbox_max[2] }, 0
            });
        } else {
            auto index = (uint32_t)nodes_count;

            const Ren::Vec3f
                c_left = (split_data.left_bounds[0] + split_data.left_bounds[1]) / 2.0f,
                c_right = (split_data.right_bounds[0] + split_data.right_bounds[1]) / 2.0f;

            const Ren::Vec3f dist = Abs(c_left - c_right);

            const uint32_t space_axis =
                (dist[0] > dist[1] && dist[0] > dist[2]) ? 0 :
                ((dist[1] > dist[0] && dist[1] > dist[2]) ? 1 : 2);

            const Ren::Vec3f
                bbox_min = Min(split_data.left_bounds[0], split_data.right_bounds[0]),
                bbox_max = Max(split_data.left_bounds[1], split_data.right_bounds[1]);

            scene_data_.nodes.push_back({ 0, 0, index + 1, index + 2,
                { bbox_min[0], bbox_min[1], bbox_min[2] }, parent_index,
                { bbox_max[0], bbox_max[1], bbox_max[2] }, space_axis,
            });

            prim_lists.emplace_front(std::move(split_data.left_indices), split_data.left_bounds[0], split_data.left_bounds[1]);
            prim_lists.emplace_front(std::move(split_data.right_indices), split_data.right_bounds[0], split_data.right_bounds[1]);

            nodes_count += 2;
        }
    }

    /*{   // reorder objects
        uint32_t j, k;
        for (uint32_t i = 0; i < (uint32_t)objects_.size(); i++) {
            while (i != (j = obj_indices[i])) {
                k = obj_indices[j];
                std::swap(objects_[j], objects_[k]);
                std::swap(obj_indices[i], obj_indices[j]);
            }
        }
    }*/
}

void SceneManager::RemoveNode(uint32_t node_index) {
    using namespace SceneManagerInternal;

    bvh_node_t *nodes = scene_data_.nodes.data();
    bvh_node_t &node = nodes[node_index];

    if (node.parent != 0xffffffff) {
        bvh_node_t &parent = nodes[node.parent];

        uint32_t other_child = (parent.left_child == node_index) ? parent.right_child : parent.left_child;
        uint32_t up_parent = parent.parent;

        if (up_parent != 0xffffffff) {
            // upper parent if not a root
            if (nodes[up_parent].left_child == node.parent) {
                nodes[up_parent].left_child = other_child;
            } else {
                nodes[up_parent].right_child = other_child;
            }
            nodes[other_child].parent = up_parent;

            // update hierarchy boxes
            while (up_parent != 0xffffffff) {
                uint32_t ch0 = nodes[up_parent].left_child, ch1 = nodes[up_parent].right_child;

                nodes[up_parent].bbox_min = Ren::Min(nodes[ch0].bbox_min, nodes[ch1].bbox_min);
                nodes[up_parent].bbox_max = Ren::Max(nodes[ch0].bbox_max, nodes[ch1].bbox_max);

                sort_children(nodes, nodes[up_parent]);

                up_parent = nodes[up_parent].parent;
            }
        } else {
            // upper parent is root
            nodes[other_child].parent = 0xffffffff;
            scene_data_.root_node = other_child;
        }

        scene_data_.free_nodes.push_back(node.parent);
    }

    scene_data_.free_nodes.push_back(node_index);
}

void SceneManager::UpdateObjects() {
    using namespace SceneManagerInternal;

    auto *transforms = (Transform *)scene_data_.comp_store[CompTransform]->Get(0);
    assert(scene_data_.comp_store[CompTransform]->IsSequential());

    scene_data_.update_counter++;

    bvh_node_t *nodes = scene_data_.nodes.data();

    for (const uint32_t obj_index : last_changed_objects_) {
        SceneObject &obj = scene_data_.objects[obj_index];
        obj.last_change_mask = 0;
    }

    // Remove nodes with associated moved objects (they will be reinserted)
    for (const uint32_t obj_index : changed_objects_) {
        SceneObject &obj = scene_data_.objects[obj_index];
        obj.last_change_mask = obj.change_mask;

        if (obj.change_mask & CompTransformBit) {
            Transform &tr = transforms[obj.components[CompTransform]];
            tr.UpdateBBox();
            if (tr.node_index != 0xffffffff) {
                const bvh_node_t &node = nodes[tr.node_index];

                bool is_fully_inside = tr.bbox_min_ws[0] >= node.bbox_min[0] &&
                                       tr.bbox_min_ws[1] >= node.bbox_min[1] &&
                                       tr.bbox_min_ws[2] >= node.bbox_min[2] &&
                                       tr.bbox_max_ws[0] <= node.bbox_max[0] &&
                                       tr.bbox_max_ws[1] <= node.bbox_max[1] &&
                                       tr.bbox_max_ws[2] <= node.bbox_max[2];

                if (is_fully_inside) {
                    // Update is not needed (object is inside of node bounds)
                    obj.change_mask ^= CompTransformBit;
                } else {
                    // Object is out of node bounds, remove node and re-insert it later
                    RemoveNode(tr.node_index);
                    tr.node_index = 0xffffffff;
                }
            }
        }
    }

    uint32_t *free_nodes = scene_data_.free_nodes.data();
    uint32_t free_nodes_pos = 0;

    // temporary buffer used to optimize memory allocation
    temp_buf.resize(scene_data_.nodes.size() * 24);

    for (const uint32_t obj_index : changed_objects_) {
        SceneObject &obj = scene_data_.objects[obj_index];

        if (obj.change_mask & CompTransformBit) {
            Transform &tr = transforms[obj.components[CompTransform]];
            tr.node_index = free_nodes[free_nodes_pos++];

            bvh_node_t &new_node = nodes[tr.node_index];

            const Ren::Vec3f d = tr.bbox_max_ws - tr.bbox_min_ws;
            new_node.bbox_min = tr.bbox_min_ws - BoundsMargin * d;
            new_node.bbox_max = tr.bbox_max_ws + BoundsMargin * d;
            new_node.prim_index = obj_index;
            new_node.prim_count = 1;

            Sys::MonoAlloc<insert_candidate_t> m_alloc(temp_buf.data(), temp_buf.size());
            Sys::BinaryTree<insert_candidate_t, decltype(&insert_candidate_compare), Sys::MonoAlloc<insert_candidate_t>> candidates(insert_candidate_compare, m_alloc);

            float node_cost = surface_area(new_node);

            uint32_t best_candidate = scene_data_.root_node;
            float best_cost = surface_area_of_union(new_node, nodes[best_candidate]);
            candidates.push({ best_candidate, best_cost });

            while (!candidates.empty()) {
                insert_candidate_t c;
                candidates.extract_top(c);

                float inherited_cost = 0.0f;
                uint32_t parent = nodes[c.node_index].parent;

                while (parent != 0xffffffff) {
                    // add change in surface area
                    inherited_cost += surface_area_of_union(nodes[parent], new_node) - surface_area(nodes[parent]);
                    parent = nodes[parent].parent;
                }

                float total_cost = c.direct_cost + inherited_cost;
                if (total_cost < best_cost) {
                    best_candidate = c.node_index;
                    best_cost = total_cost;
                }

                // consider children next
                if (!nodes[c.node_index].prim_count) {
                    float candidate_cost = surface_area(nodes[c.node_index]);
                    float lower_cost_bound = node_cost + inherited_cost + surface_area_of_union(nodes[c.node_index], new_node) - candidate_cost;

                    if (lower_cost_bound < best_cost) {
                        uint32_t ch0 = nodes[c.node_index].left_child, ch1 = nodes[c.node_index].right_child;
                        candidates.push({ ch0, surface_area_of_union(nodes[ch0], new_node) });
                        candidates.push({ ch1, surface_area_of_union(nodes[ch1], new_node) });
                    }
                }
            }

            uint32_t old_parent = nodes[best_candidate].parent;
            uint32_t new_parent = free_nodes[free_nodes_pos++];

            nodes[new_parent].prim_count = 0;
            nodes[new_parent].parent = old_parent;

            if (old_parent != 0xffffffff) {
                // sibling candidate is not root
                if (nodes[old_parent].left_child == best_candidate) {
                    nodes[old_parent].left_child = new_parent;
                } else {
                    nodes[old_parent].right_child = new_parent;
                }

                nodes[new_parent].left_child = best_candidate;
                nodes[new_parent].right_child = tr.node_index;
                nodes[best_candidate].parent = new_parent;
                nodes[tr.node_index].parent = new_parent;
            } else {
                // sibling candidate is root
                nodes[new_parent].left_child = best_candidate;
                nodes[new_parent].right_child = tr.node_index;
                nodes[new_parent].parent = 0xffffffff;
                nodes[best_candidate].parent = new_parent;
                nodes[tr.node_index].parent = new_parent;
                scene_data_.root_node = new_parent;
            }

#define left_child_of(x) nodes[x.left_child]
#define right_child_of(x) nodes[x.right_child]

            // update hierarchy boxes
            uint32_t parent = new_parent;
            while (parent != 0xffffffff) {
                bvh_node_t &par_node = nodes[parent];

                par_node.bbox_min = Ren::Min(left_child_of(par_node).bbox_min, right_child_of(par_node).bbox_min);
                par_node.bbox_max = Ren::Max(left_child_of(par_node).bbox_max, right_child_of(par_node).bbox_max);

                // rotate left_child with children of right_child
                if (!right_child_of(par_node).prim_count) {
                    float cost_before = surface_area(nodes[par_node.right_child]);

                    float rotation_costs[2] = { surface_area_of_union(left_child_of(par_node), right_child_of(right_child_of(par_node))),
                                                surface_area_of_union(left_child_of(par_node), left_child_of(right_child_of(par_node))) };

                    int best_rot = rotation_costs[0] < rotation_costs[1] ? 0 : 1;
                    if (rotation_costs[best_rot] < cost_before) {
                        left_child_of(par_node).parent = par_node.right_child;

                        if (best_rot == 0) {
                            left_child_of(right_child_of(par_node)).parent = parent;

                            std::swap(par_node.left_child, right_child_of(par_node).left_child);
                        } else {
                            right_child_of(right_child_of(par_node)).parent = parent;

                            std::swap(par_node.left_child, right_child_of(par_node).right_child);
                        }

                        update_bbox(nodes, nodes[par_node.right_child]);
                        update_bbox(nodes, nodes[parent]);

                        sort_children(nodes, nodes[par_node.right_child]);
                        // parent children will be sorted below
                    }
                }

                // rotate right_child with children of left_child
                if (!left_child_of(par_node).prim_count) {
                    const float cost_before = surface_area(left_child_of(par_node));

                    const float rotation_costs[2] = {
                        surface_area_of_union(right_child_of(par_node), right_child_of(left_child_of(par_node))),
                        surface_area_of_union(right_child_of(par_node), left_child_of(left_child_of(par_node)))
                    };

                    const int best_rot = rotation_costs[0] < rotation_costs[1] ? 0 : 1;
                    if (rotation_costs[best_rot] < cost_before) {
                        right_child_of(par_node).parent = par_node.left_child;

                        if (best_rot == 0) {
                            left_child_of(left_child_of(par_node)).parent = parent;

                            std::swap(par_node.right_child, left_child_of(par_node).left_child);
                        } else {
                            right_child_of(left_child_of(par_node)).parent = parent;

                            std::swap(par_node.right_child, left_child_of(par_node).right_child);
                        }

                        update_bbox(nodes, nodes[par_node.left_child]);
                        update_bbox(nodes, nodes[parent]);

                        sort_children(nodes, nodes[par_node.left_child]);
                        // parent children will be sorted below
                    }
                }

                sort_children(nodes, par_node);

                parent = par_node.parent;
            }

#undef left_child_of
#undef right_child_of

            obj.change_mask ^= CompTransformBit;
        }
    }

    scene_data_.free_nodes.erase(scene_data_.free_nodes.begin(), scene_data_.free_nodes.begin() + free_nodes_pos);
    last_changed_objects_ = std::move(changed_objects_);
}