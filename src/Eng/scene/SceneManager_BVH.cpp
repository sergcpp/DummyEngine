#include "SceneManager.h"

#include <deque>

#include <Phy/BVHSplit.h>
#include <Ren/Context.h>
#include <Sys/BinaryTree.h>
#include <Sys/MonoAlloc.h>

#include <optick/optick.h>
#include <vtune/ittnotify.h>
extern __itt_domain *__g_itt_domain;

#include "../renderer/Renderer_Structs.h"

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

uint16_t encode_snorm_u16(const float f) {
    return uint16_t(std::round(Ren::Clamp((f + 1) / 2.0f, 0.0f, 1.0f) * 65535.0f));
}

uint32_t encode_cosines(const float cos_a, const float cos_b) {
    const uint32_t a = uint32_t(std::floor(65535.0f * ((cos_a + 1.0f) / 2.0f)));
    const uint32_t b = uint32_t(std::floor(65535.0f * ((cos_b + 1.0f) / 2.0f)));
    return (a << 16) | b;
}

uint32_t EncodeOctDir(const Ren::Vec3f &d) {
    const float denom = fabsf(d[0]) + fabsf(d[1]) + fabsf(d[2]);
    const float v[3] = {d[0] / denom, d[1] / denom, d[2] / denom};
    if (v[2] < 0.0f) {
        const uint16_t x = encode_snorm_u16((1.0f - fabsf(v[1])) * copysignf(1.0f, v[0]));
        const uint16_t y = encode_snorm_u16((1.0f - fabsf(v[0])) * copysignf(1.0f, v[1]));
        return (uint32_t(x) << 16) | y;
    }
    const uint16_t x = encode_snorm_u16(v[0]);
    const uint16_t y = encode_snorm_u16(v[1]);
    return (uint32_t(x) << 16) | y;
}

Phy::Vec3f adapt(const Ren::Vec3f &v) { return Phy::Vec3f{v[0], v[1], v[2]}; }

__itt_string_handle *itt_rebuild_bvh_str = __itt_string_handle_create("SceneManager::RebuildSceneBVH");
__itt_string_handle *itt_update_bvh_str = __itt_string_handle_create("SceneManager::UpdateBVH");
} // namespace SceneManagerInternal

