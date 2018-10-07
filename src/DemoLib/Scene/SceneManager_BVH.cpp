#include "SceneManager.h"

#include <deque>

#include "../Utils/BVHSplit.h"

void SceneManager::RebuildBVH() {
    std::vector<prim_t> primitives;
    primitives.reserve(objects_.size());

    for (const auto &obj : objects_) {
        if (obj.flags & HasTransform) {
            const auto &tr = obj.tr;
            primitives.push_back({ tr->bbox_min_ws, tr->bbox_max_ws });
        }
    }

    nodes_.clear();
    obj_indices_.clear();

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

    size_t num_nodes = nodes_.size();
    auto root_node_index = (uint32_t)num_nodes;

    for (size_t i = 0; i < primitives.size(); i++) {
        prim_lists.back().indices.push_back((uint32_t)i);
        prim_lists.back().min = Min(prim_lists.back().min, primitives[i].bbox_min);
        prim_lists.back().max = Max(prim_lists.back().max, primitives[i].bbox_max);
    }

    Ren::Vec3f root_min = prim_lists.back().min,
               root_max = prim_lists.back().max;

    while (!prim_lists.empty()) {
        auto split_data = SplitPrimitives_SAH(&primitives[0], prim_lists.back().indices, prim_lists.back().min, prim_lists.back().max, root_min, root_max);
        prim_lists.pop_back();

        uint32_t leaf_index = (uint32_t)nodes_.size(),
                 parent_index = 0xffffffff;

        if (leaf_index) {
            // skip bound checks in debug mode
            const bvh_node_t *_out_nodes = &nodes_[0];
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

            nodes_.push_back({ (uint32_t)obj_indices_.size(), (uint32_t)split_data.left_indices.size(), 0, 0, parent_index, 0,
                             { { bbox_min[0], bbox_min[1], bbox_min[2] }, { bbox_max[0], bbox_max[1], bbox_max[2] } }
            });
            obj_indices_.insert(obj_indices_.end(), split_data.left_indices.begin(), split_data.left_indices.end());
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

            nodes_.push_back({ 0, 0, index + 1, index + 2, parent_index, space_axis,
                             { { bbox_min[0], bbox_min[1], bbox_min[2] }, { bbox_max[0], bbox_max[1], bbox_max[2] } }
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