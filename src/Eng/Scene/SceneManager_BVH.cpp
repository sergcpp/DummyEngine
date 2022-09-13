#include "SceneManager.h"

#include <deque>

#include <Sys/BinaryTree.h>
#include <Sys/MonoAlloc.h>

#include <optick/optick.h>
#include <vtune/ittnotify.h>
extern __itt_domain *__g_itt_domain;

#include "../Utils/BVHSplit.h"

namespace SceneManagerInternal {
const float BoundsMargin = 0.2f;

float surface_area(const bvh_node_t &n) {
    const Ren::Vec3f d = n.bbox_max - n.bbox_min;
    return d[0] * d[1] + d[0] * d[2] + d[1] * d[2];
}

float surface_area_of_union(const bvh_node_t &n1, const bvh_node_t &n2) {
    const Ren::Vec3f d = Max(n1.bbox_max, n2.bbox_max) - Min(n1.bbox_min, n2.bbox_max);
    return d[0] * d[1] + d[0] * d[2] + d[1] * d[2];
}

struct insert_candidate_t {
    uint32_t node_index;
    float direct_cost;
};

bool insert_candidate_compare(insert_candidate_t c1, insert_candidate_t c2) { return c1.direct_cost > c2.direct_cost; }

void sort_children(const bvh_node_t *nodes, bvh_node_t &node) {
    const uint32_t ch0 = node.left_child, ch1 = node.right_child;

    uint32_t space_axis = 0;
    const Ren::Vec3f c_left = (nodes[ch0].bbox_min + nodes[ch0].bbox_max) / 2,
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
        const uint32_t temp = node.left_child;
        node.left_child = node.right_child;
        node.right_child = temp;
    }
}

void update_bbox(const bvh_node_t *nodes, bvh_node_t &node) {
    node.bbox_min = Min(nodes[node.left_child].bbox_min, nodes[node.right_child].bbox_min);
    node.bbox_max = Max(nodes[node.left_child].bbox_max, nodes[node.right_child].bbox_max);
}

__itt_string_handle *itt_rebuild_bvh_str = __itt_string_handle_create("SceneManager::RebuildSceneBVH");
__itt_string_handle *itt_update_bvh_str = __itt_string_handle_create("SceneManager::UpdateBVH");
} // namespace SceneManagerInternal

