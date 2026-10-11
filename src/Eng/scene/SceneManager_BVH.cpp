#include "SceneManager.h"

#include <deque>
#include <utility>

#include <Phy/BVHSplit.h>
#include <Ren/Context.h>
#include <Ren/ResizableBuffer.h>
#include <Sys/BinaryTree.h>
#include <Sys/MonoAlloc.h>
#include <Sys/ScopeExit.h>

#include <optick/optick.h>
#include <vtune/ittnotify.h>
extern __itt_domain *__g_itt_domain;

namespace Eng::SceneManagerInternal {
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

    uint32_t sep_axis = 0;
    const Ren::Vec3f c_left = (nodes[ch0].bbox_min + nodes[ch0].bbox_max) / 2,
                     c_right = (nodes[ch1].bbox_min + nodes[ch1].bbox_max) / 2;

    const Ren::Vec3f dist = Abs(c_left - c_right);

    if (dist[0] > dist[1] && dist[0] > dist[2]) {
        sep_axis = 0;
    } else if (dist[1] > dist[0] && dist[1] > dist[2]) {
        sep_axis = 1;
    } else {
        sep_axis = 2;
    }

    node.sep_axis = sep_axis;

    if (c_left[sep_axis] > c_right[sep_axis]) {
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
    // NOTE: 65534 was chosen for precise 0.0 representation
    const auto a = uint32_t(std::floor(65534.0f * ((cos_a + 1.0f) / 2.0f)));
    const auto b = uint32_t(std::floor(65534.0f * ((cos_b + 1.0f) / 2.0f)));
    return (a << 16) | b;
}

uint8_t encode_snorm_u8(const float f) {
    return uint8_t(std::round(Ren::Clamp((f + 1) / 2.0f, 0.0f, 1.0f) * 255.0f));
}

uint16_t encode_cosines8(const float cos_a, const float cos_b) {
    // NOTE: 254 was chosen for precise 0.0 representation
    const uint16_t a = uint16_t(std::floor(254.0f * ((cos_a + 1.0f) / 2.0f)));
    const uint16_t b = uint16_t(std::floor(254.0f * ((cos_b + 1.0f) / 2.0f)));
    return uint16_t((a << 8) | b);
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

// Same octahedral packing, 8 bits per component (compact light tree)
uint16_t EncodeOctDir8(const Ren::Vec3f &d) {
    const float denom = fabsf(d[0]) + fabsf(d[1]) + fabsf(d[2]);
    const float v[3] = {d[0] / denom, d[1] / denom, d[2] / denom};
    if (v[2] < 0.0f) {
        const uint8_t x = encode_snorm_u8((1.0f - fabsf(v[1])) * copysignf(1.0f, v[0]));
        const uint8_t y = encode_snorm_u8((1.0f - fabsf(v[0])) * copysignf(1.0f, v[1]));
        return (uint16_t(x) << 8) | y;
    }
    const uint8_t x = encode_snorm_u8(v[0]);
    const uint8_t y = encode_snorm_u8(v[1]);
    return (uint16_t(x) << 8) | y;
}

uint8_t encode_flux(const float flux, const float parent_flux) {
    if (flux <= 0.0f || parent_flux <= 0.0f) {
        return 0;
    }
    return uint8_t(int(fmaxf(1.0f, std::round(255.0f * cbrt(Ren::Clamp(flux / parent_flux, 0.0f, 1.0f))))));
}

float quantize(const float v, const float min, const float max) {
    if (min == max) {
        return 0.0f;
    }
    return Ren::Clamp(255.0f * (v - min) / (max - min), 0.0f, 255.0f);
}

void pow2Extents(const float min[3], float max[3], int8_t out_exp[3]) {
    for (int i = 0; i < 3; ++i) {
        const float ext = max[i] - min[i];
        if (ext > 0.0f) {
            const int e = int(std::ceil(std::log2(ext / 255.0f)));
            assert(e >= -126 && e <= 127);
            out_exp[i] = int8_t(e);
            max[i] = min[i] + 255.0f * std::exp2(float(e));
        } else {
            out_exp[i] = 0;
        }
    }
}

Phy::Vec3f adapt(const Ren::Vec3f &v) { return Phy::Vec3f{v[0], v[1], v[2]}; }

__itt_string_handle *itt_rebuild_bvh_str = __itt_string_handle_create("SceneManager::RebuildSceneBVH");
__itt_string_handle *itt_update_bvh_str = __itt_string_handle_create("SceneManager::UpdateBVH");
} // namespace Eng::SceneManagerInternal

void Eng::SceneManager::RebuildSceneBVH() {
    using namespace SceneManagerInternal;

    __itt_task_begin(__g_itt_domain, __itt_null, __itt_null, itt_rebuild_bvh_str);

    ren_ctx_.log()->Info("SceneManager: RebuildSceneBVH!");

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
        uint32_t parent_index = 0xffffffff;
        std::vector<uint32_t> indices;
        Phy::Vec3f min = Phy::Vec3f{std::numeric_limits<float>::max()},
                   max = Phy::Vec3f{std::numeric_limits<float>::lowest()};
        prims_coll_t() = default;
        prims_coll_t(const uint32_t _parent_index, std::vector<uint32_t> &&_indices, const Phy::Vec3f &_min,
                     const Phy::Vec3f &_max)
            : parent_index(_parent_index), indices(std::move(_indices)), min(_min), max(_max) {}
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
        const uint32_t parent_index = prim_lists.back().parent_index;
        Phy::split_data_t split_data = SplitPrimitives_SAH(&primitives[0], prim_lists.back().indices,
                                                           prim_lists.back().min, prim_lists.back().max, s);
        prim_lists.pop_back();

        const auto leaf_index = uint32_t(scene_data_.nodes.size());

        if (split_data.right_indices.empty()) {
            const Phy::Vec3f bbox_min = split_data.left_bounds[0], bbox_max = split_data.left_bounds[1];

            const auto new_node_index = uint32_t(scene_data_.nodes.size());

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
            n.sep_axis = 0;
            n.prim_count = uint32_t(split_data.left_indices.size());
            n.parent = parent_index;
        } else {
            auto index = uint32_t(nodes_count);

            const Phy::Vec3f c_left = (split_data.left_bounds[0] + split_data.left_bounds[1]) / 2.0f,
                             c_right = (split_data.right_bounds[0] + split_data.right_bounds[1]) / 2.0f;

            const Phy::Vec3f dist = Abs(c_left - c_right);

            const uint32_t sep_axis =
                (dist[0] > dist[1] && dist[0] > dist[2]) ? 0 : ((dist[1] > dist[0] && dist[1] > dist[2]) ? 1 : 2);

            const Phy::Vec3f bbox_min = Min(split_data.left_bounds[0], split_data.right_bounds[0]),
                             bbox_max = Max(split_data.left_bounds[1], split_data.right_bounds[1]);

            scene_data_.nodes.emplace_back();
            bvh_node_t &n = scene_data_.nodes.back();

            n.bbox_min = Ren::Vec3f{bbox_min[0], bbox_min[1], bbox_min[2]};
            n.leaf_node = 0;
            n.left_child = index + 1;
            n.bbox_max = Ren::Vec3f{bbox_max[0], bbox_max[1], bbox_max[2]};
            n.sep_axis = sep_axis;
            n.right_child = index + 2;
            n.parent = parent_index;

            prim_lists.emplace_front(leaf_index, std::move(split_data.left_indices), split_data.left_bounds[0],
                                     split_data.left_bounds[1]);
            prim_lists.emplace_front(leaf_index, std::move(split_data.right_indices), split_data.right_bounds[0],
                                     split_data.right_bounds[1]);

            nodes_count += 2;
        }
    }

