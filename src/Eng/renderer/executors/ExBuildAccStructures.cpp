#include "ExBuildAccStructures.h"

#include <cfloat>
#include <cmath>
#include <deque>

#include <Phy/BVHSplit.h>
#include <Ren/Context.h>

#include "../Renderer_DrawList.h"
#include "../framegraph/FgBuilder.h"

void Eng::ExBuildAccStructures::Execute(const FgContext &fg) {
    if (fg.ren_ctx().capabilities.hwrt) {
#if !defined(REN_GL_BACKEND)
        Execute_HWRT(fg);
#endif
    } else {
        Execute_SWRT(fg);
    }
}

void Eng::ExBuildAccStructures::Execute_SWRT(const FgContext &fg) {
    const Ren::BufferHandle rt_obj_instances_buf = fg.AccessRWBuffer(rt_obj_instances_rw_);
    const Ren::BufferHandle rt_tlas = fg.AccessRWBuffer(rt_tlas_buf_);
    const Ren::BufferHandle rt_tlas_leaf_prims = fg.AccessRWBuffer(rt_tlas_leaf_prims_buf_);

    const auto &rt_obj_instances = p_list_->rt_obj_instances[rt_index_];
    const Ren::BufferHandle rt_obj_instances_stage = p_list_->rt_obj_instances_stage[rt_index_];
    const Ren::BufferHandle rt_tlas_stage = p_list_->swrt.rt_tlas_nodes_stage[rt_index_];

    const Ren::ApiContext &api = fg.ren_ctx().api();
    const Ren::StoragesRef &storages = fg.storages();

    if (rt_obj_instances.count) {
        std::vector<Phy::prim_t> temp_primitives;

        for (uint32_t i = 0; i < rt_obj_instances.count; ++i) {
            const auto &inst = rt_obj_instances.data[i];

            temp_primitives.emplace_back();
            auto &new_prim = temp_primitives.back();

            memcpy(&new_prim.bbox_min[0], inst.bbox_min_ws, 3 * sizeof(float));
            memcpy(&new_prim.bbox_max[0], inst.bbox_max_ws, 3 * sizeof(float));
        }

        std::vector<gpu_bvh_node_t> temp_nodes;
        std::vector<uint32_t> mi_indices;

        Phy::split_settings_t s;
        s.oversplit_threshold = -1.0f;
        s.min_primitives_in_leaf = 1;
        const uint32_t nodes_count = PreprocessPrims_SAH(temp_primitives, s, temp_nodes, mi_indices);
        assert(nodes_count <= MAX_RT_TLAS_NODES);

        std::vector<gpu_cwbvh_node_t> temp_cw_nodes;
        std::vector<uint32_t> temp_leaf_prims; // indices into mi_indices
        [[maybe_unused]] const uint32_t root_node = FlattenCompactBVH_r(temp_nodes, 0, temp_cw_nodes, temp_leaf_prims);
        assert(root_node == 0);

        // resolve leaf payloads to mesh instance indices
        for (uint32_t &p : temp_leaf_prims) {
            p = mi_indices[p & PRIM_INDEX_BITS];
        }

        std::vector<swrt_mesh_instance_t> mesh_instances;
        mesh_instances.reserve(rt_obj_instances.count);

        for (int i = 0; i < int(rt_obj_instances.count); ++i) {
            const auto &inst = rt_obj_instances.data[i];
            const auto &[acc_main, acc_cold] = storages.acc_structs[inst.blas];

            const mesh_t &mesh = rt_meshes_[acc_main.sw.mesh_index];
            auto &new_mi = mesh_instances.emplace_back();

            new_mi.visibility = inst.mask;
            new_mi.node_index = mesh.node_index;
            new_mi.vert_index = mesh.vert_index;
            assert(inst.geo_index <= 0x00ffffff && inst.geo_count <= 0xff);
            new_mi.geo_index_count = inst.geo_index | (mesh.geo_count << 24);

            Ren::Mat4f transform;
            memcpy(&transform[0][0], inst.xform, 12 * sizeof(float));
            const Ren::Mat4f inv_transform = InverseAffine(transform);

            memcpy(&new_mi.inv_transform[0][0], ValuePtr(inv_transform), 12 * sizeof(float));
            memcpy(&new_mi.transform[0][0], ValuePtr(transform), 12 * sizeof(float));
        }

        { // update instances buf
            const auto &[rt_obj_instances_stage_buf_main, rt_obj_instances_stage_buf_cold] =
                storages.buffers[rt_obj_instances_stage];

            uint8_t *stage_mem =
                Buffer_MapRange(api, rt_obj_instances_stage_buf_main, rt_obj_instances_stage_buf_cold,
                                fg.backend_frame() * SWRTObjInstancesBufChunkSize, SWRTObjInstancesBufChunkSize);
            const uint32_t rt_obj_instances_mem_size = uint32_t(mesh_instances.size()) * sizeof(swrt_mesh_instance_t);
            if (stage_mem) {
                memcpy(stage_mem, mesh_instances.data(), rt_obj_instances_mem_size);
                Buffer_Unmap(api, rt_obj_instances_stage_buf_main, rt_obj_instances_stage_buf_cold);
            } else {
                fg.log()->Error("ExBuildAccStructures: Failed to map rt obj instance buffer!");
            }

            Ren::BufferMain &rt_obj_instances_buf_main = storages.buffers[rt_obj_instances_buf].first;
            CopyBufferToBuffer(api, rt_obj_instances_stage_buf_main, fg.backend_frame() * SWRTObjInstancesBufChunkSize,
                               rt_obj_instances_buf_main, 0, rt_obj_instances_mem_size, fg.cmd_buf());
        }
        { // update nodes buf
            const auto &[rt_tlas_stage_buf_main, rt_tlas_stage_buf_cold] = storages.buffers[rt_tlas_stage];

            uint8_t *stage_mem =
                Buffer_MapRange(api, rt_tlas_stage_buf_main, rt_tlas_stage_buf_cold,
                                fg.backend_frame() * SWRTTLASNodesBufChunkSize, SWRTTLASNodesBufChunkSize);
            const uint32_t rt_nodes_mem_size = uint32_t(temp_cw_nodes.size()) * sizeof(gpu_cwbvh_node_t);
            const uint32_t rt_leaf_prims_mem_size = uint32_t(temp_leaf_prims.size()) * sizeof(uint32_t);
            if (stage_mem) {
                memcpy(stage_mem, temp_cw_nodes.data(), rt_nodes_mem_size);
                memcpy(stage_mem + SWRTTLASLeafPrimsOffset, temp_leaf_prims.data(), rt_leaf_prims_mem_size);
                Buffer_Unmap(api, rt_tlas_stage_buf_main, rt_tlas_stage_buf_cold);
            } else {
                fg.log()->Error("ExBuildAccStructures: Failed to map rt tlas stage buffer!");
            }

            Ren::BufferMain &rt_tlas_buf_main = storages.buffers[rt_tlas].first;
            CopyBufferToBuffer(api, rt_tlas_stage_buf_main, fg.backend_frame() * SWRTTLASNodesBufChunkSize,
                               rt_tlas_buf_main, 0, rt_nodes_mem_size, fg.cmd_buf());

            Ren::BufferMain &rt_tlas_leaf_prims_buf_main = storages.buffers[rt_tlas_leaf_prims].first;
            CopyBufferToBuffer(api, rt_tlas_stage_buf_main,
                               fg.backend_frame() * SWRTTLASNodesBufChunkSize + SWRTTLASLeafPrimsOffset,
                               rt_tlas_leaf_prims_buf_main, 0, rt_leaf_prims_mem_size, fg.cmd_buf());
        }
    } else {
        static const gpu_cwbvh_node_t dummy_node = {};

        Ren::BufferMain &rt_tlas_buf_main = storages.buffers[rt_tlas].first;
        Buffer_UpdateInPlace(api, rt_tlas_buf_main, 0, sizeof(dummy_node), &dummy_node, fg.cmd_buf());
    }
}

