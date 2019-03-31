#include "SceneManager.h"

#include <deque>

#include "../Utils/BVHSplit.h"

void SceneManager::RebuildBVH() {
    std::vector<prim_t> primitives;
    primitives.reserve(scene_data_.objects.size());

    for (const auto &obj : scene_data_.objects) {
        if (obj.comp_mask & HasTransform) {
            const auto &tr = obj.tr;
            primitives.push_back({ tr->bbox_min_ws, tr->bbox_max_ws });
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

    size_t num_nodes = scene_data_.nodes.size();
    auto root_node_index = (uint32_t)num_nodes;

    for (size_t i = 0; i < primitives.size(); i++) {
        prim_lists.back().indices.push_back((uint32_t)i);
        prim_lists.back().min = Min(prim_lists.back().min, primitives[i].bbox_min);
        prim_lists.back().max = Max(prim_lists.back().max, primitives[i].bbox_max);
    }

    Ren::Vec3f root_min = prim_lists.back().min,
               root_max = prim_lists.back().max;

    split_settings_t s;
    s.oversplit_threshold = std::numeric_limits<float>::max();
    s.node_traversal_cost = 0.0f;

    while (!prim_lists.empty()) {
        auto split_data = SplitPrimitives_SAH(&primitives[0], prim_lists.back().indices.data(), (uint32_t)prim_lists.back().indices.size(),
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
            Ren::Vec3f bbox_min = split_data.left_bounds[0],
                       bbox_max = split_data.left_bounds[1];

            uint32_t new_node_index = (uint32_t)scene_data_.nodes.size();

            for (const uint32_t i : split_data.left_indices) {
                scene_data_.objects[i].tr->node_index = new_node_index;
            }

            assert(split_data.left_indices.size() == 1 && "Wrong split!");

            scene_data_.nodes.push_back({ split_data.left_indices[0], (uint32_t)split_data.left_indices.size(), 0, 0,
                { bbox_min[0], bbox_min[1], bbox_min[2] }, parent_index,
                { bbox_max[0], bbox_max[1], bbox_max[2] }, 0
            });
        } else {
            auto index = (uint32_t)num_nodes;

            uint32_t space_axis = 0;
            Ren::Vec3f c_left = (split_data.left_bounds[0] + split_data.left_bounds[1]) / 2,
                       c_right = (split_data.right_bounds[1] + split_data.right_bounds[1]) / 2;

            Ren::Vec3f dist = Abs(c_left - c_right);

            if (dist[0] > dist[1] && dist[0] > dist[2]) {
                space_axis = 0;
            } else if (dist[1] > dist[0] && dist[1] > dist[2]) {
                space_axis = 1;
            } else {
                space_axis = 2;
            }

            Ren::Vec3f bbox_min = Min(split_data.left_bounds[0], split_data.right_bounds[0]),
                       bbox_max = Max(split_data.left_bounds[1], split_data.right_bounds[1]);

            scene_data_.nodes.push_back({ 0, 0, index + 1, index + 2,
                { bbox_min[0], bbox_min[1], bbox_min[2] }, parent_index,
                { bbox_max[0], bbox_max[1], bbox_max[2] }, space_axis,
            });

            prim_lists.emplace_front(std::move(split_data.left_indices), split_data.left_bounds[0], split_data.left_bounds[1]);
            prim_lists.emplace_front(std::move(split_data.right_indices), split_data.right_bounds[0], split_data.right_bounds[1]);

            num_nodes += 2;
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

void SceneManager::UpdateBVH() {
    RebuildBVH();
}

void SceneManager::UpdateObjects() {
    auto &nodes = scene_data_.nodes;

    for (const int i : changed_objects_) {
        auto &obj = scene_data_.objects[i];

        if (obj.change_mask & ChangePosition) {
            obj.tr->UpdateBBox();

            uint32_t node_index = obj.tr->node_index;
            auto &node = nodes[node_index];

            uint32_t other_child = 0xffffffff;

            if (node.parent != 0xffffffff) {
                auto &parent = nodes[node.parent];

                uint32_t up_parent = parent.parent;
                other_child = (parent.left_child == node_index) ? parent.right_child : parent.left_child;
                // replace parent with other node
                parent = nodes[other_child];
                parent.parent = up_parent;
                if (parent.prim_count) {
                    // relink object
                    scene_data_.objects[parent.prim_index].tr->node_index = node.parent;
                } else {
                    // relink node
                    nodes[parent.left_child].parent = node.parent;
                    nodes[parent.right_child].parent = node.parent;
                }
            }

            auto surface_area = [](const Ren::Vec3f &min, const Ren::Vec3f &max) {
                Ren::Vec3f d = max - min;
                return d[0] * d[1] + d[0] * d[2] + d[1] * d[2];
            };

            struct insert_candidate_t {
                uint32_t index;
                float direct_cost;
            };

            auto cmp = [](insert_candidate_t c1, insert_candidate_t c2) { return c1.direct_cost > c2.direct_cost; };
            std::priority_queue<insert_candidate_t, std::vector<insert_candidate_t>, decltype(cmp)> candidates(cmp);

            float node_cost = surface_area(node.bbox_min, node.bbox_max);

            uint32_t best_candidate = 0;
            float best_cost = surface_area(Ren::Min(node.bbox_min, nodes[best_candidate].bbox_min),
                                           Ren::Max(node.bbox_max, nodes[best_candidate].bbox_max));
            candidates.push({ 0, best_cost });

            while (!candidates.empty()) {
                const auto c = candidates.top();
                candidates.pop();

                float inherited_cost = 0.0f;
                uint32_t parent = nodes[c.index].parent;

                while (parent != 0xffffffff) {
                    // add change in surface area
                    inherited_cost += surface_area(Ren::Min(nodes[parent].bbox_min, node.bbox_min),
                                                   Ren::Max(nodes[parent].bbox_max, node.bbox_max)) -
                                      surface_area(nodes[parent].bbox_min, nodes[parent].bbox_max);

                    parent = nodes[parent].parent;
                }

                float total_cost = c.direct_cost + inherited_cost;
                if (total_cost < best_cost) {
                    best_candidate = c.index;
                    best_cost = total_cost;
                }

                // consider children next
                if (!nodes[c.index].prim_count) {
                    float candidate_cost = surface_area(nodes[c.index].bbox_min, nodes[c.index].bbox_max);
                    float lower_cost_bound = node_cost + inherited_cost +
                        surface_area(Ren::Min(nodes[c.index].bbox_min, node.bbox_min),
                                     Ren::Max(nodes[c.index].bbox_max, node.bbox_max)) - candidate_cost;

                    if (lower_cost_bound < best_cost) {
                        uint32_t ch0 = nodes[c.index].left_child;
                        uint32_t ch1 = nodes[c.index].right_child;
                        candidates.push({ ch0, surface_area(Ren::Min(node.bbox_min, nodes[ch0].bbox_min),
                                                            Ren::Max(node.bbox_max, nodes[ch0].bbox_max)) });
                        candidates.push({ ch1, surface_area(Ren::Min(node.bbox_min, nodes[ch1].bbox_min),
                                                            Ren::Max(node.bbox_max, nodes[ch1].bbox_max)) });
                    }
                }
            }

            uint32_t old_parent = nodes[best_candidate].parent;
            uint32_t new_parent = other_child;

            nodes[new_parent].parent = old_parent;
            nodes[new_parent].bbox_min = Ren::Min(node.bbox_min, nodes[best_candidate].bbox_min);
            nodes[new_parent].bbox_max = Ren::Max(node.bbox_max, nodes[best_candidate].bbox_max);

            if (old_parent != 0xffffffff) {
                // sibling candidate is not root
                if (nodes[old_parent].left_child == best_candidate) {
                    nodes[old_parent].left_child = new_parent;
                } else {
                    nodes[old_parent].right_child = new_parent;
                }

                nodes[new_parent].left_child = best_candidate;
                nodes[new_parent].right_child = node_index;
                nodes[best_candidate].parent = new_parent;
                nodes[node_index].parent = new_parent;
            } else {
                // sibling candidate is root
                nodes[new_parent].left_child = best_candidate;
                nodes[new_parent].right_child = node_index;
                nodes[best_candidate].parent = new_parent;
                nodes[node_index].parent = new_parent;
                scene_data_.root_node = new_parent;
            }

            // update hierarchy boxes
            uint32_t parent = nodes[node_index].parent;
            while (parent != 0xffffffff) {
                uint32_t ch0 = nodes[parent].left_child;
                uint32_t ch1 = nodes[parent].right_child;

                nodes[parent].bbox_min = Ren::Min(nodes[ch0].bbox_min, nodes[ch1].bbox_min);
                nodes[parent].bbox_max = Ren::Max(nodes[ch0].bbox_max, nodes[ch1].bbox_max);

                parent = nodes[parent].parent;
            }

            obj.change_mask ^= ChangePosition;
        }
    }

    UpdateBVH();

    changed_objects_.clear();
}