void SceneManager::RebuildSceneBVH() {
    using namespace SceneManagerInternal;

    __itt_task_begin(__g_itt_domain, __itt_null, __itt_null, itt_rebuild_bvh_str);

    auto *transforms = (Transform *)scene_data_.comp_store[CompTransform]->SequentialData();

    std::vector<prim_t> primitives;
    primitives.reserve(scene_data_.objects.size());

    for (const SceneObject &obj : scene_data_.objects) {
        if (obj.comp_mask & CompTransformBit) {
            const Transform &tr = transforms[obj.components[CompTransform]];
            const Ren::Vec3f d = tr.bbox_max_ws - tr.bbox_min_ws;
            primitives.push_back({0, 0, 0, tr.bbox_min_ws - BoundsMargin * d, tr.bbox_max_ws + BoundsMargin * d});
        }
    }

    scene_data_.nodes.clear();
    scene_data_.root_node = 0xffffffff;

    if (primitives.empty())
        return;

    struct prims_coll_t {
        std::vector<uint32_t> indices;
        Ren::Vec3f min = Ren::Vec3f{std::numeric_limits<float>::max()},
                   max = Ren::Vec3f{std::numeric_limits<float>::lowest()};
        prims_coll_t() = default;
        prims_coll_t(std::vector<uint32_t> &&_indices, const Ren::Vec3f &_min, const Ren::Vec3f &_max)
            : indices(std::move(_indices)), min(_min), max(_max) {}
    };

    std::deque<prims_coll_t> prim_lists;
    prim_lists.emplace_back();

    size_t nodes_count = scene_data_.nodes.size();
    scene_data_.root_node = uint32_t(nodes_count);

    for (size_t i = 0; i < primitives.size(); i++) {
        prim_lists.back().indices.push_back(uint32_t(i));
        prim_lists.back().min = Min(prim_lists.back().min, primitives[i].bbox_min);
        prim_lists.back().max = Max(prim_lists.back().max, primitives[i].bbox_max);
    }

    const Ren::Vec3f root_min = prim_lists.back().min, root_max = prim_lists.back().max;

    split_settings_t s;
    s.oversplit_threshold = std::numeric_limits<float>::max();
    s.node_traversal_cost = 0.0f;

    while (!prim_lists.empty()) {
        split_data_t split_data = SplitPrimitives_SAH(&primitives[0], prim_lists.back().indices, prim_lists.back().min,
                                                      prim_lists.back().max, root_min, root_max, s);
        prim_lists.pop_back();

        const uint32_t leaf_index = uint32_t(scene_data_.nodes.size());
        uint32_t parent_index = 0xffffffff;

        if (leaf_index) {
            // skip bound checks in debug mode
            const bvh_node_t *_out_nodes = &scene_data_.nodes[0];
            for (uint32_t i = leaf_index - 1; i >= scene_data_.root_node; i--) {
                if (_out_nodes[i].left_child == leaf_index || _out_nodes[i].right_child == leaf_index) {
                    parent_index = uint32_t(i);
                    break;
                }
            }
        }

        if (split_data.right_indices.empty()) {
            const Ren::Vec3f bbox_min = split_data.left_bounds[0], bbox_max = split_data.left_bounds[1];

            const auto new_node_index = (uint32_t)scene_data_.nodes.size();

            for (const uint32_t i : split_data.left_indices) {
                Transform &tr = transforms[scene_data_.objects[i].components[CompTransform]];
                tr.node_index = new_node_index;
            }

            assert(split_data.left_indices.size() == 1 && "Wrong split!");

            scene_data_.nodes.emplace_back();
            bvh_node_t &n = scene_data_.nodes.back();

            n.bbox_min = Ren::Vec3f{bbox_min[0], bbox_min[1], bbox_min[2]};
            n.leaf_node = 1;
            n.prim_index = split_data.left_indices[0];
            n.bbox_max = Ren::Vec3f{bbox_max[0], bbox_max[1], bbox_max[2]};
            n.space_axis = 0;
            n.prim_count = uint32_t(split_data.left_indices.size());
            n.parent = parent_index;
        } else {
            auto index = (uint32_t)nodes_count;

            const Ren::Vec3f c_left = (split_data.left_bounds[0] + split_data.left_bounds[1]) / 2.0f,
                             c_right = (split_data.right_bounds[0] + split_data.right_bounds[1]) / 2.0f;

            const Ren::Vec3f dist = Abs(c_left - c_right);

            const uint32_t space_axis =
                (dist[0] > dist[1] && dist[0] > dist[2]) ? 0 : ((dist[1] > dist[0] && dist[1] > dist[2]) ? 1 : 2);

            const Ren::Vec3f bbox_min = Min(split_data.left_bounds[0], split_data.right_bounds[0]),
                             bbox_max = Max(split_data.left_bounds[1], split_data.right_bounds[1]);

            scene_data_.nodes.emplace_back();
            bvh_node_t &n = scene_data_.nodes.back();

            n.bbox_min = Ren::Vec3f{bbox_min[0], bbox_min[1], bbox_min[2]};
            n.leaf_node = 0;
            n.left_child = index + 1;
            n.bbox_max = Ren::Vec3f{bbox_max[0], bbox_max[1], bbox_max[2]};
            n.space_axis = space_axis;
            n.right_child = index + 2;
            n.parent = parent_index;

            prim_lists.emplace_front(std::move(split_data.left_indices), split_data.left_bounds[0],
                                     split_data.left_bounds[1]);
            prim_lists.emplace_front(std::move(split_data.right_indices), split_data.right_bounds[0],
                                     split_data.right_bounds[1]);

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

    __itt_task_end(__g_itt_domain);
}

void SceneManager::RemoveNode(const uint32_t node_index) {
    using namespace SceneManagerInternal;

    bvh_node_t *nodes = scene_data_.nodes.data();
    bvh_node_t &node = nodes[node_index];

    if (node.parent != 0xffffffff) {
        bvh_node_t &parent = nodes[node.parent];

        const uint32_t other_child = (parent.left_child == node_index) ? parent.right_child : parent.left_child;
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
                const uint32_t ch0 = nodes[up_parent].left_child, ch1 = nodes[up_parent].right_child;

                nodes[up_parent].bbox_min = Min(nodes[ch0].bbox_min, nodes[ch1].bbox_min);
                nodes[up_parent].bbox_max = Max(nodes[ch0].bbox_max, nodes[ch1].bbox_max);

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

    OPTICK_EVENT("SceneManager::UpdateObjects");
    __itt_task_begin(__g_itt_domain, __itt_null, __itt_null, itt_update_bvh_str);

    const auto *physes = (Physics *)scene_data_.comp_store[CompPhysics]->SequentialData();
    auto *transforms = (Transform *)scene_data_.comp_store[CompTransform]->SequentialData();

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

        if (obj.change_mask & CompPhysicsBit) {
            const Physics &ph = physes[obj.components[CompPhysics]];
            Transform &tr = transforms[obj.components[CompTransform]];

            tr.world_from_object_prev = tr.world_from_object;
            tr.world_from_object = Ren::Mat4f{1.0f};

            // Copy position
            tr.world_from_object[3][0] = float(ph.body.pos[0]);
            tr.world_from_object[3][1] = float(ph.body.pos[1]);
            tr.world_from_object[3][2] = float(ph.body.pos[2]);

            // Copy orientation
            const Phy::Mat3 ph_rot = Phy::ToMat3(ph.body.rot);
            for (int j = 0; j < 3; j++) {
                for (int i = 0; i < 3; i++) {
                    tr.world_from_object[j][i] = float(ph_rot[j][i]);
                }
            }

            tr.world_from_object = Scale(tr.world_from_object, tr.scale);

            obj.change_mask |= CompTransformBit;
            obj.change_mask ^= CompPhysicsBit;
        }

        if (obj.change_mask & CompTransformBit) {
            Transform &tr = transforms[obj.components[CompTransform]];
            tr.UpdateTemporaryData();
            if (tr.node_index != 0xffffffff) {
                assert(tr.node_index < scene_data_.nodes.size());
                const bvh_node_t &node = nodes[tr.node_index];

                const bool is_fully_inside =
                    tr.bbox_min_ws[0] >= node.bbox_min[0] && tr.bbox_min_ws[1] >= node.bbox_min[1] &&
                    tr.bbox_min_ws[2] >= node.bbox_min[2] && tr.bbox_max_ws[0] <= node.bbox_max[0] &&
                    tr.bbox_max_ws[1] <= node.bbox_max[1] && tr.bbox_max_ws[2] <= node.bbox_max[2];

                if (is_fully_inside) {
                    // Update is not needed (object is still inside of node bounds)
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

    // for (const uint32_t obj_index : changed_objects_) {
    for (int k = 0; k < int(changed_objects_.size()); k++) {
        const uint32_t obj_index = changed_objects_[k];
        SceneObject &obj = scene_data_.objects[obj_index];

        if (obj.change_mask & CompTransformBit) {
            Transform &tr = transforms[obj.components[CompTransform]];
            tr.node_index = free_nodes[free_nodes_pos++];

            bvh_node_t &new_node = nodes[tr.node_index];

            const Ren::Vec3f d = tr.bbox_max_ws - tr.bbox_min_ws;
            new_node.bbox_min = tr.bbox_min_ws - BoundsMargin * d;
            new_node.leaf_node = 1;
            new_node.prim_index = obj_index;
            new_node.bbox_max = tr.bbox_max_ws + BoundsMargin * d;
            new_node.space_axis = 0;
            new_node.prim_count = 1;

            Sys::MonoAlloc<insert_candidate_t> m_alloc(temp_buf.data(), temp_buf.size());
            Sys::BinaryTree<insert_candidate_t, decltype(&insert_candidate_compare), Sys::MonoAlloc<insert_candidate_t>>
                candidates(insert_candidate_compare, m_alloc);

            const float node_cost = surface_area(new_node);

            uint32_t best_candidate = scene_data_.root_node;
            float best_cost = surface_area_of_union(new_node, nodes[best_candidate]);
            candidates.push({best_candidate, best_cost});

            while (!candidates.empty()) {
                insert_candidate_t c; // NOLINT
                candidates.extract_top(c);

                float inherited_cost = 0.0f;
                uint32_t parent = nodes[c.node_index].parent;

                while (parent != 0xffffffff) {
                    // add change in surface area
                    inherited_cost += surface_area_of_union(nodes[parent], new_node) - surface_area(nodes[parent]);
                    parent = nodes[parent].parent;
                }

                const float total_cost = c.direct_cost + inherited_cost;
                if (total_cost < best_cost) {
                    best_candidate = c.node_index;
                    best_cost = total_cost;
                }

                // consider children next
                if (!nodes[c.node_index].prim_count) {
                    const float candidate_cost = surface_area(nodes[c.node_index]);
                    const float lower_cost_bound = node_cost + inherited_cost +
                                                   surface_area_of_union(nodes[c.node_index], new_node) -
                                                   candidate_cost;

                    if (lower_cost_bound < best_cost) {
                        const uint32_t ch0 = nodes[c.node_index].left_child, ch1 = nodes[c.node_index].right_child;
                        candidates.push({ch0, surface_area_of_union(nodes[ch0], new_node)});
                        candidates.push({ch1, surface_area_of_union(nodes[ch1], new_node)});
                    }
                }
            }

            const uint32_t old_parent = nodes[best_candidate].parent;
            const uint32_t new_parent = free_nodes[free_nodes_pos++];

            nodes[new_parent].leaf_node = 0;
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

                par_node.bbox_min = Min(left_child_of(par_node).bbox_min, right_child_of(par_node).bbox_min);
                par_node.bbox_max = Max(left_child_of(par_node).bbox_max, right_child_of(par_node).bbox_max);

                // rotate left_child with children of right_child
                if (!right_child_of(par_node).prim_count) {
                    float cost_before = surface_area(nodes[par_node.right_child]);

                    float rotation_costs[2] = {
                        surface_area_of_union(left_child_of(par_node), right_child_of(right_child_of(par_node))),
                        surface_area_of_union(left_child_of(par_node), left_child_of(right_child_of(par_node)))};

                    int best_rot = rotation_costs[0] < rotation_costs[1] ? 0 : 1;
                    if (rotation_costs[best_rot] < cost_before) {
                        left_child_of(par_node).parent = par_node.right_child;

                        if (best_rot == 0) {
                            left_child_of(right_child_of(par_node)).parent = parent;

                            const uint32_t temp = par_node.left_child;
                            par_node.left_child = right_child_of(par_node).left_child;
                            right_child_of(par_node).left_child = temp;
                        } else {
                            right_child_of(right_child_of(par_node)).parent = parent;

                            const uint32_t temp = par_node.left_child;
                            par_node.left_child = right_child_of(par_node).right_child;
                            right_child_of(par_node).right_child = temp;
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
                        surface_area_of_union(right_child_of(par_node), left_child_of(left_child_of(par_node)))};

                    const int best_rot = rotation_costs[0] < rotation_costs[1] ? 0 : 1;
                    if (rotation_costs[best_rot] < cost_before) {
                        right_child_of(par_node).parent = par_node.left_child;

                        if (best_rot == 0) {
                            left_child_of(left_child_of(par_node)).parent = parent;

                            const uint32_t temp = par_node.right_child;
                            par_node.right_child = left_child_of(par_node).left_child;
                            left_child_of(par_node).left_child = temp;
                        } else {
                            right_child_of(left_child_of(par_node)).parent = parent;

                            const uint32_t temp = par_node.right_child;
                            par_node.right_child = left_child_of(par_node).right_child;
                            left_child_of(par_node).right_child = temp;
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

    __itt_task_end(__g_itt_domain);
}

void SceneManager::InitSWAccStructures() {
    using namespace SceneManagerInternal;

    const VkDeviceSize AccStructAlignment = 256;

    Ren::ApiContext *api_ctx = ren_ctx_.api_ctx();

    struct Blas {
        // Ren::SmallVector<VkAccelerationStructureGeometryKHR, 16> geometries;
        // Ren::SmallVector<VkAccelerationStructureBuildRangeInfoKHR, 16> build_ranges;
        Ren::SmallVector<uint32_t, 16> prim_counts;
        // VkAccelerationStructureBuildSizesInfoKHR size_info;
        // VkAccelerationStructureBuildGeometryInfoKHR build_info;
        AccStructure *acc;
    };
    std::vector<Blas> all_blases;

    // uint32_t needed_build_scratch_size = 0;
    // uint32_t needed_total_acc_struct_size = 0;

    std::vector<prim_t> temp_primitives;

    std::vector<gpu_bvh_node_t> nodes;
    std::vector<uint32_t> node_indices;

    uint32_t acc_index = scene_data_.comp_store[CompAccStructure]->First();
    while (acc_index != 0xffffffff) {
        auto *acc = (AccStructure *)scene_data_.comp_store[CompAccStructure]->Get(acc_index);
        if (acc->mesh->blas) {
            // already processed
            acc_index = scene_data_.comp_store[CompAccStructure]->Next(acc_index);
            continue;
        }

        const Ren::BufferRange &attribs = acc->mesh->attribs_buf1();
        const Ren::BufferRange &indices = acc->mesh->indices_buf();

        const float *positions = reinterpret_cast<const float *>(acc->mesh->attribs());
        const uint32_t *tri_indices = reinterpret_cast<const uint32_t *>(acc->mesh->indices());
        const uint32_t tri_indices_count = indices.size / sizeof(uint32_t);

        for (uint32_t i = 0; i < tri_indices_count; i += 3) {
            const uint32_t i0 = tri_indices[i + 0], i1 = tri_indices[i + 1], i2 = tri_indices[i + 2];

            const Ren::Vec3f p0 = Ren::MakeVec3(&positions[i0 * 4]), p1 = Ren::MakeVec3(&positions[i1 * 4]),
                             p2 = Ren::MakeVec3(&positions[i2 * 4]);

            const Ren::Vec3f bbox_min = Min(p0, Min(p1, p2)), bbox_max = Max(p0, Max(p1, p2));

            temp_primitives.push_back({i0, i1, i2, bbox_min, bbox_max});
        }

        split_settings_t s;
        const uint32_t nodes_count = PreprocessPrims_SAH(temp_primitives, s, nodes, node_indices);

        // VkAccelerationStructureGeometryTrianglesDataKHR tri_data = {
        //     VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR};
        // tri_data.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
        // tri_data.vertexData.deviceAddress = attribs.buf->vk_device_address();
        // tri_data.vertexStride = 16;
        // tri_data.indexType = VK_INDEX_TYPE_UINT32;
        // tri_data.indexData.deviceAddress = indices.buf->vk_device_address();
        // tri_data.maxVertex = attribs.size / 16;

        //
        // Gather geometries
        //
        all_blases.emplace_back();
        Blas &new_blas = all_blases.back();

        const uint32_t indices_start = indices.offset;
        for (const Ren::TriGroup &grp : acc->mesh->groups()) {
            const Ren::Material *mat = grp.mat.get();
            const uint32_t mat_flags = mat->flags();

            if ((mat_flags & uint32_t(Ren::eMatFlags::AlphaBlend)) != 0) {
                // Include only opaque surfaces
                continue;
            }

            // auto &new_geo = new_blas.geometries.emplace_back();
            // new_geo = {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
            // new_geo.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
            // new_geo.flags = 0;
            // if ((mat_flags & uint32_t(Ren::eMatFlags::AlphaTest)) == 0) {
            //     new_geo.flags |= VK_GEOMETRY_OPAQUE_BIT_KHR;
            // }
            // new_geo.geometry.triangles = tri_data;

            // auto &new_range = new_blas.build_ranges.emplace_back();
            // new_range.firstVertex = attribs.offset / 16;
            // new_range.primitiveCount = grp.num_indices / 3;
            // new_range.primitiveOffset = indices_start + grp.offset;
            // new_range.transformOffset = 0;

            // new_blas.prim_counts.push_back(grp.num_indices / 3);
        }

        //
        // Query needed memory
        //
        /* new_blas.build_info = {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
        new_blas.build_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        new_blas.build_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        new_blas.build_info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
                                    VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR;
        new_blas.build_info.geometryCount = uint32_t(new_blas.geometries.size());
        new_blas.build_info.pGeometries = new_blas.geometries.cdata();

        new_blas.size_info = {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
        vkGetAccelerationStructureBuildSizesKHR(api_ctx->device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                                &new_blas.build_info, new_blas.prim_counts.cdata(),
                                                &new_blas.size_info);

        // make sure we will not use this potentially stale pointer
        new_blas.build_info.pGeometries = nullptr;

        needed_build_scratch_size = std::max(needed_build_scratch_size, uint32_t(new_blas.size_info.buildScratchSize));
        needed_total_acc_struct_size +=
            uint32_t(align_up(new_blas.size_info.accelerationStructureSize, AccStructAlignment));

        new_blas.acc = acc;*/
        acc->mesh->blas.reset(new Ren::AccStructureSW);

        acc_index = scene_data_.comp_store[CompAccStructure]->Next(acc_index);
        break;
    }

    const uint32_t total_nodes_size = uint32_t(nodes.size() * sizeof(gpu_bvh_node_t));

    Ren::Buffer rt_blas_stage_buf("SWRT BLAS Stage Buf", api_ctx, Ren::eBufType::Stage, total_nodes_size);
    {
        uint8_t *rt_blas_stage = rt_blas_stage_buf.Map(Ren::BufMapWrite);
        memcpy(rt_blas_stage, nodes.data(), total_nodes_size);
        rt_blas_stage_buf.Unmap();
    }

    scene_data_.persistent_data.rt_blas_buf =
        ren_ctx_.LoadBuffer("SWRT BLAS Buf", Ren::eBufType::Storage, total_nodes_size);

    VkCommandBuffer cmd_buf = Ren::BegSingleTimeCommands(api_ctx->device, api_ctx->temp_command_pool);

    Ren::CopyBufferToBuffer(rt_blas_stage_buf, 0, *scene_data_.persistent_data.rt_blas_buf, 0, total_nodes_size,
                            cmd_buf);

    Ren::EndSingleTimeCommands(api_ctx->device, api_ctx->graphics_queue, cmd_buf, api_ctx->temp_command_pool);

#if 0
    if (!all_blases.empty()) {
        //
        // Allocate memory
        //
        Ren::Buffer scratch_buf("BLAS Scratch Buf", api_ctx, Ren::eBufType::Storage,
                                next_power_of_two(needed_build_scratch_size));
        VkDeviceAddress scratch_addr = scratch_buf.vk_device_address();

        Ren::Buffer acc_structs_buf("BLAS Before-Compaction Buf", api_ctx, Ren::eBufType::AccStructure,
                                    needed_total_acc_struct_size);

        //

        VkQueryPoolCreateInfo query_pool_create_info = {VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
        query_pool_create_info.queryCount = uint32_t(all_blases.size());
        query_pool_create_info.queryType = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR;

        VkQueryPool query_pool;
        VkResult res = vkCreateQueryPool(api_ctx->device, &query_pool_create_info, nullptr, &query_pool);
        assert(res == VK_SUCCESS);

        { // Submit build commands
            VkDeviceSize acc_buf_offset = 0;
            VkCommandBuffer cmd_buf = Ren::BegSingleTimeCommands(api_ctx->device, api_ctx->temp_command_pool);

            vkCmdResetQueryPool(cmd_buf, query_pool, 0, uint32_t(all_blases.size()));

            for (int i = 0; i < int(all_blases.size()); ++i) {
                VkAccelerationStructureCreateInfoKHR acc_create_info = {
                    VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
                acc_create_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
                acc_create_info.buffer = acc_structs_buf.vk_handle();
                acc_create_info.offset = acc_buf_offset;
                acc_create_info.size = all_blases[i].size_info.accelerationStructureSize;
                acc_buf_offset += align_up(acc_create_info.size, AccStructAlignment);

                VkAccelerationStructureKHR acc_struct;
                VkResult res =
                    vkCreateAccelerationStructureKHR(api_ctx->device, &acc_create_info, nullptr, &acc_struct);
                if (res != VK_SUCCESS) {
                    ren_ctx_.log()->Error(
                        "[SceneManager::InitHWAccStructures]: Failed to create acceleration structure!");
                }

                auto &vk_blas = static_cast<Ren::AccStructureVK &>(*all_blases[i].acc->mesh->blas);
                if (!vk_blas.Init(api_ctx, acc_struct)) {
                    ren_ctx_.log()->Error("[SceneManager::InitHWAccStructures]: Failed to init BLAS!");
                }

                all_blases[i].build_info.pGeometries = all_blases[i].geometries.cdata();

                all_blases[i].build_info.dstAccelerationStructure = acc_struct;
                all_blases[i].build_info.scratchData.deviceAddress = scratch_addr;

                const VkAccelerationStructureBuildRangeInfoKHR *build_ranges = all_blases[i].build_ranges.cdata();
                vkCmdBuildAccelerationStructuresKHR(cmd_buf, 1, &all_blases[i].build_info, &build_ranges);

                { // Place barrier
                    VkMemoryBarrier scr_buf_barrier = {VK_STRUCTURE_TYPE_MEMORY_BARRIER};
                    scr_buf_barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
                    scr_buf_barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;

                    vkCmdPipelineBarrier(cmd_buf, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                                         VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &scr_buf_barrier,
                                         0, nullptr, 0, nullptr);
                }

                vkCmdWriteAccelerationStructuresPropertiesKHR(
                    cmd_buf, 1, &all_blases[i].build_info.dstAccelerationStructure,
                    VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR, query_pool, i);
            }

            Ren::EndSingleTimeCommands(api_ctx->device, api_ctx->graphics_queue, cmd_buf, api_ctx->temp_command_pool);
        }

        std::vector<VkDeviceSize> compact_sizes(all_blases.size());
        res = vkGetQueryPoolResults(api_ctx->device, query_pool, 0, uint32_t(all_blases.size()),
                                    all_blases.size() * sizeof(VkDeviceSize), compact_sizes.data(),
                                    sizeof(VkDeviceSize), VK_QUERY_RESULT_WAIT_BIT);
        assert(res == VK_SUCCESS);

        vkDestroyQueryPool(api_ctx->device, query_pool, nullptr);

        VkDeviceSize total_compacted_size = 0;
        for (int i = 0; i < int(compact_sizes.size()); ++i) {
            total_compacted_size += align_up(compact_sizes[i], AccStructAlignment);
        }

        scene_data_.persistent_data.rt_blas_buf = ren_ctx_.LoadBuffer(
            "BLAS After-Compaction Buf", Ren::eBufType::AccStructure, uint32_t(total_compacted_size));

        { // Submit compaction commands
            VkDeviceSize compact_acc_buf_offset = 0;
            VkCommandBuffer cmd_buf = Ren::BegSingleTimeCommands(api_ctx->device, api_ctx->temp_command_pool);

            for (int i = 0; i < int(all_blases.size()); ++i) {
                VkAccelerationStructureCreateInfoKHR acc_create_info = {
                    VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
                acc_create_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
                acc_create_info.buffer = scene_data_.persistent_data.rt_blas_buf->vk_handle();
                acc_create_info.offset = compact_acc_buf_offset;
                acc_create_info.size = compact_sizes[i];
                assert(compact_acc_buf_offset + compact_sizes[i] <= total_compacted_size);
                compact_acc_buf_offset += align_up(acc_create_info.size, AccStructAlignment);

                VkAccelerationStructureKHR compact_acc_struct;
                const VkResult res =
                    vkCreateAccelerationStructureKHR(api_ctx->device, &acc_create_info, nullptr, &compact_acc_struct);
                if (res != VK_SUCCESS) {
                    ren_ctx_.log()->Error(
                        "[SceneManager::InitHWAccStructures]: Failed to create acceleration structure!");
                }

                auto &vk_blas = static_cast<Ren::AccStructureVK &>(*all_blases[i].acc->mesh->blas);

                VkCopyAccelerationStructureInfoKHR copy_info = {VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR};
                copy_info.mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR;
                copy_info.src = vk_blas.vk_handle();
                copy_info.dst = compact_acc_struct;

                vkCmdCopyAccelerationStructureKHR(cmd_buf, &copy_info);

                if (!vk_blas.Init(api_ctx, compact_acc_struct)) {
                    ren_ctx_.log()->Error("[SceneManager::InitHWAccStructures]: Blas compaction failed!");
                }
            }

            Ren::EndSingleTimeCommands(api_ctx->device, api_ctx->graphics_queue, cmd_buf, api_ctx->temp_command_pool);
        }
    }

    //
    // Build TLAS
    //

    // retrieve pointers to components for fast access
    const auto *transforms = (Transform *)scene_data_.comp_store[CompTransform]->SequentialData();
    const auto *acc_structs = (AccStructure *)scene_data_.comp_store[CompAccStructure]->SequentialData();
    const auto *lightmaps = (Lightmap *)scene_data_.comp_store[CompLightmap]->SequentialData();
    const auto *probes = (LightProbe *)scene_data_.comp_store[CompProbe]->SequentialData();
    const CompStorage *probe_store = scene_data_.comp_store[CompProbe];

    std::vector<RTGeoInstance> geo_instances;
    std::vector<VkAccelerationStructureInstanceKHR> tlas_instances;

    for (const auto &obj : scene_data_.objects) {
        if ((obj.comp_mask & (CompTransformBit | CompAccStructureBit)) != (CompTransformBit | CompAccStructureBit)) {
            continue;
        }

        const Transform &tr = transforms[obj.components[CompTransform]];
        const AccStructure &acc = acc_structs[obj.components[CompAccStructure]];
        const Lightmap *lm = nullptr;
        if (obj.comp_mask & CompLightmapBit) {
            lm = &lightmaps[obj.components[CompLightmap]];
        }
        uint32_t closest_probe = 0xffffffff;
        if (obj.comp_mask & CompProbeBit) {
            closest_probe = probes[obj.components[CompProbe]].layer_index;
        }

        auto &vk_blas = static_cast<Ren::AccStructureVK &>(*acc.mesh->blas);
        vk_blas.geo_index = uint32_t(geo_instances.size());
        vk_blas.geo_count = 0;

        tlas_instances.emplace_back();
        auto &new_instance = tlas_instances.back();
        to_khr_xform(tr.world_from_object, new_instance.transform.matrix);
        new_instance.instanceCustomIndex = vk_blas.geo_index;
        new_instance.mask = 0xff;
        new_instance.instanceShaderBindingTableRecordOffset = 0;
        new_instance.flags = 0;
        // VK_GEOMETRY_INSTANCE_TRIANGLE_FRONT_COUNTERCLOCKWISE_BIT_KHR; //
        // VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        new_instance.accelerationStructureReference = static_cast<uint64_t>(vk_blas.vk_device_address());

        const uint32_t indices_start = acc.mesh->indices_buf().offset;
        for (const Ren::TriGroup &grp : acc.mesh->groups()) {
            const Ren::Material *mat = grp.mat.get();
            const uint32_t mat_flags = mat->flags();
            if ((mat_flags & uint32_t(Ren::eMatFlags::AlphaBlend)) != 0) {
                // Include only opaque surfaces
                continue;
            }

            ++vk_blas.geo_count;

            geo_instances.emplace_back();
            auto &geo = geo_instances.back();
            geo.indices_start = (indices_start + grp.offset) / sizeof(uint32_t);
            geo.vertices_start = acc.mesh->attribs_buf1().offset / 16;
            geo.material_index = grp.mat.index();
            geo.flags = 0;
            if (lm) {
                geo.flags |= RTGeoLightmappedBit;
                memcpy(&geo.lmap_transform[0], ValuePtr(lm->xform), 4 * sizeof(float));
            } else {
                if (closest_probe == 0xffffffff) {
                    // find closest probe
                    float min_dist2 = std::numeric_limits<float>::max();
                    for (const auto &probe : scene_data_.objects) {
                        if ((probe.comp_mask & (CompTransformBit | CompProbeBit)) !=
                            (CompTransformBit | CompProbeBit)) {
                            continue;
                        }

                        const Transform &probe_tr = transforms[probe.components[CompTransform]];
                        const LightProbe &probe_pr = probes[probe.components[CompProbe]];

                        const float dist2 =
                            Distance2(0.5f * (tr.bbox_min_ws + tr.bbox_max_ws),
                                      0.5f * (probe_tr.bbox_min_ws + probe_tr.bbox_max_ws) + probe_pr.offset);
                        if (dist2 < min_dist2) {
                            closest_probe = probe_pr.layer_index;
                            min_dist2 = dist2;
                        }
                    }
                }
                geo.flags |= (closest_probe & 0xff);
            }
        }
    }

    if (geo_instances.empty()) {
        geo_instances.emplace_back();
        auto &dummy_geo = geo_instances.back();
        dummy_geo = {};

        tlas_instances.emplace_back();
        auto &dummy_instance = tlas_instances.back();
        dummy_instance = {};
    }

    scene_data_.persistent_data.rt_geo_data_buf = ren_ctx_.LoadBuffer(
        "RT Geo Data Buf", Ren::eBufType::Storage, uint32_t(geo_instances.size() * sizeof(RTGeoInstance)));
    Ren::Buffer geo_data_stage_buf("RT Geo Data Stage Buf", api_ctx, Ren::eBufType::Stage,
                                   uint32_t(geo_instances.size() * sizeof(RTGeoInstance)));

    {
        uint8_t *geo_data_stage = geo_data_stage_buf.Map(Ren::BufMapWrite);
        memcpy(geo_data_stage, geo_instances.data(), geo_instances.size() * sizeof(RTGeoInstance));
        geo_data_stage_buf.Unmap();
    }

    scene_data_.persistent_data.rt_instance_buf =
        ren_ctx_.LoadBuffer("RT Instance Buf", Ren::eBufType::Storage,
                            uint32_t(REN_MAX_RT_OBJ_INSTANCES * sizeof(VkAccelerationStructureInstanceKHR)));
    // Ren::Buffer instance_stage_buf("RT Instance Stage Buf", api_ctx, Ren::eBufType::Stage,
    //                                uint32_t(tlas_instances.size() * sizeof(VkAccelerationStructureInstanceKHR)));

    /*{
        uint8_t *instance_stage = instance_stage_buf.Map(Ren::BufMapWrite);
        memcpy(instance_stage, tlas_instances.data(),
               tlas_instances.size() * sizeof(VkAccelerationStructureInstanceKHR));
        instance_stage_buf.Unmap();
    }*/

    VkDeviceAddress instance_buf_addr = scene_data_.persistent_data.rt_instance_buf->vk_device_address();

#if 1
    VkCommandBuffer cmd_buf = Ren::BegSingleTimeCommands(api_ctx->device, api_ctx->temp_command_pool);

    Ren::CopyBufferToBuffer(geo_data_stage_buf, 0, *scene_data_.persistent_data.rt_geo_data_buf, 0,
                            geo_data_stage_buf.size(), cmd_buf);

    { // Make sure compaction copying of BLASes has finished
        VkMemoryBarrier mem_barrier = {VK_STRUCTURE_TYPE_MEMORY_BARRIER};
        mem_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        mem_barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;

        vkCmdPipelineBarrier(cmd_buf, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &mem_barrier, 0, nullptr, 0,
                             nullptr);
    }

    const uint32_t max_instance_count = REN_MAX_RT_OBJ_INSTANCES; // allocate for worst case

    VkAccelerationStructureGeometryInstancesDataKHR instances_data = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR};
    instances_data.data.deviceAddress = instance_buf_addr;

    VkAccelerationStructureGeometryKHR tlas_geo = {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
    tlas_geo.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    tlas_geo.geometry.instances = instances_data;

    VkAccelerationStructureBuildGeometryInfoKHR tlas_build_info = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
    tlas_build_info.flags =
        VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_NV;
    tlas_build_info.geometryCount = 1;
    tlas_build_info.pGeometries = &tlas_geo;
    tlas_build_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    tlas_build_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    tlas_build_info.srcAccelerationStructure = VK_NULL_HANDLE;

    VkAccelerationStructureBuildSizesInfoKHR size_info = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
    vkGetAccelerationStructureBuildSizesKHR(api_ctx->device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                            &tlas_build_info, &max_instance_count, &size_info);

    scene_data_.persistent_data.rt_tlas_buf =
        ren_ctx_.LoadBuffer("TLAS Buf", Ren::eBufType::AccStructure, uint32_t(size_info.accelerationStructureSize));
    scene_data_.persistent_data.rt_sh_tlas_buf = ren_ctx_.LoadBuffer("TLAS Shadow Buf", Ren::eBufType::AccStructure,
                                                                     uint32_t(size_info.accelerationStructureSize));

    Ren::BufferRef tlas_scratch_buf =
        ren_ctx_.LoadBuffer("TLAS Scratch Buf", Ren::eBufType::Storage, uint32_t(size_info.buildScratchSize));

    { // Main TLAS
        VkAccelerationStructureCreateInfoKHR create_info = {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
        create_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        create_info.buffer = scene_data_.persistent_data.rt_tlas_buf->vk_handle();
        create_info.offset = 0;
        create_info.size = size_info.accelerationStructureSize;

        VkAccelerationStructureKHR tlas_handle;
        VkResult res = vkCreateAccelerationStructureKHR(api_ctx->device, &create_info, nullptr, &tlas_handle);
        if (res != VK_SUCCESS) {
            ren_ctx_.log()->Error("[SceneManager::InitHWAccStructures]: Failed to create acceleration structure!");
        }

        std::unique_ptr<Ren::AccStructureVK> vk_tlas(new Ren::AccStructureVK);
        if (!vk_tlas->Init(api_ctx, tlas_handle)) {
            ren_ctx_.log()->Error("[SceneManager::InitHWAccStructures]: Failed to init TLAS!");
        }
        scene_data_.persistent_data.rt_tlas = std::move(vk_tlas);
    }

    { // Shadow TLAS
        VkAccelerationStructureCreateInfoKHR create_info = {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
        create_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        create_info.buffer = scene_data_.persistent_data.rt_sh_tlas_buf->vk_handle();
        create_info.offset = 0;
        create_info.size = size_info.accelerationStructureSize;

        VkAccelerationStructureKHR tlas_handle;
        VkResult res = vkCreateAccelerationStructureKHR(api_ctx->device, &create_info, nullptr, &tlas_handle);
        if (res != VK_SUCCESS) {
            ren_ctx_.log()->Error("[SceneManager::InitHWAccStructures]: Failed to create acceleration structure!");
        }

        std::unique_ptr<Ren::AccStructureVK> vk_tlas(new Ren::AccStructureVK);
        if (!vk_tlas->Init(api_ctx, tlas_handle)) {
            ren_ctx_.log()->Error("[SceneManager::InitHWAccStructures]: Failed to init TLAS!");
        }
        scene_data_.persistent_data.rt_sh_tlas = std::move(vk_tlas);
    }

    scene_data_.persistent_data.rt_tlas_build_scratch_size = uint32_t(size_info.buildScratchSize);

    Ren::EndSingleTimeCommands(api_ctx->device, api_ctx->graphics_queue, cmd_buf, api_ctx->temp_command_pool);
#else
    VkCommandBuffer cmd_buf = Ren::BegSingleTimeCommands(api_ctx->device, api_ctx->temp_command_pool);

    Ren::CopyBufferToBuffer(geo_data_stage_buf, 0, *scene_data_.persistent_data.rt_geo_data_buf, 0,
                            geo_data_stage_buf.size(), cmd_buf);

    Ren::CopyBufferToBuffer(instance_stage_buf, 0, *scene_data_.persistent_data.rt_instance_buf, 0,
                            instance_stage_buf.size(), cmd_buf);

    { // Make sure compaction copying of BLASes has finished
        VkMemoryBarrier mem_barrier = {VK_STRUCTURE_TYPE_MEMORY_BARRIER};
        mem_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        mem_barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;

        vkCmdPipelineBarrier(cmd_buf, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &mem_barrier, 0, nullptr, 0,
                             nullptr);
    }

    { //
        VkAccelerationStructureGeometryInstancesDataKHR instances_data = {
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR};
        instances_data.data.deviceAddress = instance_buf_addr;

        VkAccelerationStructureGeometryKHR tlas_geo = {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
        tlas_geo.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
        tlas_geo.geometry.instances = instances_data;

        VkAccelerationStructureBuildGeometryInfoKHR tlas_build_info = {
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
        tlas_build_info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
                                VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_NV;
        tlas_build_info.geometryCount = 1;
        tlas_build_info.pGeometries = &tlas_geo;
        tlas_build_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        tlas_build_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        tlas_build_info.srcAccelerationStructure = VK_NULL_HANDLE;

        const uint32_t instance_count = uint32_t(tlas_instances.size());
        const uint32_t max_instance_count = REN_MAX_RT_OBJ_INSTANCES; // allocate for worst case

        VkAccelerationStructureBuildSizesInfoKHR size_info = {
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
        vkGetAccelerationStructureBuildSizesKHR(api_ctx->device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                                &tlas_build_info, &max_instance_count, &size_info);

        scene_data_.persistent_data.rt_tlas_buf =
            ren_ctx_.LoadBuffer("TLAS Buf", Ren::eBufType::AccStructure, uint32_t(size_info.accelerationStructureSize));

        VkAccelerationStructureCreateInfoKHR create_info = {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
        create_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        create_info.buffer = scene_data_.persistent_data.rt_tlas_buf->vk_handle();
        create_info.offset = 0;
        create_info.size = size_info.accelerationStructureSize;

        VkAccelerationStructureKHR tlas_handle;
        VkResult res = vkCreateAccelerationStructureKHR(api_ctx->device, &create_info, nullptr, &tlas_handle);
        if (res != VK_SUCCESS) {
            ren_ctx_.log()->Error("[SceneManager::InitHWAccStructures]: Failed to create acceleration structure!");
        }

        Ren::BufferRef tlas_scratch_buf =
            ren_ctx_.LoadBuffer("TLAS Scratch Buf", Ren::eBufType::Storage, uint32_t(size_info.buildScratchSize));
        VkDeviceAddress tlas_scratch_buf_addr = tlas_scratch_buf->vk_device_address();

        tlas_build_info.srcAccelerationStructure = VK_NULL_HANDLE;
        tlas_build_info.dstAccelerationStructure = tlas_handle;
        tlas_build_info.scratchData.deviceAddress = tlas_scratch_buf_addr;

        VkAccelerationStructureBuildRangeInfoKHR range_info = {};
        range_info.primitiveOffset = 0;
        range_info.primitiveCount = 0;
        // instance_count;
        range_info.firstVertex = 0;
        range_info.transformOffset = 0;

        const VkAccelerationStructureBuildRangeInfoKHR *build_range = &range_info;
        vkCmdBuildAccelerationStructuresKHR(cmd_buf, 1, &tlas_build_info, &build_range);

        std::unique_ptr<Ren::AccStructureVK> vk_tlas(new Ren::AccStructureVK);
        if (!vk_tlas->Init(api_ctx, tlas_handle)) {
            ren_ctx_.log()->Error("[SceneManager::InitHWAccStructures]: Failed to init TLAS!");
        }
        scene_data_.persistent_data.rt_tlas = std::move(vk_tlas);

        scene_data_.persistent_data.rt_tlas_build_scratch_size = uint32_t(size_info.buildScratchSize);
    }

    Ren::EndSingleTimeCommands(api_ctx->device, api_ctx->graphics_queue, cmd_buf, api_ctx->temp_command_pool);
#endif

#endif
}

uint32_t SceneManager::PreprocessPrims_SAH(Ren::Span<const prim_t> prims, const split_settings_t &s,
                                           std::vector<gpu_bvh_node_t> &out_nodes, std::vector<uint32_t> &out_indices) {
    struct prims_coll_t {
        std::vector<uint32_t> indices;
        Ren::Vec3f min = Ren::Vec3f{std::numeric_limits<float>::max()},
                   max = Ren::Vec3f{std::numeric_limits<float>::lowest()};
        prims_coll_t() {}
        prims_coll_t(std::vector<uint32_t> &&_indices, const Ren::Vec3f &_min, const Ren::Vec3f &_max)
            : indices(std::move(_indices)), min(_min), max(_max) {}
    };

    std::deque<prims_coll_t> prim_lists;
    prim_lists.emplace_back();

    size_t num_nodes = out_nodes.size();
    const auto root_node_index = uint32_t(num_nodes);

    for (uint32_t j = 0; j < uint32_t(prims.size()); j++) {
        prim_lists.back().indices.push_back(j);
        prim_lists.back().min = Min(prim_lists.back().min, prims[j].bbox_min);
        prim_lists.back().max = Max(prim_lists.back().max, prims[j].bbox_max);
    }

    Ren::Vec3f root_min = prim_lists.back().min, root_max = prim_lists.back().max;

    while (!prim_lists.empty()) {
        split_data_t split_data = SplitPrimitives_SAH(prims.data(), prim_lists.back().indices, prim_lists.back().min,
                                                      prim_lists.back().max, root_min, root_max, s);
        prim_lists.pop_back();

        if (split_data.right_indices.empty()) {
            Ren::Vec3f bbox_min = split_data.left_bounds[0], bbox_max = split_data.left_bounds[1];

            out_nodes.emplace_back();
            gpu_bvh_node_t &n = out_nodes.back();

            n.prim_index = LEAF_NODE_BIT + uint32_t(out_indices.size());
            n.prim_count = uint32_t(split_data.left_indices.size());
            memcpy(&n.bbox_min[0], &bbox_min[0], 3 * sizeof(float));
            memcpy(&n.bbox_max[0], &bbox_max[0], 3 * sizeof(float));
            out_indices.insert(out_indices.end(), split_data.left_indices.begin(), split_data.left_indices.end());
        } else {
            const auto index = uint32_t(num_nodes);

            uint32_t space_axis = 0;
            const Ren::Vec3f c_left = (split_data.left_bounds[0] + split_data.left_bounds[1]) / 2.0f,
                             c_right = (split_data.right_bounds[0] + split_data.right_bounds[1]) / 2.0f;

            const Ren::Vec3f dist = Abs(c_left - c_right);

            if (dist[0] > dist[1] && dist[0] > dist[2]) {
                space_axis = 0;
            } else if (dist[1] > dist[0] && dist[1] > dist[2]) {
                space_axis = 1;
            } else {
                space_axis = 2;
            }

            const Ren::Vec3f bbox_min = Min(split_data.left_bounds[0], split_data.right_bounds[0]),
                             bbox_max = Max(split_data.left_bounds[1], split_data.right_bounds[1]);

            out_nodes.emplace_back();
            gpu_bvh_node_t &n = out_nodes.back();
            n.left_child = index + 1;
            n.right_child = (space_axis << 30) + index + 2;
            memcpy(&n.bbox_min[0], &bbox_min[0], 3 * sizeof(float));
            memcpy(&n.bbox_max[0], &bbox_max[0], 3 * sizeof(float));
            prim_lists.emplace_front(std::move(split_data.left_indices), split_data.left_bounds[0],
                                     split_data.left_bounds[1]);
            prim_lists.emplace_front(std::move(split_data.right_indices), split_data.right_bounds[0],
                                     split_data.right_bounds[1]);

            num_nodes += 2;
        }
    }

    return uint32_t(out_nodes.size() - root_node_index);
}