// TODO: avoid duplication with SceneManager::PreprocessPrims_SAH
uint32_t Eng::ExBuildAccStructures::PreprocessPrims_SAH(Ren::Span<const Phy::prim_t> prims,
                                                        const Phy::split_settings_t &s,
                                                        std::vector<gpu_bvh_node_t> &out_nodes,
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

namespace {
// CWBVH quantization helpers (mirrors SceneManagerInternal in SceneManager_BVH.cpp)
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
} // namespace

// TODO: avoid duplication with SceneManager::FlattenCompactBVH_r
uint32_t Eng::ExBuildAccStructures::FlattenCompactBVH_r(Ren::Span<const gpu_bvh_node_t> nodes,
                                                        const uint32_t node_index,
                                                        std::vector<gpu_cwbvh_node_t> &out_nodes,
                                                        std::vector<uint32_t> &out_leaf_prims, const int at_index,
                                                        const uint32_t depth) {
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
    // per parent in slot order (leaf_prims[prim_base + rank]).
    const uint32_t prim_base = uint32_t(out_leaf_prims.size());
    uint8_t meta_bytes[8] = {};
    int leaf_rank = 0;
    for (int i = 0; i < 8; i++) {
        if (sorted_children[i] == 0xffffffff) {
            continue;
        }
        if (nodes[sorted_children[i]].prim_index & LEAF_NODE_BIT) {
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