void Eng::SceneManager::RebuildSceneBVH() {
    using namespace SceneManagerInternal;

    __itt_task_begin(__g_itt_domain, __itt_null, __itt_null, itt_rebuild_bvh_str);

    auto *transforms = (Transform *)scene_data_.comp_store[CompTransform]->SequentialData();

    std::vector<Phy::prim_t> primitives;
    primitives.reserve(scene_data_.objects.size());

    for (const SceneObject &obj : scene_data_.objects) {
        if (obj.comp_mask & CompTransformBit) {
            const Transform &tr = transforms[obj.components[CompTransform]];
            const Ren::Vec3f d = tr.bbox_max_ws - tr.bbox_min_ws;
            primitives.push_back(
                {0, 0, 0, adapt(tr.bbox_min_ws - BoundsMargin * d), adapt(tr.bbox_max_ws + BoundsMargin * d)});
        }
    }

    scene_data_.nodes.clear();
    scene_data_.root_node = 0xffffffff;

    if (primitives.empty()) {
        return;
    }

    struct prims_coll_t {
        std::vector<uint32_t> indices;
        Phy::Vec3f min = Phy::Vec3f{std::numeric_limits<float>::max()},
                   max = Phy::Vec3f{std::numeric_limits<float>::lowest()};
        prims_coll_t() = default;
        prims_coll_t(std::vector<uint32_t> &&_indices, const Phy::Vec3f &_min, const Phy::Vec3f &_max)
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

    Phy::split_settings_t s;
    s.oversplit_threshold = std::numeric_limits<float>::max();
    s.min_primitives_in_leaf = 1;

    while (!prim_lists.empty()) {
        Phy::split_data_t split_data = SplitPrimitives_SAH(&primitives[0], prim_lists.back().indices,
                                                           prim_lists.back().min, prim_lists.back().max, s);
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
            const Phy::Vec3f bbox_min = split_data.left_bounds[0], bbox_max = split_data.left_bounds[1];

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

            const Phy::Vec3f c_left = (split_data.left_bounds[0] + split_data.left_bounds[1]) / 2.0f,
                             c_right = (split_data.right_bounds[0] + split_data.right_bounds[1]) / 2.0f;

            const Phy::Vec3f dist = Abs(c_left - c_right);

            const uint32_t space_axis =
                (dist[0] > dist[1] && dist[0] > dist[2]) ? 0 : ((dist[1] > dist[0] && dist[1] > dist[2]) ? 1 : 2);

            const Phy::Vec3f bbox_min = Min(split_data.left_bounds[0], split_data.right_bounds[0]),
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

std::unique_ptr<Ren::IAccStructure> Eng::SceneManager::Build_SWRT_BLAS(const AccStructure &acc) {
    using namespace SceneManagerInternal;

    Ren::ApiContext *api_ctx = ren_ctx_.api_ctx();

    const Ren::BufferRange &attribs = acc.mesh->attribs_buf1(), &indices = acc.mesh->indices_buf();

    assert((attribs.sub.offset % 16) == 0);
    const uint32_t first_vertex = attribs.sub.offset / 16;
    assert((indices.sub.offset % (3 * sizeof(uint32_t))) == 0);
    const uint32_t prim_offset = indices.sub.offset / (3 * sizeof(uint32_t));

    assert(acc.mesh->type() == Ren::eMeshType::Simple);
    const int VertexStride = 13;

    if (!scene_data_.persistent_data.swrt.rt_meshes_buf) {
        scene_data_.persistent_data.swrt.rt_meshes_buf =
            ren_ctx_.LoadBuffer("SWRT Meshes Buf", Ren::eBufType::Storage, 16 * sizeof(gpu_mesh_t), sizeof(gpu_mesh_t));
    }
    if (!scene_data_.persistent_data.swrt.rt_prim_indices_buf) {
        scene_data_.persistent_data.swrt.rt_prim_indices_buf = ren_ctx_.LoadBuffer(
            "SWRT Prim Indices Buf", Ren::eBufType::Texture, 1024 * sizeof(uint32_t), sizeof(uint32_t));
    }
    if (!scene_data_.persistent_data.swrt.rt_blas_buf) {
        scene_data_.persistent_data.swrt.rt_blas_buf = ren_ctx_.LoadBuffer(
            "SWRT BLAS Buf", Ren::eBufType::Storage, 1024 * sizeof(gpu_bvh_node_t), sizeof(gpu_bvh_node_t));
    }

    const Ren::SubAllocation mesh_alloc =
        scene_data_.persistent_data.swrt.rt_meshes_buf->AllocSubRegion(sizeof(gpu_mesh_t), nullptr);

    //
    // Gather geometries
    //

    gpu_mesh_t new_mesh = {};

    std::vector<Phy::prim_t> temp_primitives;
    std::vector<uint32_t> temp_indices;

    Ren::Span<const float> positions = acc.mesh->attribs();
    Ren::Span<const uint32_t> tri_indices = acc.mesh->indices();

    for (const Ren::TriGroup &grp : acc.mesh->groups()) {
        const Ren::Material *mat = grp.front_mat.get();
        const Ren::Bitmask<Ren::eMatFlags> mat_flags = mat->flags();
        if (mat_flags & Ren::eMatFlags::AlphaBlend) {
            // Include only opaque surfaces
            continue;
        }

        const uint32_t index_beg = grp.byte_offset / sizeof(uint32_t);
        const uint32_t index_end = index_beg + grp.num_indices;

        for (uint32_t i = index_beg; i < index_end; i += 3) {
            const uint32_t i0 = tri_indices[i + 0], i1 = tri_indices[i + 1], i2 = tri_indices[i + 2];

            const Phy::Vec3f p0 = Phy::MakeVec3(&positions[i0 * VertexStride]),
                             p1 = Phy::MakeVec3(&positions[i1 * VertexStride]),
                             p2 = Phy::MakeVec3(&positions[i2 * VertexStride]);

            const Phy::Vec3f bbox_min = Min(p0, Min(p1, p2)), bbox_max = Max(p0, Max(p1, p2));

            temp_primitives.push_back({i0, i1, i2, bbox_min, bbox_max});
            temp_indices.push_back(i / 3);
        }

        ++new_mesh.geo_count;
    }

    std::vector<gpu_bvh_node_t> nodes;
    std::vector<uint32_t> prim_indices;

    Phy::split_settings_t s;
    PreprocessPrims_SAH(temp_primitives, s, nodes, prim_indices);

    const Ren::SubAllocation nodes_alloc = scene_data_.persistent_data.swrt.rt_blas_buf->AllocSubRegion(
        uint32_t(nodes.size()) * sizeof(gpu_bvh_node_t), nullptr);
    const Ren::SubAllocation prim_alloc = scene_data_.persistent_data.swrt.rt_prim_indices_buf->AllocSubRegion(
        uint32_t(prim_indices.size()) * sizeof(uint32_t), nullptr);

    new_mesh.node_index = nodes_alloc.offset / sizeof(gpu_bvh_node_t);
    new_mesh.node_count = uint32_t(nodes.size());
    new_mesh.tris_index = prim_alloc.offset / sizeof(uint32_t);
    new_mesh.tris_count = uint32_t(prim_indices.size());
    new_mesh.vert_index = first_vertex;

    // offset node indices
    for (int i = 0; i < int(nodes.size()); ++i) {
        gpu_bvh_node_t &n = nodes[i];
        if (n.prim_index & LEAF_NODE_BIT) {
            n.prim_index += new_mesh.tris_index;
        } else {
            n.left_child += new_mesh.node_index;
            n.right_child += new_mesh.node_index;
        }
    }

    for (int i = 0; i < int(prim_indices.size()); ++i) {
        prim_indices[i] = prim_offset + temp_indices[prim_indices[i]];
    }

    auto new_blas = std::make_unique<Ren::AccStructureSW>(mesh_alloc, nodes_alloc, prim_alloc);

    const uint32_t total_nodes_size = uint32_t(nodes.size() * sizeof(gpu_bvh_node_t));
    const uint32_t total_prim_indices_size = uint32_t(prim_indices.size() * sizeof(uint32_t));

    Ren::Buffer rt_meshes_stage_buf("SWRT Meshes Upload Buf", api_ctx, Ren::eBufType::Upload, sizeof(gpu_mesh_t));
    {
        uint8_t *rt_meshes_stage = rt_meshes_stage_buf.Map();
        memcpy(rt_meshes_stage, &new_mesh, sizeof(gpu_mesh_t));
        rt_meshes_stage_buf.Unmap();
    }

    Ren::Buffer rt_blas_stage_buf("SWRT BLAS Upload Buf", api_ctx, Ren::eBufType::Upload, total_nodes_size);
    {
        uint8_t *rt_blas_stage = rt_blas_stage_buf.Map();
        memcpy(rt_blas_stage, nodes.data(), total_nodes_size);
        rt_blas_stage_buf.Unmap();
    }

    Ren::Buffer rt_prim_indices_stage_buf("SWRT Prim Indices Upload Buf", api_ctx, Ren::eBufType::Upload,
                                          total_prim_indices_size);
    {
        uint8_t *rt_prim_indices_stage = rt_prim_indices_stage_buf.Map();
        memcpy(rt_prim_indices_stage, prim_indices.data(), total_prim_indices_size);
        rt_prim_indices_stage_buf.Unmap();
    }

    Ren::CommandBuffer cmd_buf = ren_ctx_.BegTempSingleTimeCommands();

    Ren::CopyBufferToBuffer(rt_meshes_stage_buf, 0, *scene_data_.persistent_data.swrt.rt_meshes_buf, mesh_alloc.offset,
                            sizeof(gpu_mesh_t), cmd_buf);
    Ren::CopyBufferToBuffer(rt_blas_stage_buf, 0, *scene_data_.persistent_data.swrt.rt_blas_buf, nodes_alloc.offset,
                            total_nodes_size, cmd_buf);
    Ren::CopyBufferToBuffer(rt_prim_indices_stage_buf, 0, *scene_data_.persistent_data.swrt.rt_prim_indices_buf,
                            prim_alloc.offset, total_prim_indices_size, cmd_buf);

    ren_ctx_.EndTempSingleTimeCommands(cmd_buf);

    rt_meshes_stage_buf.FreeImmediate();
    rt_blas_stage_buf.FreeImmediate();
    rt_prim_indices_stage_buf.FreeImmediate();

    return new_blas;
}

void Eng::SceneManager::Alloc_SWRT_TLAS() {
    scene_data_.persistent_data.rt_tlas_buf =
        ren_ctx_.LoadBuffer("TLAS Buf", Ren::eBufType::Storage, uint32_t(MAX_RT_TLAS_NODES * sizeof(gpu_bvh_node_t)));
    scene_data_.persistent_data.rt_sh_tlas_buf = ren_ctx_.LoadBuffer(
        "TLAS Shadow Buf", Ren::eBufType::Storage, uint32_t(MAX_RT_TLAS_NODES * sizeof(gpu_bvh_node_t)));
}

uint32_t Eng::SceneManager::PreprocessPrims_SAH(Ren::Span<const Phy::prim_t> prims, const Phy::split_settings_t &s,
                                                std::vector<gpu_bvh_node_t> &out_nodes,
                                                std::vector<uint32_t> &out_indices) {
    struct prims_coll_t {
        std::vector<uint32_t> indices;
        Phy::Vec3f min = Phy::Vec3f{std::numeric_limits<float>::max()},
                   max = Phy::Vec3f{std::numeric_limits<float>::lowest()};
        prims_coll_t() {}
        prims_coll_t(std::vector<uint32_t> &&_indices, const Phy::Vec3f &_min, const Phy::Vec3f &_max)
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
        Phy::split_data_t split_data = SplitPrimitives_SAH(prims.data(), prim_lists.back().indices,
                                                           prim_lists.back().min, prim_lists.back().max, s);
        prim_lists.pop_back();

        if (split_data.right_indices.empty()) {
            Phy::Vec3f bbox_min = split_data.left_bounds[0], bbox_max = split_data.left_bounds[1];

            gpu_bvh_node_t &n = out_nodes.emplace_back();

            n.prim_index = LEAF_NODE_BIT + uint32_t(out_indices.size());
            n.prim_count = uint32_t(split_data.left_indices.size());
            memcpy(&n.bbox_min[0], &bbox_min[0], 3 * sizeof(float));
            memcpy(&n.bbox_max[0], &bbox_max[0], 3 * sizeof(float));
            out_indices.insert(out_indices.end(), split_data.left_indices.begin(), split_data.left_indices.end());
        } else {
            const auto index = uint32_t(num_nodes);

            uint32_t space_axis = 0;
            const Phy::Vec3f c_left = (split_data.left_bounds[0] + split_data.left_bounds[1]) / 2.0f,
                             c_right = (split_data.right_bounds[0] + split_data.right_bounds[1]) / 2.0f;

            const Phy::Vec3f dist = Abs(c_left - c_right);

            if (dist[0] > dist[1] && dist[0] > dist[2]) {
                space_axis = 0;
            } else if (dist[1] > dist[0] && dist[1] > dist[2]) {
                space_axis = 1;
            } else {
                space_axis = 2;
            }

            const Phy::Vec3f bbox_min = Min(split_data.left_bounds[0], split_data.right_bounds[0]),
                             bbox_max = Max(split_data.left_bounds[1], split_data.right_bounds[1]);

            gpu_bvh_node_t &n = out_nodes.emplace_back();
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

uint32_t Eng::SceneManager::FlattenBVH_r(Ren::Span<const gpu_light_bvh_node_t> nodes, const uint32_t node_index,
                                         const uint32_t parent_index, std::vector<gpu_light_wbvh_node_t> &out_nodes) {
    using namespace SceneManagerInternal;

    const gpu_light_bvh_node_t &cur_node = nodes[node_index];

    // allocate new node
    const auto new_node_index = uint32_t(out_nodes.size());
    out_nodes.emplace_back();

    if (cur_node.prim_index & LEAF_NODE_BIT) {
        gpu_light_wbvh_node_t &new_node = out_nodes[new_node_index];

        new_node.bbox_min[0][0] = cur_node.bbox_min[0];
        new_node.bbox_min[1][0] = cur_node.bbox_min[1];
        new_node.bbox_min[2][0] = cur_node.bbox_min[2];

        new_node.bbox_max[0][0] = cur_node.bbox_max[0];
        new_node.bbox_max[1][0] = cur_node.bbox_max[1];
        new_node.bbox_max[2][0] = cur_node.bbox_max[2];

        new_node.child[0] = cur_node.prim_index;
        new_node.child[1] = cur_node.prim_count;

        new_node.flux[0] = cur_node.flux;
        new_node.axis[0] = EncodeOctDir(cur_node.axis);
        new_node.cos_omega_ne[0] = encode_cosines(cosf(cur_node.omega_n), cosf(cur_node.omega_e));

        return new_node_index;
    }

    // Gather children 2 levels deep

    uint32_t children[8];
    int children_count = 0;

    const gpu_light_bvh_node_t &child0 = nodes[cur_node.left_child];

    if (child0.prim_index & LEAF_NODE_BIT) {
        children[children_count++] = cur_node.left_child & LEFT_CHILD_BITS;
    } else {
        const gpu_light_bvh_node_t &child00 = nodes[child0.left_child];
        const gpu_light_bvh_node_t &child01 = nodes[child0.right_child & RIGHT_CHILD_BITS];

        if (child00.prim_index & LEAF_NODE_BIT) {
            children[children_count++] = child0.left_child & LEFT_CHILD_BITS;
        } else {
            children[children_count++] = child00.left_child;
            children[children_count++] = child00.right_child & RIGHT_CHILD_BITS;
        }

        if (child01.prim_index & LEAF_NODE_BIT) {
            children[children_count++] = child0.right_child & RIGHT_CHILD_BITS;
        } else {
            children[children_count++] = child01.left_child;
            children[children_count++] = child01.right_child & RIGHT_CHILD_BITS;
        }
    }

    const gpu_light_bvh_node_t &child1 = nodes[cur_node.right_child & RIGHT_CHILD_BITS];

    if (child1.prim_index & LEAF_NODE_BIT) {
        children[children_count++] = cur_node.right_child & RIGHT_CHILD_BITS;
    } else {
        const gpu_light_bvh_node_t &child10 = nodes[child1.left_child];
        const gpu_light_bvh_node_t &child11 = nodes[child1.right_child & RIGHT_CHILD_BITS];

        if (child10.prim_index & LEAF_NODE_BIT) {
            children[children_count++] = child1.left_child & LEFT_CHILD_BITS;
        } else {
            children[children_count++] = child10.left_child;
            children[children_count++] = child10.right_child & RIGHT_CHILD_BITS;
        }

        if (child11.prim_index & LEAF_NODE_BIT) {
            children[children_count++] = child1.right_child & RIGHT_CHILD_BITS;
        } else {
            children[children_count++] = child11.left_child;
            children[children_count++] = child11.right_child & RIGHT_CHILD_BITS;
        }
    }

    // Sort children in morton order
    Ren::Vec3f children_centers[8], whole_box_min = {FLT_MAX}, whole_box_max = {-FLT_MAX};
    for (int i = 0; i < children_count; i++) {
        children_centers[i] = 0.5f * (nodes[children[i]].bbox_min + nodes[children[i]].bbox_max);
        whole_box_min = Min(whole_box_min, children_centers[i]);
        whole_box_max = Max(whole_box_max, children_centers[i]);
    }

    whole_box_max += 0.001f;

    const Ren::Vec3f scale = 2.0f / (whole_box_max - whole_box_min);

    uint32_t sorted_children[8] = {0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
                                   0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff};
    for (int i = 0; i < children_count; i++) {
        Ren::Vec3f code = (children_centers[i] - whole_box_min) * scale;

        const auto x = uint32_t(code[0]), y = uint32_t(code[1]), z = uint32_t(code[2]);

        uint32_t mort = (z << 2) | (y << 1) | (x << 0);

        while (sorted_children[mort] != 0xffffffff) {
            mort = (mort + 1) % 8;
        }

        sorted_children[mort] = children[i];
    }

    uint32_t new_children[8];

    for (int i = 0; i < 8; i++) {
        if (sorted_children[i] != 0xffffffff) {
            new_children[i] = FlattenBVH_r(nodes, sorted_children[i], node_index, out_nodes);
        } else {
            new_children[i] = 0x7fffffff;
        }
    }

    gpu_light_wbvh_node_t &new_node = out_nodes[new_node_index];
    memcpy(new_node.child, new_children, sizeof(new_children));

    for (int i = 0; i < 8; i++) {
        if (new_children[i] != 0x7fffffff) {
            new_node.bbox_min[0][i] = nodes[sorted_children[i]].bbox_min[0];
            new_node.bbox_min[1][i] = nodes[sorted_children[i]].bbox_min[1];
            new_node.bbox_min[2][i] = nodes[sorted_children[i]].bbox_min[2];

            new_node.bbox_max[0][i] = nodes[sorted_children[i]].bbox_max[0];
            new_node.bbox_max[1][i] = nodes[sorted_children[i]].bbox_max[1];
            new_node.bbox_max[2][i] = nodes[sorted_children[i]].bbox_max[2];

            new_node.flux[i] = nodes[sorted_children[i]].flux;
            new_node.axis[i] = EncodeOctDir(nodes[sorted_children[i]].axis);
            new_node.cos_omega_ne[i] =
                encode_cosines(cosf(nodes[sorted_children[i]].omega_n), cosf(nodes[sorted_children[i]].omega_e));
        } else {
            // Init as invalid bounding box
            new_node.bbox_min[0][i] = new_node.bbox_min[1][i] = new_node.bbox_min[2][i] = 0.0f;
            new_node.bbox_max[0][i] = new_node.bbox_max[1][i] = new_node.bbox_max[2][i] = 0.0f;
            // Init as zero light
            new_node.flux[i] = 0.0f;
            new_node.axis[i] = 0;
            new_node.cos_omega_ne[i] = 0;
        }
    }

    return new_node_index;
}

void Eng::SceneManager::RebuildLightTree() {
    scene_data_.persistent_data.stoch_lights_buf = {};
    scene_data_.persistent_data.stoch_lights_nodes_buf = {};

    const auto *transforms = (Transform *)scene_data_.comp_store[CompTransform]->SequentialData();
    const auto *acc_structs = (AccStructure *)scene_data_.comp_store[CompAccStructure]->SequentialData();

    struct additional_data_t {
        Ren::Vec3f axis;
        float flux, omega_n, omega_e;
    };
    std::vector<additional_data_t> additional_data;

    std::vector<LightItem> stochastic_lights;
    std::vector<Phy::prim_t> temp_primitives;
    std::vector<uint32_t> temp_indices;

    const uint64_t RTCompMask = (CompTransformBit | CompAccStructureBit);
    for (int i = 0; i < int(scene_data_.objects.size()); ++i) {
        const SceneObject &obj = scene_data_.objects[i];
        if ((obj.comp_mask & RTCompMask) != RTCompMask) {
            continue;
        }

        const Transform &tr = transforms[obj.components[CompTransform]];
        const AccStructure &acc = acc_structs[obj.components[CompAccStructure]];

        assert(acc.mesh->type() == Ren::eMeshType::Simple);
        const int VertexStride = 13;

        Ren::Span<const float> positions = acc.mesh->attribs();
        Ren::Span<const uint32_t> tri_indices = acc.mesh->indices();
        const uint32_t indices_start = acc.mesh->indices_buf().sub.offset / sizeof(uint32_t);

        const Ren::Span<const Ren::TriGroup> groups = acc.mesh->groups();
        for (int j = 0; j < int(groups.size()); ++j) {
            const Ren::TriGroup &grp = groups[j];

            const Ren::MaterialRef &front_mat =
                (j >= acc.material_override.size()) ? grp.front_mat : acc.material_override[j].first;
            const Ren::MaterialRef &back_mat =
                (j >= acc.material_override.size()) ? grp.back_mat : acc.material_override[j].second;

            if ((front_mat->flags() & Ren::eMatFlags::Emissive) == 0) {
                continue;
            }

            const bool doublesided = (back_mat->flags() & Ren::eMatFlags::Emissive) != 0;

            const uint32_t index_beg = grp.byte_offset / sizeof(uint32_t);
            const uint32_t index_end = index_beg + grp.num_indices;

            for (uint32_t i = index_beg; i < index_end; i += 3) {
                const uint32_t i0 = tri_indices[i + 0], i1 = tri_indices[i + 1], i2 = tri_indices[i + 2];

                const Phy::Vec3f p0 = Phy::MakeVec3(&positions[i0 * VertexStride]),
                                 p1 = Phy::MakeVec3(&positions[i1 * VertexStride]),
                                 p2 = Phy::MakeVec3(&positions[i2 * VertexStride]);

                const Phy::Vec2f uv0 = Phy::MakeVec2(&positions[i0 * VertexStride + 9]),
                                 uv1 = Phy::MakeVec2(&positions[i1 * VertexStride + 9]),
                                 uv2 = Phy::MakeVec2(&positions[i2 * VertexStride + 9]);

                const Ren::Vec4f p0_ws = tr.world_from_object * Ren::Vec4f{p0[0], p0[1], p0[2], 1.0f};
                const Ren::Vec4f p1_ws = tr.world_from_object * Ren::Vec4f{p1[0], p1[1], p1[2], 1.0f};
                const Ren::Vec4f p2_ws = tr.world_from_object * Ren::Vec4f{p2[0], p2[1], p2[2], 1.0f};

                const Ren::Vec4f bbox_min_ws = Min(p0_ws, Min(p1_ws, p2_ws)),
                                 bbox_max_ws = Max(p0_ws, Max(p1_ws, p2_ws));

                const auto bbox_min = Phy::Vec3f{bbox_min_ws[0], bbox_min_ws[1], bbox_min_ws[2]},
                           bbox_max = Phy::Vec3f{bbox_max_ws[0], bbox_max_ws[1], bbox_max_ws[2]};

                if (Phy::Distance2(bbox_min, bbox_max) > 1e-7f) {
                    const Ren::Vec3f light_forward =
                        Cross(Ren::Vec3f{p1_ws} - Ren::Vec3f{p0_ws}, Ren::Vec3f{p2_ws} - Ren::Vec3f{p0_ws});
                    const float area = 0.5f * Length(light_forward);

                    const Ren::Vec3f axis = Normalize(light_forward);
                    const float omega_n = doublesided ? Ren::Pi<float>() : 0.0f;
                    const float omega_e = Ren::Pi<float>() / 2.0f;

                    const float lum = front_mat->params[3][1] + front_mat->params[3][2] + front_mat->params[3][3];
                    const float flux = lum * area;
                    additional_data.push_back({axis, flux, omega_n, omega_e});

                    temp_primitives.push_back({0, 0, 0, bbox_min, bbox_max});
                    temp_indices.push_back(i / 3);

                    LightItem &tri_light = stochastic_lights.emplace_back();
                    tri_light.type_and_flags = LIGHT_TYPE_TRI;
                    if (bool(acc.vis_mask & AccStructure::eRayType::Diffuse)) {
                        tri_light.type_and_flags |= LIGHT_DIFFUSE_BIT;
                    }
                    if (bool(acc.vis_mask & AccStructure::eRayType::Specular)) {
                        tri_light.type_and_flags |= LIGHT_SPECULAR_BIT;
                    }
                    if (doublesided) {
                        tri_light.type_and_flags |= LIGHT_DOUBLESIDED_BIT;
                    }
                    memcpy(tri_light.col, ValuePtr(front_mat->params[3]) + 1, 3 * sizeof(float));
                    memcpy(tri_light.pos, ValuePtr(p0_ws), 3 * sizeof(float));
                    memcpy(tri_light.u, ValuePtr(p1_ws), 3 * sizeof(float));
                    memcpy(tri_light.v, ValuePtr(p2_ws), 3 * sizeof(float));
                    // store UVs
                    tri_light.radius = uv0[0];
                    tri_light.dir[0] = uv0[1];
                    tri_light.dir[1] = uv1[0];
                    tri_light.dir[2] = uv1[1];
                    tri_light.spot = uv2[0];
                    tri_light.blend = uv2[1];
                    // use as emissive texture index
                    tri_light.shadowreg_index = front_mat.index() * MAX_TEX_PER_MATERIAL + MAT_TEX_EMISSION;
                    // store triangle index
                    tri_light.tri_index = (indices_start + i) / 3;
                }
            }
        }
    }

    if (stochastic_lights.empty()) {
        return;
    }

    std::vector<gpu_light_wbvh_node_t> light_wnodes;
    { // Build BVH
        std::vector<uint32_t> prim_indices;
        prim_indices.reserve(temp_primitives.size());

        std::vector<gpu_bvh_node_t> temp_nodes;

        Phy::split_settings_t s;
        s.oversplit_threshold = -1.0f;
        s.min_primitives_in_leaf = 1;
        PreprocessPrims_SAH(temp_primitives, s, temp_nodes, prim_indices);

        // convert to light nodes
        std::vector<gpu_light_bvh_node_t> light_nodes(temp_nodes.size(), gpu_light_bvh_node_t{});
        for (uint32_t i = 0; i < temp_nodes.size(); ++i) {
            static_cast<gpu_bvh_node_t &>(light_nodes[i]) = temp_nodes[i];
            if ((temp_nodes[i].prim_index & LEAF_NODE_BIT) != 0) {
                const uint32_t prim_index = prim_indices[temp_nodes[i].prim_index & PRIM_INDEX_BITS];
                light_nodes[i].axis = additional_data[prim_index].axis;
                light_nodes[i].flux = additional_data[prim_index].flux;
                light_nodes[i].omega_n = additional_data[prim_index].omega_n;
                light_nodes[i].omega_e = additional_data[prim_index].omega_e;
            }
        }

        // Init parent array
        std::vector<uint32_t> parent_indices(light_nodes.size());
        parent_indices[0] = 0xffffffff; // root node has no parent

        std::vector<uint32_t> leaf_indices;
        leaf_indices.reserve(temp_primitives.size());

        Ren::SmallVector<uint32_t, 128> stack;
        stack.push_back(0);
        while (!stack.empty()) {
            const uint32_t i = stack.back();
            stack.pop_back();

            if ((light_nodes[i].prim_index & LEAF_NODE_BIT) == 0) {
                const uint32_t left_child = (light_nodes[i].left_child & LEFT_CHILD_BITS),
                               right_child = (light_nodes[i].right_child & RIGHT_CHILD_BITS);
                parent_indices[left_child] = parent_indices[right_child] = i;

                stack.push_back(left_child);
                stack.push_back(right_child);
            } else {
                leaf_indices.push_back(i);
            }
        }

        // Propagate flux and cone up the hierarchy
        std::vector<uint32_t> to_process;
        to_process.reserve(light_nodes.size());
        to_process.insert(end(to_process), begin(leaf_indices), end(leaf_indices));
        for (uint32_t i = 0; i < uint32_t(to_process.size()); ++i) {
            const uint32_t n = to_process[i];
            const uint32_t parent = parent_indices[n];
            if (parent == 0xffffffff) {
                continue;
            }

            light_nodes[parent].flux += light_nodes[n].flux;
            if (light_nodes[parent].axis[0] == 0.0f && light_nodes[parent].axis[1] == 0.0f &&
                light_nodes[parent].axis[2] == 0.0f) {
                light_nodes[parent].axis = light_nodes[n].axis;
                light_nodes[parent].omega_n = light_nodes[n].omega_n;
            } else {
                Ren::Vec3f axis1 = light_nodes[parent].axis, axis2 = light_nodes[n].axis;
                const float angle_between = acosf(Ren::Clamp(Dot(axis1, axis2), -1.0f, 1.0f));

                axis1 += axis2;
                const float axis_length = Length(axis1);
                if (axis_length != 0.0f) {
                    axis1 /= axis_length;
                } else {
                    axis1 = Ren::Vec3f{0.0f, 1.0f, 0.0f};
                }

                light_nodes[parent].axis = axis1;

                light_nodes[parent].omega_n =
                    fminf(0.5f * (light_nodes[parent].omega_n +
                                  fmaxf(light_nodes[parent].omega_n, angle_between + light_nodes[n].omega_n)),
                          Ren::Pi<float>());
            }
            light_nodes[parent].omega_e = fmaxf(light_nodes[parent].omega_e, light_nodes[n].omega_e);
            if ((light_nodes[parent].left_child & LEFT_CHILD_BITS) == n) {
                to_process.push_back(parent);
            }
        }

        // Remove indices indirection
        for (uint32_t i = 0; i < leaf_indices.size(); ++i) {
            gpu_light_bvh_node_t &n = light_nodes[leaf_indices[i]];
            assert((n.prim_index & LEAF_NODE_BIT) != 0);
            const uint32_t li_index = prim_indices[n.prim_index & PRIM_INDEX_BITS];
            n.prim_index &= ~PRIM_INDEX_BITS;
            n.prim_index |= li_index;
        }

        // Flatten to 8-wide BVH
        const uint32_t root_node = FlattenBVH_r(light_nodes, 0, 0xffffffff, light_wnodes);
        assert(root_node == 0);
        (void)root_node;

        // Collapse leaf level (all nodes have only 1 child)
        std::vector<bool> should_remove(light_wnodes.size(), false);
        for (uint32_t i = 0; i < light_wnodes.size(); ++i) {
            if ((light_wnodes[i].child[0] & LEAF_NODE_BIT) == 0) {
                for (int j = 0; j < 8; ++j) {
                    if (light_wnodes[i].child[j] == 0x7fffffff) {
                        continue;
                    }
                    if ((light_wnodes[light_wnodes[i].child[j]].child[0] & LEAF_NODE_BIT) != 0) {
                        assert(light_wnodes[light_wnodes[i].child[j]].child[1] == 1);
                        should_remove[light_wnodes[i].child[j]] = true;
                        light_wnodes[i].child[j] = light_wnodes[light_wnodes[i].child[j]].child[0];
                    }
                }
            }
        }
        std::vector<uint32_t> compacted_indices;
        uint32_t cur_index = 0;
        for (const bool b : should_remove) {
            compacted_indices.push_back(cur_index);
            if (!b) {
                ++cur_index;
            }
        }
        for (int i = int(light_wnodes.size() - 1); i >= 0; --i) {
            if (should_remove[i]) {
                light_wnodes.erase(begin(light_wnodes) + i);
            } else {
                for (int j = 0; j < 8; ++j) {
                    if (light_wnodes[i].child[j] == 0x7fffffff) {
                        continue;
                    }
                    if ((light_wnodes[i].child[j] & LEAF_NODE_BIT) == 0) {
                        light_wnodes[i].child[j] = compacted_indices[light_wnodes[i].child[j] & PRIM_INDEX_BITS];
                    }
                }
            }
        }
    }

    { // Init GPU data
        scene_data_.persistent_data.stoch_lights_buf = ren_ctx_.LoadBuffer(
            "Stochastic Lights Buf", Ren::eBufType::Texture, uint32_t(stochastic_lights.size() * sizeof(LightItem)));
        scene_data_.persistent_data.stoch_lights_nodes_buf =
            ren_ctx_.LoadBuffer("Stochastic Light Nodes Buf", Ren::eBufType::Texture,
                                uint32_t(light_wnodes.size() * sizeof(gpu_light_wbvh_node_t)));

        Ren::Buffer lights_buf_stage("Stochastic Lights Stage Buf", ren_ctx_.api_ctx(), Ren::eBufType::Upload,
                                     scene_data_.persistent_data.stoch_lights_buf->size());
        { // init stage buf
            uint8_t *mapped_ptr = lights_buf_stage.Map();
            memcpy(mapped_ptr, stochastic_lights.data(), stochastic_lights.size() * sizeof(LightItem));
            lights_buf_stage.Unmap();
        }

        Ren::Buffer nodes_buf_stage("Stochastic Light Nodes Stage Buf", ren_ctx_.api_ctx(), Ren::eBufType::Upload,
                                    scene_data_.persistent_data.stoch_lights_nodes_buf->size());
        { // init stage buf
            uint8_t *mapped_ptr = nodes_buf_stage.Map();
            memcpy(mapped_ptr, light_wnodes.data(), light_wnodes.size() * sizeof(gpu_light_wbvh_node_t));
            nodes_buf_stage.Unmap();
        }

        Ren::CommandBuffer cmd_buf = ren_ctx_.BegTempSingleTimeCommands();
        Ren::CopyBufferToBuffer(lights_buf_stage, 0, *scene_data_.persistent_data.stoch_lights_buf, 0,
                                uint32_t(stochastic_lights.size() * sizeof(LightItem)), cmd_buf);
        Ren::CopyBufferToBuffer(nodes_buf_stage, 0, *scene_data_.persistent_data.stoch_lights_nodes_buf, 0,
                                uint32_t(light_wnodes.size() * sizeof(gpu_light_wbvh_node_t)), cmd_buf);
        ren_ctx_.EndTempSingleTimeCommands(cmd_buf);

        lights_buf_stage.FreeImmediate();
        nodes_buf_stage.FreeImmediate();
    }
}