    __itt_task_end(__g_itt_domain);
}

void Eng::SceneManager::UpdateWorldScrolling(const Ren::Vec3d &new_origin) {
    using namespace SceneManagerInternal;

    ren_ctx_.log()->Info("SceneManager: UpdateWorldScrolling!");

    auto *transforms = (Transform *)scene_data_.comp_store[CompTransform]->SequentialData();

    bvh_node_t *nodes = scene_data_.nodes.data();

    const Ren::Vec3f origin_diff = Ren::Vec3f(new_origin - scene_data_.origin);

    for (uint32_t i = 0; i < scene_data_.objects.size(); ++i) {
        SceneObject &obj = scene_data_.objects[i];
        if (obj.comp_mask & CompTransformBit) {
            Transform &tr = transforms[obj.components[CompTransform]];

            tr.world_from_object[3][0] = float(tr.position[0] - new_origin[0]);
            tr.world_from_object[3][1] = float(tr.position[1] - new_origin[1]);
            tr.world_from_object[3][2] = float(tr.position[2] - new_origin[2]);
            tr.UpdateTemporaryData();

            // The best we can do here is to translate previous matrix into the space of a new camera sector
            tr.world_from_object_prev[3][0] += origin_diff[0];
            tr.world_from_object_prev[3][1] += origin_diff[1];
            tr.world_from_object_prev[3][2] += origin_diff[2];

            if (tr.node_index != 0xffffffff) {
                bvh_node_t &node = nodes[tr.node_index];

                const Ren::Vec3f d = tr.bbox_max_ws - tr.bbox_min_ws;
                node.bbox_min = tr.bbox_min_ws - BoundsMargin * d;
                node.bbox_max = tr.bbox_max_ws + BoundsMargin * d;

                // update hierarchy boxes
                uint32_t up_parent = node.parent;
                while (up_parent != 0xffffffff) {
                    const uint32_t ch0 = nodes[up_parent].left_child, ch1 = nodes[up_parent].right_child;

                    nodes[up_parent].bbox_min = Min(nodes[ch0].bbox_min, nodes[ch1].bbox_min);
                    nodes[up_parent].bbox_max = Max(nodes[ch0].bbox_max, nodes[ch1].bbox_max);

                    // NOTE: children sorting is unnecessary here

                    up_parent = nodes[up_parent].parent;
                }
            }

            instance_data_to_update_.push_back(i);
        }
    }

    scene_data_.origin = new_origin;
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
            new_node.sep_axis = 0;
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

Ren::AccStructHandle Eng::SceneManager::Build_SWRT_BLAS(const AccStructure &acc) {
    using namespace SceneManagerInternal;

    const Ren::ApiContext &api = ren_ctx_.api();
    const Ren::StoragesRef &storages = ren_ctx_.storages();

    const auto &[mesh_main, mesh_cold] = storages.meshes[acc.mesh];
    const Ren::BufferRange &attribs = mesh_main.attribs_buf1, &indices = mesh_main.indices_buf;

    assert((attribs.sub.offset % 16) == 0);
    const uint32_t first_vertex = attribs.sub.offset / 16;
    assert((indices.sub.offset % (3 * sizeof(uint32_t))) == 0);
    const uint32_t prim_offset = indices.sub.offset / (3 * sizeof(uint32_t));

    assert(mesh_main.type == Ren::eMeshType::Simple);
    const int VertexStride = 13;

    if (!scene_data_.persistent_data->swrt.rt_prim_indices_buf) {
        scene_data_.persistent_data->swrt.rt_prim_indices_buf = std::make_unique<Ren::ResizableBuffer>(
            ren_ctx_.api(), "SWRT Prim Indices", Ren::eBufType::Texture, uint32_t(sizeof(uint32_t)), storages.buffers);
        scene_data_.persistent_data->swrt.rt_prim_indices_buf->Resize(uint32_t(1024 * sizeof(uint32_t)),
                                                                      ren_ctx_.log());
        scene_data_.persistent_data->swrt.rt_prim_indices_buf->AddView(Ren::eFormat::R32UI);
    }
    if (!scene_data_.persistent_data->swrt.rt_blas_buf) {
        scene_data_.persistent_data->swrt.rt_blas_buf = std::make_unique<Ren::ResizableBuffer>(
            ren_ctx_.api(), "SWRT BLAS", Ren::eBufType::Storage, 16, storages.buffers);
        scene_data_.persistent_data->swrt.rt_blas_buf->Resize(uint32_t(1024 * sizeof(gpu_cwbvh_node_t)),
                                                              ren_ctx_.log());
        scene_data_.persistent_data->swrt.rt_blas_buf->AddView(Ren::eFormat::RGBA32F);
    }
    if (!scene_data_.persistent_data->swrt.rt_blas_leaf_prims_buf) {
        scene_data_.persistent_data->swrt.rt_blas_leaf_prims_buf = std::make_unique<Ren::ResizableBuffer>(
            ren_ctx_.api(), "SWRT BLAS Leaf Prims", Ren::eBufType::Texture, uint32_t(sizeof(uint32_t)),
            storages.buffers);
        scene_data_.persistent_data->swrt.rt_blas_leaf_prims_buf->Resize(uint32_t(1024 * sizeof(uint32_t)),
                                                                         ren_ctx_.log());
        scene_data_.persistent_data->swrt.rt_blas_leaf_prims_buf->AddView(Ren::eFormat::R32UI);
    }

    const uint32_t mesh_index = scene_data_.persistent_data->swrt.rt_meshes.Emplace();

    //
    // Gather geometries
    //

    mesh_t new_mesh = {};

    std::vector<Phy::prim_t> temp_primitives;
    std::vector<uint32_t> temp_indices;

    Ren::Span<const float> positions = mesh_cold.attribs;
    Ren::Span<const uint32_t> tri_indices = mesh_cold.indices;

    for (const Ren::tri_group_t &grp : mesh_cold.groups) {
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

    std::vector<gpu_bvh_node_t> temp_nodes;
    std::vector<uint32_t> prim_indices;

    Phy::split_settings_t s;
    s.oversplit_threshold = -1.0f;
    s.min_primitives_in_leaf = 8;
    PreprocessPrims_SAH(temp_primitives, s, 8, temp_nodes, prim_indices);

    std::vector<gpu_cwbvh_node_t> temp_cw_nodes;
    std::vector<uint32_t> temp_leaf_prims; // 8-primitive block bases into prim_indices
    [[maybe_unused]] const uint32_t root_node = FlattenCompactBVH_r(temp_nodes, 0, temp_cw_nodes, temp_leaf_prims);
    assert(root_node == 0);

    const Ren::SubAllocation nodes_alloc = scene_data_.persistent_data->swrt.rt_blas_buf->AllocSubRegion(
        uint32_t(temp_cw_nodes.size()) * sizeof(gpu_cwbvh_node_t), sizeof(gpu_cwbvh_node_t), {}, ren_ctx_.log());
    const auto &[rt_blas_buf_main, rt_blas_buf_cold] = scene_data_.persistent_data->swrt.rt_blas_buf->buffer();

    const Ren::SubAllocation prim_alloc = scene_data_.persistent_data->swrt.rt_prim_indices_buf->AllocSubRegion(
        uint32_t(prim_indices.size()) * sizeof(uint32_t), sizeof(uint32_t), {}, ren_ctx_.log());
    const auto &[rt_prim_indices_buf_main, rt_prim_indices_buf_cold] =
        scene_data_.persistent_data->swrt.rt_prim_indices_buf->buffer();

    const Ren::SubAllocation leaf_prims_alloc =
        scene_data_.persistent_data->swrt.rt_blas_leaf_prims_buf->AllocSubRegion(
            uint32_t(temp_leaf_prims.size()) * sizeof(uint32_t), sizeof(uint32_t), {}, ren_ctx_.log());
    const auto &[rt_blas_leaf_prims_buf_main, rt_blas_leaf_prims_buf_cold] =
        scene_data_.persistent_data->swrt.rt_blas_leaf_prims_buf->buffer();

    new_mesh.node_index = nodes_alloc.offset / sizeof(gpu_cwbvh_node_t);
    new_mesh.node_count = uint32_t(temp_cw_nodes.size());
    new_mesh.tris_index = prim_alloc.offset / sizeof(uint32_t);
    new_mesh.tris_count = uint32_t(prim_indices.size());
    new_mesh.vert_index = first_vertex;

    // offset node & leaf payload indices
    const uint32_t leaf_prims_index = leaf_prims_alloc.offset / sizeof(uint32_t);
    for (uint32_t i = 0; i < uint32_t(temp_cw_nodes.size()); ++i) {
        gpu_cwbvh_node_t &n = temp_cw_nodes[i];
        // child_base lives in the low 24 bits, node_index is far below the imask bits
        assert(cwbvh_child_base(n) + new_mesh.node_index < (1u << 24));
        n.child_base_imask += new_mesh.node_index;
        n.prim_base += leaf_prims_index;
    }
    for (uint32_t &p : temp_leaf_prims) {
        p = new_mesh.tris_index + (p & PRIM_INDEX_BITS);
    }

    for (int i = 0; i < int(prim_indices.size()); ++i) {
        prim_indices[i] = prim_offset + temp_indices[prim_indices[i]];
    }

    scene_data_.persistent_data->swrt.rt_meshes[mesh_index] = new_mesh;

    const Ren::AccStructHandle new_blas = ren_ctx_.CreateAccStruct();
    const auto &[blas_main, blas_cold] = storages.acc_structs[new_blas];
    if (!AccStruct_Init(blas_main, blas_cold, mesh_cold.name, mesh_index, nodes_alloc, prim_alloc)) {
        ren_ctx_.ReleaseAccStruct(new_blas);
        return {};
    }
    scene_data_.persistent_data->rt_blases.push_back(new_blas);

    const auto total_nodes_size = uint32_t(temp_cw_nodes.size() * sizeof(gpu_cwbvh_node_t));
    const auto total_prim_indices_size = uint32_t(prim_indices.size() * sizeof(uint32_t));
    const auto total_leaf_prims_size = uint32_t(temp_leaf_prims.size() * sizeof(uint32_t));

    Ren::BufferMain rt_blas_stage_buf_main = {};
    Ren::BufferCold rt_blas_stage_buf_cold = {};
    if (!Ren::Buffer_Init(api, rt_blas_stage_buf_main, rt_blas_stage_buf_cold, Ren::String{"SWRT BLAS Upload"},
                          Ren::eBufType::Upload, total_nodes_size, ren_ctx_.log())) {
        ren_ctx_.log()->Error("Failed to initialize staging buffer for SWRT BLAS");
        return {};
    }
    SCOPE_EXIT({ Buffer_DestroyImmediately(api, rt_blas_stage_buf_main, rt_blas_stage_buf_cold); })

    {
        uint8_t *rt_blas_stage = Buffer_Map(api, rt_blas_stage_buf_main, rt_blas_stage_buf_cold);
        memcpy(rt_blas_stage, temp_cw_nodes.data(), total_nodes_size);
        Buffer_Unmap(api, rt_blas_stage_buf_main, rt_blas_stage_buf_cold);
    }

    Ren::BufferMain rt_prim_indices_stage_buf_main = {};
    Ren::BufferCold rt_prim_indices_stage_buf_cold = {};
    if (!Ren::Buffer_Init(api, rt_prim_indices_stage_buf_main, rt_prim_indices_stage_buf_cold,
                          Ren::String{"SWRT Prim Indices Upload"}, Ren::eBufType::Upload, total_prim_indices_size,
                          ren_ctx_.log())) {
        ren_ctx_.log()->Error("Failed to initialize staging buffer for SWRT primitive indices");
        return {};
    }
    SCOPE_EXIT({ Buffer_DestroyImmediately(api, rt_prim_indices_stage_buf_main, rt_prim_indices_stage_buf_cold); })

    {
        uint8_t *rt_prim_indices_stage =
            Buffer_Map(api, rt_prim_indices_stage_buf_main, rt_prim_indices_stage_buf_cold);
        memcpy(rt_prim_indices_stage, prim_indices.data(), total_prim_indices_size);
        Buffer_Unmap(api, rt_prim_indices_stage_buf_main, rt_prim_indices_stage_buf_cold);
    }

    Ren::BufferMain rt_leaf_prims_stage_buf_main = {};
    Ren::BufferCold rt_leaf_prims_stage_buf_cold = {};
    if (!Ren::Buffer_Init(api, rt_leaf_prims_stage_buf_main, rt_leaf_prims_stage_buf_cold,
                          Ren::String{"SWRT BLAS Leaf Prims Upload"}, Ren::eBufType::Upload, total_leaf_prims_size,
                          ren_ctx_.log())) {
        ren_ctx_.log()->Error("Failed to initialize staging buffer for SWRT BLAS leaf prims");
        return {};
    }
    SCOPE_EXIT({ Buffer_DestroyImmediately(api, rt_leaf_prims_stage_buf_main, rt_leaf_prims_stage_buf_cold); })

    {
        uint8_t *rt_leaf_prims_stage = Buffer_Map(api, rt_leaf_prims_stage_buf_main, rt_leaf_prims_stage_buf_cold);
        memcpy(rt_leaf_prims_stage, temp_leaf_prims.data(), total_leaf_prims_size);
        Buffer_Unmap(api, rt_leaf_prims_stage_buf_main, rt_leaf_prims_stage_buf_cold);
    }

    Ren::CommandBuffer cmd_buf = ren_ctx_.BegTempSingleTimeCommands();

    CopyBufferToBuffer(api, rt_blas_stage_buf_main, 0, rt_blas_buf_main, nodes_alloc.offset, total_nodes_size, cmd_buf);
    CopyBufferToBuffer(api, rt_prim_indices_stage_buf_main, 0, rt_prim_indices_buf_main, prim_alloc.offset,
                       total_prim_indices_size, cmd_buf);
    CopyBufferToBuffer(api, rt_leaf_prims_stage_buf_main, 0, rt_blas_leaf_prims_buf_main, leaf_prims_alloc.offset,
                       total_leaf_prims_size, cmd_buf);

    ren_ctx_.EndTempSingleTimeCommands(cmd_buf);

    return new_blas;
}

void Eng::SceneManager::Alloc_SWRT_TLAS() {
    const Ren::StoragesRef &storages = ren_ctx_.storages();

    auto &data = *scene_data_.persistent_data;

    data.rt_tlas_buf[int(eTLASIndex::Main)] = ren_ctx_.CreateBuffer(
        Ren::String{"TLAS"}, Ren::eBufType::Storage, uint32_t(MAX_RT_TLAS_NODES * sizeof(gpu_cwbvh_node_t)));
    ren_ctx_.FindOrCreateBufferView(data.rt_tlas_buf[int(eTLASIndex::Main)], Ren::eFormat::RGBA32F);
    data.rt_tlas_leaf_prims_buf[int(eTLASIndex::Main)] = ren_ctx_.CreateBuffer(
        Ren::String{"TLAS Leaf Prims"}, Ren::eBufType::Storage, uint32_t(MAX_RT_TLAS_NODES * sizeof(uint32_t)));
    ren_ctx_.FindOrCreateBufferView(data.rt_tlas_leaf_prims_buf[int(eTLASIndex::Main)], Ren::eFormat::R32UI);

    data.rt_tlas_buf[int(eTLASIndex::Shadow)] = ren_ctx_.CreateBuffer(
        Ren::String{"TLAS Shadow"}, Ren::eBufType::Storage, uint32_t(MAX_RT_TLAS_NODES * sizeof(gpu_cwbvh_node_t)));
    ren_ctx_.FindOrCreateBufferView(data.rt_tlas_buf[int(eTLASIndex::Shadow)], Ren::eFormat::RGBA32F);
    data.rt_tlas_leaf_prims_buf[int(eTLASIndex::Shadow)] = ren_ctx_.CreateBuffer(
        Ren::String{"TLAS Shadow Leaf Prims"}, Ren::eBufType::Storage,
        uint32_t(MAX_RT_TLAS_NODES * sizeof(uint32_t)));
    ren_ctx_.FindOrCreateBufferView(data.rt_tlas_leaf_prims_buf[int(eTLASIndex::Shadow)], Ren::eFormat::R32UI);

    data.rt_tlas_buf[int(eTLASIndex::Volume)] = ren_ctx_.CreateBuffer(
        Ren::String{"TLAS Volume"}, Ren::eBufType::Storage, uint32_t(MAX_RT_TLAS_NODES * sizeof(gpu_cwbvh_node_t)));
    ren_ctx_.FindOrCreateBufferView(data.rt_tlas_buf[int(eTLASIndex::Volume)], Ren::eFormat::RGBA32F);
    data.rt_tlas_leaf_prims_buf[int(eTLASIndex::Volume)] = ren_ctx_.CreateBuffer(
        Ren::String{"TLAS Volume Leaf Prims"}, Ren::eBufType::Storage, uint32_t(MAX_RT_TLAS_NODES * sizeof(uint32_t)));
    ren_ctx_.FindOrCreateBufferView(data.rt_tlas_leaf_prims_buf[int(eTLASIndex::Volume)], Ren::eFormat::R32UI);

    if (!data.swrt.rt_prim_indices_buf) {
        scene_data_.persistent_data->swrt.rt_prim_indices_buf = std::make_unique<Ren::ResizableBuffer>(
            ren_ctx_.api(), "SWRT Prim Indices", Ren::eBufType::Texture, uint32_t(sizeof(uint32_t)), storages.buffers);
        scene_data_.persistent_data->swrt.rt_prim_indices_buf->Resize(uint32_t(1024 * sizeof(uint32_t)),
                                                                      ren_ctx_.log());
        scene_data_.persistent_data->swrt.rt_prim_indices_buf->AddView(Ren::eFormat::R32UI);
    }
    if (!data.swrt.rt_blas_buf) {
        data.swrt.rt_blas_buf = std::make_unique<Ren::ResizableBuffer>(ren_ctx_.api(), "SWRT BLAS",
                                                                       Ren::eBufType::Texture, 16, storages.buffers);
        data.swrt.rt_blas_buf->Resize(uint32_t(1024 * sizeof(gpu_cwbvh_node_t)), ren_ctx_.log());
        data.swrt.rt_blas_buf->AddView(Ren::eFormat::RGBA32F);
    }
    if (!data.swrt.rt_blas_leaf_prims_buf) {
        data.swrt.rt_blas_leaf_prims_buf = std::make_unique<Ren::ResizableBuffer>(
            ren_ctx_.api(), "SWRT BLAS Leaf Prims", Ren::eBufType::Texture, uint32_t(sizeof(uint32_t)), storages.buffers);
        data.swrt.rt_blas_leaf_prims_buf->Resize(uint32_t(1024 * sizeof(uint32_t)), ren_ctx_.log());
        data.swrt.rt_blas_leaf_prims_buf->AddView(Ren::eFormat::R32UI);
    }
}

uint32_t Eng::SceneManager::PreprocessPrims_SAH(Ren::Span<const Phy::prim_t> prims, const Phy::split_settings_t &s,
                                                const int primitive_alignment, std::vector<gpu_bvh_node_t> &out_nodes,
                                                std::vector<uint32_t> &out_indices) {
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
            while (out_indices.size() % primitive_alignment) {
                out_indices.push_back(out_indices.back());
            }
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

uint32_t Eng::SceneManager::FlattenLightBVH_r(Ren::Span<const gpu_light_bvh_node_t> nodes, const uint32_t node_index,
                                              std::vector<gpu_light_cwbvh_node_t> &out_nodes,
                                              std::vector<uint32_t> &out_leaf_lights, const int at_index,
                                              const uint32_t depth) {
    using namespace SceneManagerInternal;

    const gpu_light_bvh_node_t &cur_node = nodes[node_index];

    // The traversal stack holds at most one entry per level of the path - catch too-deep trees
    // at build time instead of relying on a ray hitting the overflow assert during rendering.
    assert(depth < uint32_t(MAX_STACK_SIZE) && "light tree too deep for the traversal stack");

    // allocate new node (or use the slot pre-reserved by our parent for contiguous children)
    uint32_t new_node_index;
    if (at_index >= 0) {
        new_node_index = uint32_t(at_index);
    } else {
        new_node_index = uint32_t(out_nodes.size());
        out_nodes.emplace_back();
    }

    if (cur_node.prim_index & LEAF_NODE_BIT) {
        // Single light tree - wrap the light into a root node so the root is never a leaf
        // reference; everywhere else the leaf level is collapsed by the parent (see below)
        gpu_light_cwbvh_node_t &new_node = out_nodes[new_node_index];

        new_node.child_base_imask = 0;
        new_node.prim_base = uint32_t(out_leaf_lights.size());
        out_leaf_lights.push_back(cur_node.prim_index);
        new_node.meta4[0] = 0x40u; // slot 0: leaf, rank 0
        new_node.meta4[1] = 0;
        if (cur_node.bbox_min[0] > -MAX_DIST) {
            // the whole-box child encoding (0/255) must decode conservatively under the pow2 step
            float bmin[3], bmax[3];
            memcpy(bmin, &cur_node.bbox_min[0], 3 * sizeof(float));
            memcpy(bmax, &cur_node.bbox_max[0], 3 * sizeof(float));
            pow2Extents(bmin, bmax, new_node.ch_exp);
        } else {
            memset(new_node.ch_exp, 0, sizeof(new_node.ch_exp));
        }

        new_node.bbox_min = cur_node.bbox_min;

        if (cur_node.bbox_min[0] > -MAX_DIST) {
            // Init as whole bounding box
            new_node.ch_bbox_min[0][0] = new_node.ch_bbox_min[1][0] = new_node.ch_bbox_min[2][0] = 0;
            new_node.ch_bbox_max[0][0] = new_node.ch_bbox_max[1][0] = new_node.ch_bbox_max[2][0] = 0xff;
        } else {
            // Init as infinite bounding box
            new_node.ch_bbox_min[0][0] = new_node.ch_bbox_min[1][0] = new_node.ch_bbox_min[2][0] = 0xff;
            new_node.ch_bbox_max[0][0] = new_node.ch_bbox_max[1][0] = new_node.ch_bbox_max[2][0] = 0;
        }

        for (int j = 1; j < 8; ++j) {
            // Init as invalid bounding box
            new_node.ch_bbox_min[0][j] = new_node.ch_bbox_min[1][j] = new_node.ch_bbox_min[2][j] = 0xff;
            new_node.ch_bbox_max[0][j] = new_node.ch_bbox_max[1][j] = new_node.ch_bbox_max[2][j] = 0xff;
            // Init as zero light
            new_node.ch_bitmask[j] = 0;
            new_node.flux[j] = 0;
            new_node.axis[j] = 0;
            new_node.cos_omega_ne[j] = 0;
        }

        new_node.ch_bitmask[0] = 0xff;
        new_node.flux[0] = encode_flux(cur_node.flux, cur_node.flux);
        new_node.axis[0] = EncodeOctDir8(cur_node.axis);
        new_node.cos_omega_ne[0] = encode_cosines8(cosf(cur_node.omega_n), fmaxf(cosf(cur_node.omega_e), 0.0f));

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
    Ren::Vec3f children_centers[8], all_box_min = {FLT_MAX}, all_box_max = {-FLT_MAX};
    for (int i = 0; i < children_count; i++) {
        children_centers[i] = 0.5f * (nodes[children[i]].bbox_min + nodes[children[i]].bbox_max);
        if (nodes[children[i]].bbox_min[0] > -MAX_DIST) {
            all_box_min = Min(all_box_min, nodes[children[i]].bbox_min);
            all_box_max = Max(all_box_max, nodes[children[i]].bbox_max);
        }
    }

    const Ren::Vec3f scale = 2.0f / (all_box_max + 0.001f - all_box_min);

    uint32_t sorted_children[8] = {0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
                                   0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff};
    for (int i = 0; i < children_count; i++) {
        const Ren::Vec3f code = (children_centers[i] - all_box_min) * scale;

        const auto x = uint32_t(code[0]), y = uint32_t(code[1]), z = uint32_t(code[2]);

        uint32_t mort = (z << 2) | (y << 1) | (x << 0);
        while (sorted_children[mort] != 0xffffffff) {
            mort = (mort + 1) % 8;
        }

        sorted_children[mort] = children[i];
    }

    // CWBVH-style pow2 quantization: expand the max corner so the per-axis quantization step is
    // a power of two and store the exponent. Infinite children keep the 0xff/0x00 sentinel.
    int8_t node_exp[3] = {0, 0, 0};
    {
        float bmin[3], bmax[3];
        memcpy(bmin, &all_box_min[0], 3 * sizeof(float));
        memcpy(bmax, &all_box_max[0], 3 * sizeof(float));
        pow2Extents(bmin, bmax, node_exp);
        all_box_min = Ren::Vec3f{bmin[0], bmin[1], bmin[2]};
        all_box_max = Ren::Vec3f{bmax[0], bmax[1], bmax[2]};
    }

    // NOTE: 'new_node' must not be held across recursive calls - out_nodes can reallocate!

    // Implicit child addressing (CWBVH paper): pre-reserve contiguous slots for the internal
    // children, in slot order, so child i resolves to child_base + popc(internal slots below i).
    uint8_t internal_mask = 0;
    int internal_count = 0;
    for (int i = 0; i < 8; i++) {
        if (sorted_children[i] != 0xffffffff && !(nodes[sorted_children[i]].prim_index & LEAF_NODE_BIT)) {
            internal_mask |= uint8_t(1 << i);
            ++internal_count;
        }
    }
    const uint32_t child_base = uint32_t(out_nodes.size());
    out_nodes.resize(size_t(child_base) + size_t(internal_count));

    // meta4 packing (CWBVH paper): leaf light references go to out_leaf_lights, contiguous
    // per parent in slot order (light_leaf_prims[prim_base + rank]).
    const uint32_t prim_base = uint32_t(out_leaf_lights.size());
    uint8_t meta_bytes[8] = {};
    int leaf_rank = 0;

    int child_rank = 0;
    for (int i = 0; i < 8; i++) {
        if (sorted_children[i] == 0xffffffff) {
            continue;
        }
        const gpu_light_bvh_node_t &child = nodes[sorted_children[i]];
        if (child.prim_index & LEAF_NODE_BIT) {
            // Collapse leaf level - lights are referenced directly by their parents
            // (all light leaves hold exactly one light, so no wrapper node is emitted)
            assert(child.prim_count == 1);
            out_leaf_lights.push_back(child.prim_index);
            meta_bytes[i] = uint8_t(0x40 | leaf_rank++);
        } else {
            // the pre-reserved slot - the recursion below writes the node there
            ++child_rank;
            meta_bytes[i] = uint8_t(0x80 | i);
        }
    }
    assert(leaf_rank <= 0x3f);
    assert(child_rank == internal_count);

    // Recurse after this node's children arrays are laid out; the internal-child slots were
    // pre-reserved above, so the recursion cannot move them (never hold a reference across it).
    child_rank = 0;
    for (int i = 0; i < 8; i++) {
        if (sorted_children[i] != 0xffffffff && !(nodes[sorted_children[i]].prim_index & LEAF_NODE_BIT)) {
            FlattenLightBVH_r(nodes, sorted_children[i], out_nodes, out_leaf_lights, int(child_base + child_rank),
                              depth + 1);
            ++child_rank;
        }
    }

    gpu_light_cwbvh_node_t &new_node = out_nodes[new_node_index];
    // child_base lives in the low 24 bits of the packed word
    assert(child_base < (1u << 24));
    new_node.child_base_imask = child_base | (uint32_t(internal_mask) << 24);
    new_node.prim_base = prim_base;
    new_node.meta4[0] = uint32_t(meta_bytes[0]) | (uint32_t(meta_bytes[1]) << 8) | (uint32_t(meta_bytes[2]) << 16) |
                        (uint32_t(meta_bytes[3]) << 24);
    new_node.meta4[1] = uint32_t(meta_bytes[4]) | (uint32_t(meta_bytes[5]) << 8) | (uint32_t(meta_bytes[6]) << 16) |
                        (uint32_t(meta_bytes[7]) << 24);
    new_node.bbox_min = all_box_min;
    memcpy(new_node.ch_exp, node_exp, 3);
    new_node.ch_exp[3] = 0;

    for (int i = 0; i < 8; i++) {
        if (sorted_children[i] != 0xffffffff) {
            const gpu_light_bvh_node_t &child = nodes[sorted_children[i]];

            if (child.bbox_min[0] > -MAX_DIST) {
                new_node.ch_bbox_min[0][i] =
                    uint8_t(floorf(quantize(child.bbox_min[0], all_box_min[0], all_box_max[0])));
                new_node.ch_bbox_min[1][i] =
                    uint8_t(floorf(quantize(child.bbox_min[1], all_box_min[1], all_box_max[1])));
                new_node.ch_bbox_min[2][i] =
                    uint8_t(floorf(quantize(child.bbox_min[2], all_box_min[2], all_box_max[2])));

                new_node.ch_bbox_max[0][i] =
                    uint8_t(ceilf(quantize(child.bbox_max[0], all_box_min[0], all_box_max[0])));
                new_node.ch_bbox_max[1][i] =
                    uint8_t(ceilf(quantize(child.bbox_max[1], all_box_min[1], all_box_max[1])));
                new_node.ch_bbox_max[2][i] =
                    uint8_t(ceilf(quantize(child.bbox_max[2], all_box_min[2], all_box_max[2])));
            } else {
                // Init as infinite bounding box
                new_node.ch_bbox_min[0][i] = new_node.ch_bbox_min[1][i] = new_node.ch_bbox_min[2][i] = 0xff;
                new_node.ch_bbox_max[0][i] = new_node.ch_bbox_max[1][i] = new_node.ch_bbox_max[2][i] = 0;
            }

            new_node.ch_bitmask[i] = 0xff;
            new_node.flux[i] = encode_flux(child.flux, cur_node.flux);
            new_node.axis[i] = EncodeOctDir8(child.axis);
            new_node.cos_omega_ne[i] = encode_cosines8(cosf(child.omega_n), fmaxf(cosf(child.omega_e), 0.0f));
        } else {
            // Init as invalid bounding box
            new_node.ch_bbox_min[0][i] = new_node.ch_bbox_min[1][i] = new_node.ch_bbox_min[2][i] = 0xff;
            new_node.ch_bbox_max[0][i] = new_node.ch_bbox_max[1][i] = new_node.ch_bbox_max[2][i] = 0;
            // Init as zero light
            new_node.ch_bitmask[i] = 0;
            new_node.flux[i] = 0;
            new_node.axis[i] = 0;
            new_node.cos_omega_ne[i] = 0;
        }
    }

    return new_node_index;
}

uint32_t Eng::SceneManager::FlattenCompactBVH_r(Ren::Span<const gpu_bvh_node_t> nodes, const uint32_t node_index,
                                                std::vector<gpu_cwbvh_node_t> &out_nodes,
                                                std::vector<uint32_t> &out_leaf_prims, const int at_index,
                                                const uint32_t depth) {
    using namespace SceneManagerInternal;

    const gpu_bvh_node_t &cur_node = nodes[node_index];

    // The traversal stack holds at most one entry per level of the path - catch too-deep trees
    // at build time instead of relying on a ray hitting the overflow assert during rendering.
    assert(depth < uint32_t(MAX_STACK_SIZE) && "cwbvh tree too deep for the traversal stack");

    // allocate new node (or use the slot pre-reserved by our parent for contiguous children)
    uint32_t new_node_index;
    if (at_index >= 0) {
        new_node_index = uint32_t(at_index);
    } else {
        new_node_index = uint32_t(out_nodes.size());
        out_nodes.emplace_back();
    }

    if (cur_node.prim_index & LEAF_NODE_BIT) {
        // Single leaf tree - wrap it into a regular internal node to make sure root node is never a leaf
        gpu_cwbvh_node_t &new_node = out_nodes[new_node_index];
        new_node.child_base_imask = 0;
        new_node.prim_base = uint32_t(out_leaf_prims.size());
        out_leaf_prims.push_back(cur_node.prim_index);
        new_node.meta4[0] = 0x40u; // slot 0: leaf child, rank 0
        new_node.meta4[1] = 0;
        new_node.bbox_min = cur_node.bbox_min;
        { // the whole-box child encoding (0/255) must decode conservatively under the pow2 step
            float bmin[3], bmax[3];
            memcpy(bmin, &cur_node.bbox_min[0], 3 * sizeof(float));
            memcpy(bmax, &cur_node.bbox_max[0], 3 * sizeof(float));
            pow2Extents(bmin, bmax, new_node.ch_exp);
        }
        for (int j = 0; j < 3; ++j) {
            new_node.ch_bbox_min[j][0] = 0;
            new_node.ch_bbox_max[j][0] = 0xff;
        }
        for (int j = 1; j < 8; ++j) {
            // Init as infinite bounding box
            new_node.ch_bbox_min[0][j] = new_node.ch_bbox_min[1][j] = new_node.ch_bbox_min[2][j] = 0xff;
            new_node.ch_bbox_max[0][j] = new_node.ch_bbox_max[1][j] = new_node.ch_bbox_max[2][j] = 0;
        }
        return new_node_index;
    }

    // Gather children 2 levels deep

    uint32_t children[8];
    int children_count = 0;

    const gpu_bvh_node_t &child0 = nodes[cur_node.left_child];

    if (child0.prim_index & LEAF_NODE_BIT) {
        children[children_count++] = cur_node.left_child & LEFT_CHILD_BITS;
    } else {
        const gpu_bvh_node_t &child00 = nodes[child0.left_child];
        const gpu_bvh_node_t &child01 = nodes[child0.right_child & RIGHT_CHILD_BITS];

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

    const gpu_bvh_node_t &child1 = nodes[cur_node.right_child & RIGHT_CHILD_BITS];

    if (child1.prim_index & LEAF_NODE_BIT) {
        children[children_count++] = cur_node.right_child & RIGHT_CHILD_BITS;
    } else {
        const gpu_bvh_node_t &child10 = nodes[child1.left_child];
        const gpu_bvh_node_t &child11 = nodes[child1.right_child & RIGHT_CHILD_BITS];

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

    whole_box_max += Ren::Vec3f{0.001f};

    const Ren::Vec3f scale = 2.0f / (whole_box_max - whole_box_min);

    uint32_t sorted_children[8] = {0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
                                   0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff};
    for (int i = 0; i < children_count; i++) {
        const Ren::Vec3f code = (children_centers[i] - whole_box_min) * scale;

        const auto x = uint32_t(code[0]), y = uint32_t(code[1]), z = uint32_t(code[2]);

        uint32_t mort = (z << 2) | (y << 1) | (x << 0);
        while (sorted_children[mort] != 0xffffffff) {
            mort = (mort + 1) % 8;
        }

        sorted_children[mort] = children[i];
    }

    // NOTE: 'new_node' must not be held across recursive calls - out_nodes can reallocate!
    float node_min[3], node_max[3];
    int8_t node_exp[3];
    memcpy(node_min, &cur_node.bbox_min[0], 3 * sizeof(float));
    memcpy(node_max, &cur_node.bbox_max[0], 3 * sizeof(float));
    pow2Extents(node_min, node_max, node_exp);

    uint8_t new_ch_min[3][8], new_ch_max[3][8];

    // Implicit child addressing (CWBVH paper): pre-reserve contiguous slots for the internal
    // children, in slot order, so child i resolves to child_base + popc(internal slots below i).
    uint8_t internal_mask = 0;
    int internal_count = 0;
    for (int i = 0; i < 8; i++) {
        if (sorted_children[i] != 0xffffffff && !(nodes[sorted_children[i]].prim_index & LEAF_NODE_BIT)) {
            internal_mask |= uint8_t(1 << i);
            ++internal_count;
        }
    }
    const uint32_t child_base = uint32_t(out_nodes.size());
    out_nodes.resize(size_t(child_base) + size_t(internal_count));

    // meta4 packing (CWBVH paper): leaf primitive references go to out_leaf_prims, contiguous
    // per parent in slot order (leaf_prims[prim_base + rank]). Scene leaves are 8-primitive
    // blocks (primitive_alignment), the payload is the block base.
    const uint32_t prim_base = uint32_t(out_leaf_prims.size());
    uint8_t meta_bytes[8] = {};
    int leaf_rank = 0;
    for (int i = 0; i < 8; i++) {
        if (sorted_children[i] == 0xffffffff) {
            continue;
        }
        if (nodes[sorted_children[i]].prim_index & LEAF_NODE_BIT) {
            assert((nodes[sorted_children[i]].prim_count & PRIM_COUNT_BITS) <= 8);
            out_leaf_prims.push_back(nodes[sorted_children[i]].prim_index);
            meta_bytes[i] = uint8_t(0x40 | leaf_rank++);
        } else {
            meta_bytes[i] = uint8_t(0x80 | i);
        }
    }
    assert(leaf_rank <= 0x3f);

    for (int i = 0; i < 8; i++) {
        if (sorted_children[i] == 0xffffffff) {
            // Init as infinite bounding box
            new_ch_min[0][i] = new_ch_min[1][i] = new_ch_min[2][i] = 0xff;
            new_ch_max[0][i] = new_ch_max[1][i] = new_ch_max[2][i] = 0;
            continue;
        }

        const gpu_bvh_node_t &child = nodes[sorted_children[i]];

        for (int j = 0; j < 3; ++j) {
            new_ch_min[j][i] = uint8_t(floorf(quantize(child.bbox_min[j], node_min[j], node_max[j])));
            new_ch_max[j][i] = uint8_t(ceilf(quantize(child.bbox_max[j], node_min[j], node_max[j])));
            // valid scene children always have finite boxes - the compacted traversal relies on
            // this (no degenerate 0xff/0x00 'infinite box' encoding outside the light tree)
            assert(new_ch_min[j][i] <= new_ch_max[j][i]);
        }
    }

    // Recurse after this node's children arrays are laid out; the internal-child slots were
    // pre-reserved above, so the recursion cannot move them (never hold a reference across it).
    int child_rank = 0;
    for (int i = 0; i < 8; i++) {
        if (sorted_children[i] != 0xffffffff && !(nodes[sorted_children[i]].prim_index & LEAF_NODE_BIT)) {
            FlattenCompactBVH_r(nodes, sorted_children[i], out_nodes, out_leaf_prims, int(child_base + child_rank),
                                depth + 1);
            ++child_rank;
        }
    }

    gpu_cwbvh_node_t &new_node = out_nodes[new_node_index];
    assert(child_rank == internal_count);
    // child_base lives in the low 24 bits of the packed word
    assert(child_base < (1u << 24));
    new_node.child_base_imask = child_base | (uint32_t(internal_mask) << 24);
    new_node.prim_base = prim_base;
    new_node.meta4[0] = uint32_t(meta_bytes[0]) | (uint32_t(meta_bytes[1]) << 8) | (uint32_t(meta_bytes[2]) << 16) |
                        (uint32_t(meta_bytes[3]) << 24);
    new_node.meta4[1] = uint32_t(meta_bytes[4]) | (uint32_t(meta_bytes[5]) << 8) | (uint32_t(meta_bytes[6]) << 16) |
                        (uint32_t(meta_bytes[7]) << 24);
    new_node.bbox_min = Ren::Vec3f{node_min[0], node_min[1], node_min[2]};
    memcpy(new_node.ch_exp, node_exp, 3);
    new_node.ch_exp[3] = 0;
    for (int j = 0; j < 3; ++j) {
        memcpy(new_node.ch_bbox_min[j], new_ch_min[j], 8);
        memcpy(new_node.ch_bbox_max[j], new_ch_max[j], 8);
    }

    return new_node_index;
}

void Eng::SceneManager::RebuildLightTree() {
    scene_data_.persistent_data->stoch_lights = {};
    scene_data_.persistent_data->stoch_lights_nodes = {};

    const Ren::ApiContext &api = ren_ctx_.api();
    const Ren::StoragesRef &storages = ren_ctx_.storages();

    const auto *transforms = (Transform *)scene_data_.comp_store[CompTransform]->SequentialData();
    const auto *acc_structs = (AccStructure *)scene_data_.comp_store[CompAccStructure]->SequentialData();

    struct additional_data_t {
        Ren::Vec3f axis;
        float flux, omega_n, omega_e;
    };
    std::vector<additional_data_t> additional_data;

    std::vector<light_item_t> stochastic_lights;
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

        const auto &[mesh_main, mesh_cold] = storages.meshes[acc.mesh];
        assert(mesh_main.type == Ren::eMeshType::Simple);
        const int VertexStride = 13;

        Ren::Span<const float> positions = mesh_cold.attribs;
        Ren::Span<const uint32_t> tri_indices = mesh_cold.indices;
        const uint32_t indices_start = mesh_main.indices_buf.sub.offset / sizeof(uint32_t);

        const Ren::Span<const Ren::tri_group_t> groups = mesh_cold.groups;
        for (int j = 0; j < int(groups.size()); ++j) {
            const Ren::tri_group_t &grp = groups[j];

            const Ren::MaterialHandle front_mat =
                (j >= acc.material_override.size()) ? grp.front_mat : acc.material_override[j][0];
            const auto &[front_main, front_cold] = storages.materials[front_mat];

            const Ren::MaterialHandle back_mat =
                (j >= acc.material_override.size()) ? grp.back_mat : acc.material_override[j][1];
            const auto &[back_main, back_cold] = storages.materials[back_mat];

            if (!front_mat || (front_main.flags & Ren::eMatFlags::Emissive) == 0) {
                continue;
            }

            const bool doublesided = (back_main.flags & Ren::eMatFlags::Emissive) != 0;

            const uint32_t index_beg = grp.byte_offset / sizeof(uint32_t);
            const uint32_t index_end = index_beg + grp.num_indices;

            for (uint32_t k = index_beg; k < index_end; k += 3) {
                const uint32_t i0 = tri_indices[k + 0], i1 = tri_indices[k + 1], i2 = tri_indices[k + 2];

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

                    const float lum = front_cold.params[3][1] + front_cold.params[3][2] + front_cold.params[3][3];
                    const float flux = lum * area;
                    additional_data.push_back({axis, flux, omega_n, omega_e});

                    temp_primitives.push_back({0, 0, 0, bbox_min, bbox_max});
                    temp_indices.push_back(k / 3);

                    light_item_t &tri_light = stochastic_lights.emplace_back();
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
                    memcpy(tri_light.col, ValuePtr(front_cold.params[3]) + 1, 3 * sizeof(float));
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
                    tri_light.shadowreg_index = front_mat.index * MAX_TEX_PER_MATERIAL + MAT_TEX_EMISSION;
                    // store triangle index
                    tri_light.tri_index = (indices_start + k) / 3;
                }
            }
        }
    }

    if (stochastic_lights.empty()) {
        return;
    }

    std::vector<gpu_light_cwbvh_node_t> light_wnodes;
    std::vector<uint32_t> light_leaf_lights;  // meta4 leaf payloads (light references)
    { // Build BVH
        std::vector<uint32_t> prim_indices;
        prim_indices.reserve(temp_primitives.size());

        std::vector<gpu_bvh_node_t> temp_nodes;

        Phy::split_settings_t s;
        s.oversplit_threshold = -1.0f;
        s.min_primitives_in_leaf = 1;
        PreprocessPrims_SAH(temp_primitives, s, 1, temp_nodes, prim_indices);

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
        std::vector<bool> processed(light_nodes.size(), false);
        std::vector<uint32_t> to_process = leaf_indices;
        while (!to_process.empty()) {
            uint32_t next_count = 0;
            for (uint32_t i = 0; i < uint32_t(to_process.size()); ++i) {
                const uint32_t n = to_process[i];
                const uint32_t parent = parent_indices[n];
                if (parent == 0xffffffff) {
                    continue;
                }

                assert(!processed[n]);
                processed[n] = true;

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

                const uint32_t left_child = light_nodes[parent].left_child & LEFT_CHILD_BITS,
                               right_child = light_nodes[parent].right_child & RIGHT_CHILD_BITS;
                if (processed[left_child] && processed[right_child]) {
                    assert(next_count <= i);
                    to_process[next_count++] = parent;
                }
            }
            to_process.resize(next_count);
        }

        // Remove indices indirection
        for (uint32_t i = 0; i < leaf_indices.size(); ++i) {
            gpu_light_bvh_node_t &n = light_nodes[leaf_indices[i]];
            assert((n.prim_index & LEAF_NODE_BIT) != 0);
            const uint32_t li_index = prim_indices[n.prim_index & PRIM_INDEX_BITS];
            n.prim_index &= ~PRIM_INDEX_BITS;
            n.prim_index |= li_index;
        }

        // Flatten to compact 8-wide BVH (the leaf level is collapsed into light_leaf_lights)
        [[maybe_unused]] const uint32_t root_node =
            FlattenLightBVH_r(light_nodes, 0, light_wnodes, light_leaf_lights);
        assert(root_node == 0);
    }

    { // Init GPU data
        const uint32_t lights_buf_size = uint32_t(stochastic_lights.size() * sizeof(light_item_t));
        scene_data_.persistent_data->stoch_lights =
            ren_ctx_.CreateBuffer(Ren::String{"Stochastic Lights"}, Ren::eBufType::Texture, lights_buf_size);
        ren_ctx_.FindOrCreateBufferView(scene_data_.persistent_data->stoch_lights, Ren::eFormat::RGBA32UI);

        const uint32_t nodes_buf_size = uint32_t(light_wnodes.size() * sizeof(gpu_light_cwbvh_node_t));
        scene_data_.persistent_data->stoch_lights_nodes =
            ren_ctx_.CreateBuffer(Ren::String{"Stochastic Light Nodes"}, Ren::eBufType::Texture, nodes_buf_size);
        ren_ctx_.FindOrCreateBufferView(scene_data_.persistent_data->stoch_lights_nodes, Ren::eFormat::RGBA32F);

        const uint32_t leaf_prims_buf_size = uint32_t(light_leaf_lights.size() * sizeof(uint32_t));
        scene_data_.persistent_data->stoch_lights_leaf_prims =
            ren_ctx_.CreateBuffer(Ren::String{"Stochastic Light Leaf Prims"}, Ren::eBufType::Texture,
                                  leaf_prims_buf_size);
        ren_ctx_.FindOrCreateBufferView(scene_data_.persistent_data->stoch_lights_leaf_prims, Ren::eFormat::R32UI);

        Ren::BufferMain lights_stage_buf_main = {};
        Ren::BufferCold lights_stage_buf_cold = {};
        if (!Ren::Buffer_Init(api, lights_stage_buf_main, lights_stage_buf_cold, Ren::String{"Stochastic Lights Stage"},
                              Ren::eBufType::Upload, lights_buf_size, ren_ctx_.log())) {
            // TODO: Properly handle failure
            assert(false);
        }

        { // init stage buf
            uint8_t *mapped_ptr = Buffer_Map(api, lights_stage_buf_main, lights_stage_buf_cold);
            memcpy(mapped_ptr, stochastic_lights.data(), stochastic_lights.size() * sizeof(light_item_t));
            Buffer_Unmap(api, lights_stage_buf_main, lights_stage_buf_cold);
        }

        Ren::BufferMain nodes_stage_buf_main = {};
        Ren::BufferCold nodes_stage_buf_cold = {};
        if (!Ren::Buffer_Init(api, nodes_stage_buf_main, nodes_stage_buf_cold,
                              Ren::String{"Stochastic Light Nodes Stage"}, Ren::eBufType::Upload, nodes_buf_size,
                              ren_ctx_.log())) {
            // TODO: Properly handle failure
            assert(false);
        }

        { // init stage buf
            uint8_t *mapped_ptr = Buffer_Map(api, nodes_stage_buf_main, nodes_stage_buf_cold);
            memcpy(mapped_ptr, light_wnodes.data(), light_wnodes.size() * sizeof(gpu_light_cwbvh_node_t));
            Buffer_Unmap(api, nodes_stage_buf_main, nodes_stage_buf_cold);
        }

        Ren::BufferMain leaf_prims_stage_buf_main = {};
        Ren::BufferCold leaf_prims_stage_buf_cold = {};
        if (!Ren::Buffer_Init(api, leaf_prims_stage_buf_main, leaf_prims_stage_buf_cold,
                              Ren::String{"Stochastic Light Leaf Prims Stage"}, Ren::eBufType::Upload,
                              leaf_prims_buf_size, ren_ctx_.log())) {
            // TODO: Properly handle failure
            assert(false);
        }

        { // init stage buf
            uint8_t *mapped_ptr = Buffer_Map(api, leaf_prims_stage_buf_main, leaf_prims_stage_buf_cold);
            memcpy(mapped_ptr, light_leaf_lights.data(), leaf_prims_buf_size);
            Buffer_Unmap(api, leaf_prims_stage_buf_main, leaf_prims_stage_buf_cold);
        }

        Ren::CommandBuffer cmd_buf = ren_ctx_.BegTempSingleTimeCommands();

        const auto &[lights_main, lights_cold] = storages.buffers[scene_data_.persistent_data->stoch_lights];
        CopyBufferToBuffer(api, lights_stage_buf_main, 0, lights_main, 0, lights_buf_size, cmd_buf);

        const auto &[nodes_main, nodes_cold] = storages.buffers[scene_data_.persistent_data->stoch_lights_nodes];
        CopyBufferToBuffer(api, nodes_stage_buf_main, 0, nodes_main, 0, nodes_buf_size, cmd_buf);

        const auto &[leaf_prims_main, leaf_prims_cold] =
            storages.buffers[scene_data_.persistent_data->stoch_lights_leaf_prims];
        CopyBufferToBuffer(api, leaf_prims_stage_buf_main, 0, leaf_prims_main, 0, leaf_prims_buf_size, cmd_buf);

        ren_ctx_.EndTempSingleTimeCommands(cmd_buf);

        Buffer_DestroyImmediately(api, lights_stage_buf_main, lights_stage_buf_cold);
        Buffer_DestroyImmediately(api, nodes_stage_buf_main, nodes_stage_buf_cold);
        Buffer_DestroyImmediately(api, leaf_prims_stage_buf_main, leaf_prims_stage_buf_cold);
    }
}

void Eng::SceneManager::ReleaseLightTree(const bool immediate) {
    const Ren::StoragesRef &storages = ren_ctx_.storages();
    if (scene_data_.persistent_data->stoch_lights) {
        const auto &[lights_main, lights_cold] = storages.buffers[scene_data_.persistent_data->stoch_lights];
        if (immediate) {
            Buffer_DestroyImmediately(ren_ctx_.api(), lights_main, lights_cold);
        } else {
            Buffer_Destroy(ren_ctx_.api(), lights_main, lights_cold);
        }
    }
    if (scene_data_.persistent_data->stoch_lights_nodes) {
        const auto &[nodes_main, nodes_cold] = storages.buffers[scene_data_.persistent_data->stoch_lights_nodes];
        if (immediate) {
            Buffer_DestroyImmediately(ren_ctx_.api(), nodes_main, nodes_cold);
        } else {
            Buffer_Destroy(ren_ctx_.api(), nodes_main, nodes_cold);
        }
    }
    if (scene_data_.persistent_data->stoch_lights_leaf_prims) {
        const auto &[leaf_prims_main, leaf_prims_cold] =
            storages.buffers[scene_data_.persistent_data->stoch_lights_leaf_prims];
        if (immediate) {
            Buffer_DestroyImmediately(ren_ctx_.api(), leaf_prims_main, leaf_prims_cold);
        } else {
            Buffer_Destroy(ren_ctx_.api(), leaf_prims_main, leaf_prims_cold);
        }
    }
}
