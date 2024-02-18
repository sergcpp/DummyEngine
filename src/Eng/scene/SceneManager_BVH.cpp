#include "SceneManager.h"

#include <deque>

#include <Ren/Context.h>
#include <Sys/BinaryTree.h>
#include <Sys/MonoAlloc.h>

#include <optick/optick.h>
#include <vtune/ittnotify.h>
extern __itt_domain *__g_itt_domain;

#include "../renderer/Renderer_Structs.h"
#include "../utils/BVHSplit.h"

namespace SceneManagerInternal {
const float BoundsMargin = 0.2f;

float surface_area(const Eng::bvh_node_t &n) {
    const Ren::Vec3f d = n.bbox_max - n.bbox_min;
    return d[0] * d[1] + d[0] * d[2] + d[1] * d[2];
}

float surface_area_of_union(const Eng::bvh_node_t &n1, const Eng::bvh_node_t &n2) {
    const Ren::Vec3f d = Max(n1.bbox_max, n2.bbox_max) - Min(n1.bbox_min, n2.bbox_max);
    return d[0] * d[1] + d[0] * d[2] + d[1] * d[2];
}

struct insert_candidate_t {
    uint32_t node_index;
    float direct_cost;
};

bool insert_candidate_compare(insert_candidate_t c1, insert_candidate_t c2) { return c1.direct_cost > c2.direct_cost; }

void sort_children(const Eng::bvh_node_t *nodes, Eng::bvh_node_t &node) {
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

void update_bbox(const Eng::bvh_node_t *nodes, Eng::bvh_node_t &node) {
    node.bbox_min = Min(nodes[node.left_child].bbox_min, nodes[node.right_child].bbox_min);
    node.bbox_max = Max(nodes[node.left_child].bbox_max, nodes[node.right_child].bbox_max);
}

__itt_string_handle *itt_rebuild_bvh_str = __itt_string_handle_create("SceneManager::RebuildSceneBVH");
__itt_string_handle *itt_update_bvh_str = __itt_string_handle_create("SceneManager::UpdateBVH");
} // namespace SceneManagerInternal

void Eng::SceneManager::RebuildSceneBVH() {
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

    if (primitives.empty()) {
        return;
    }

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

    split_settings_t s;
    s.oversplit_threshold = std::numeric_limits<float>::max();
    s.node_traversal_cost = 0.0f;

    while (!prim_lists.empty()) {
        split_data_t split_data = SplitPrimitives_SAH(&primitives[0], prim_lists.back().indices, prim_lists.back().min,
                                                      prim_lists.back().max, s);
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

void Eng::SceneManager::RemoveNode(const uint32_t node_index) {
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

void Eng::SceneManager::UpdateObjects() {
    using namespace SceneManagerInternal;

    OPTICK_EVENT("SceneManager::UpdateObjects");
    __itt_task_begin(__g_itt_domain, __itt_null, __itt_null, itt_update_bvh_str);

    const auto *physes = (Physics *)scene_data_.comp_store[CompPhysics]->SequentialData();
    auto *transforms = (Transform *)scene_data_.comp_store[CompTransform]->SequentialData();

    scene_data_.update_counter++;

    // instance_data_to_update_.clear();

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

            instance_data_to_update_.push_back(obj_index);
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
                    const float cost_before = surface_area(nodes[par_node.right_child]);

                    const float rotation_costs[2] = {
                        surface_area_of_union(left_child_of(par_node), right_child_of(right_child_of(par_node))),
                        surface_area_of_union(left_child_of(par_node), left_child_of(right_child_of(par_node)))};

                    const int best_rot = rotation_costs[0] < rotation_costs[1] ? 0 : 1;
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

void Eng::SceneManager::InitSWRTAccStructures() {
    using namespace SceneManagerInternal;

    std::vector<gpu_bvh_node_t> nodes;
    std::vector<gpu_mesh_t> meshes;
    std::vector<gpu_mesh_instance_t> mesh_instances;
    std::vector<uint32_t> prim_indices;

    std::vector<prim_t> temp_primitives;
    std::vector<uint32_t> temp_indices;

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

        assert((attribs.sub.offset % 16) == 0);
        const uint32_t first_vertex = attribs.sub.offset / 16;
        assert((indices.sub.offset % (3 * sizeof(uint32_t))) == 0);
        const uint32_t prim_offset = indices.sub.offset / (3 * sizeof(uint32_t));

        assert(acc->mesh->type() == Ren::eMeshType::Simple);
        const int VertexStride = 13;

        const uint32_t mesh_index = uint32_t(meshes.size());

        meshes.emplace_back();
        auto &new_mesh = meshes.back();
        new_mesh.node_index = uint32_t(nodes.size());
        new_mesh.tris_index = uint32_t(prim_indices.size());
        new_mesh.geo_count = 0;

        //
        // Gather geometries
        //

        temp_primitives.clear();
        temp_indices.clear();

        const float *positions = reinterpret_cast<const float *>(acc->mesh->attribs());
        const uint32_t *tri_indices = reinterpret_cast<const uint32_t *>(acc->mesh->indices());

        for (const Ren::TriGroup &grp : acc->mesh->groups()) {
            const Ren::Material *mat = grp.mat.get();
            const uint32_t mat_flags = mat->flags();
            if ((mat_flags & uint32_t(Ren::eMatFlags::AlphaBlend)) != 0) {
                // Include only opaque surfaces
                continue;
            }

            const uint32_t index_beg = grp.offset / sizeof(uint32_t);
            const uint32_t index_end = index_beg + grp.num_indices;

            for (uint32_t i = index_beg; i < index_end; i += 3) {
                const uint32_t i0 = tri_indices[i + 0], i1 = tri_indices[i + 1], i2 = tri_indices[i + 2];

                const Ren::Vec3f p0 = Ren::MakeVec3(&positions[i0 * VertexStride]),
                                 p1 = Ren::MakeVec3(&positions[i1 * VertexStride]),
                                 p2 = Ren::MakeVec3(&positions[i2 * VertexStride]);

                const Ren::Vec3f bbox_min = Min(p0, Min(p1, p2)), bbox_max = Max(p0, Max(p1, p2));

                temp_primitives.push_back({i0, i1, i2, bbox_min, bbox_max});
                temp_indices.push_back(i / 3);
            }

            ++new_mesh.geo_count;
        }

        split_settings_t s;
        new_mesh.node_count = PreprocessPrims_SAH(temp_primitives, s, nodes, prim_indices);

        for (int i = new_mesh.tris_index; i < int(prim_indices.size()); ++i) {
            // prim_indices[i] += prim_offset;

            prim_indices[i] = prim_offset + temp_indices[prim_indices[i]];
        }

        new_mesh.tris_count = uint32_t(prim_indices.size()) - new_mesh.tris_index;
        new_mesh.vert_index = first_vertex;

        acc->mesh->blas = std::make_unique<Ren::AccStructureSW>(mesh_index);

        acc_index = scene_data_.comp_store[CompAccStructure]->Next(acc_index);
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

        auto &swrt_blas = static_cast<Ren::AccStructureSW &>(*acc.mesh->blas);
        swrt_blas.geo_index = uint32_t(geo_instances.size());
        swrt_blas.geo_count = 0;

        mesh_instances.emplace_back();
        auto &new_instance = mesh_instances.back();

        new_instance.bbox_min = tr.bbox_min;
        new_instance.bbox_max = tr.bbox_max;
        new_instance.mesh_index = swrt_blas.mesh_index;
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 4; ++j) {
                new_instance.inv_transform[i][j] = tr.object_from_world[j][i];
            }
        }

        const uint32_t indices_start = acc.mesh->indices_buf().sub.offset;
        for (const Ren::TriGroup &grp : acc.mesh->groups()) {
            const Ren::Material *mat = grp.mat.get();
            const uint32_t mat_flags = mat->flags();
            if ((mat_flags & uint32_t(Ren::eMatFlags::AlphaBlend)) != 0) {
                // Include only opaque surfaces
                continue;
            }

            ++swrt_blas.geo_count;

            geo_instances.emplace_back();
            auto &geo = geo_instances.back();
            geo.indices_start = (indices_start + grp.offset) / sizeof(uint32_t);
            geo.vertices_start = acc.mesh->attribs_buf1().sub.offset / 16;
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

    const uint32_t total_nodes_size = uint32_t(nodes.size() * sizeof(gpu_bvh_node_t));
    const uint32_t total_prim_indices_size = uint32_t(prim_indices.size() * sizeof(uint32_t));
    const uint32_t total_meshes_size = uint32_t(meshes.size() * sizeof(gpu_mesh_t));
    const uint32_t total_mesh_instances_size = uint32_t(mesh_instances.size() * sizeof(gpu_mesh_instance_t));
    const uint32_t total_geo_instances_size = uint32_t(geo_instances.size() * sizeof(RTGeoInstance));

    if (!total_nodes_size) {
        scene_data_.persistent_data.rt_blas_buf = {};
        scene_data_.persistent_data.swrt.rt_prim_indices_buf = {};
        scene_data_.persistent_data.swrt.rt_meshes_buf = {};
        scene_data_.persistent_data.rt_instance_buf = {};
        scene_data_.persistent_data.rt_geo_data_buf = {};
        return;
    }

    Ren::ApiContext *api_ctx = ren_ctx_.api_ctx();

    Ren::Buffer rt_blas_stage_buf("SWRT BLAS Stage Buf", api_ctx, Ren::eBufType::Stage, total_nodes_size);
    {
        uint8_t *rt_blas_stage = rt_blas_stage_buf.Map(Ren::eBufMap::Write);
        memcpy(rt_blas_stage, nodes.data(), total_nodes_size);
        rt_blas_stage_buf.Unmap();
    }

    Ren::Buffer rt_prim_indices_stage_buf("SWRT Prim Indices Stage Buf", api_ctx, Ren::eBufType::Stage,
                                          total_prim_indices_size);
    {
        uint8_t *rt_prim_indices_stage = rt_prim_indices_stage_buf.Map(Ren::eBufMap::Write);
        memcpy(rt_prim_indices_stage, prim_indices.data(), total_prim_indices_size);
        rt_prim_indices_stage_buf.Unmap();
    }

    Ren::Buffer rt_meshes_stage_buf("SWRT Meshes Stage Buf", api_ctx, Ren::eBufType::Stage, total_meshes_size);
    {
        uint8_t *rt_meshes_stage = rt_meshes_stage_buf.Map(Ren::eBufMap::Write);
        memcpy(rt_meshes_stage, meshes.data(), total_meshes_size);
        rt_meshes_stage_buf.Unmap();
    }

    Ren::Buffer rt_mesh_instances_stage_buf("SWRT Mesh Instances Stage Buf", api_ctx, Ren::eBufType::Stage,
                                            total_mesh_instances_size);
    {
        uint8_t *rt_mesh_instances_stage = rt_mesh_instances_stage_buf.Map(Ren::eBufMap::Write);
        memcpy(rt_mesh_instances_stage, mesh_instances.data(), total_mesh_instances_size);
        rt_mesh_instances_stage_buf.Unmap();
    }

    Ren::Buffer geo_data_stage_buf("SWRT Geo Data Stage Buf", api_ctx, Ren::eBufType::Stage, total_geo_instances_size);
    {
        uint8_t *geo_data_stage = geo_data_stage_buf.Map(Ren::eBufMap::Write);
        memcpy(geo_data_stage, geo_instances.data(), total_geo_instances_size);
        geo_data_stage_buf.Unmap();
    }

    scene_data_.persistent_data.rt_blas_buf =
        ren_ctx_.LoadBuffer("SWRT BLAS Buf", Ren::eBufType::Storage, total_nodes_size);
    scene_data_.persistent_data.swrt.rt_prim_indices_buf =
        ren_ctx_.LoadBuffer("SWRT Prim Indices Buf", Ren::eBufType::Texture, total_prim_indices_size);
    scene_data_.persistent_data.swrt.rt_meshes_buf =
        ren_ctx_.LoadBuffer("SWRT Meshes Buf", Ren::eBufType::Storage, total_meshes_size);
    scene_data_.persistent_data.rt_instance_buf =
        ren_ctx_.LoadBuffer("SWRT Mesh Instances Buf", Ren::eBufType::Storage, total_mesh_instances_size);
    scene_data_.persistent_data.rt_geo_data_buf =
        ren_ctx_.LoadBuffer("SWRT Geo Data Buf", Ren::eBufType::Storage, total_geo_instances_size);

    const uint32_t max_nodes_count = MAX_RT_TLAS_NODES;
    scene_data_.persistent_data.rt_tlas_buf =
        ren_ctx_.LoadBuffer("TLAS Buf", Ren::eBufType::Storage, uint32_t(max_nodes_count * sizeof(gpu_bvh_node_t)));
    scene_data_.persistent_data.rt_sh_tlas_buf = ren_ctx_.LoadBuffer(
        "TLAS Shadow Buf", Ren::eBufType::Storage, uint32_t(max_nodes_count * sizeof(gpu_bvh_node_t)));

#if defined(USE_VK_RENDER)
    VkCommandBuffer cmd_buf = api_ctx->BegSingleTimeCommands();
#else
    void *cmd_buf = nullptr;
#endif

    Ren::CopyBufferToBuffer(rt_blas_stage_buf, 0, *scene_data_.persistent_data.rt_blas_buf, 0, total_nodes_size,
                            cmd_buf);
    Ren::CopyBufferToBuffer(rt_prim_indices_stage_buf, 0, *scene_data_.persistent_data.swrt.rt_prim_indices_buf, 0,
                            total_prim_indices_size, cmd_buf);
    Ren::CopyBufferToBuffer(rt_meshes_stage_buf, 0, *scene_data_.persistent_data.swrt.rt_meshes_buf, 0,
                            total_meshes_size, cmd_buf);
    Ren::CopyBufferToBuffer(rt_mesh_instances_stage_buf, 0, *scene_data_.persistent_data.rt_instance_buf, 0,
                            total_mesh_instances_size, cmd_buf);
    Ren::CopyBufferToBuffer(geo_data_stage_buf, 0, *scene_data_.persistent_data.rt_geo_data_buf, 0,
                            total_geo_instances_size, cmd_buf);

#if defined(USE_VK_RENDER)
    api_ctx->EndSingleTimeCommands(cmd_buf);
#endif
}

uint32_t Eng::SceneManager::PreprocessPrims_SAH(Ren::Span<const prim_t> prims, const split_settings_t &s,
                                                std::vector<gpu_bvh_node_t> &out_nodes,
                                                std::vector<uint32_t> &out_indices) {
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

    while (!prim_lists.empty()) {
        split_data_t split_data = SplitPrimitives_SAH(prims.data(), prim_lists.back().indices, prim_lists.back().min,
                                                      prim_lists.back().max, s);
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