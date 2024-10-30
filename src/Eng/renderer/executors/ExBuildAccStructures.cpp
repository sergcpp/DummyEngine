#include "ExBuildAccStructures.h"

#include <deque>

#include <Phy/BVHSplit.h>
#include <Ren/Context.h>

void Eng::ExBuildAccStructures::Execute(FgBuilder &builder) {
    if (builder.ctx().capabilities.hwrt) {
#if !defined(USE_GL_RENDER)
        Execute_HWRT(builder);
#endif
    } else {
        Execute_SWRT(builder);
    }
}

void Eng::ExBuildAccStructures::Execute_SWRT(FgBuilder &builder) {
    FgAllocBuf &rt_obj_instances_buf = builder.GetWriteBuffer(rt_obj_instances_buf_);
    FgAllocBuf &rt_tlas_buf = builder.GetWriteBuffer(rt_tlas_buf_);

    Ren::Context &ctx = builder.ctx();

    const auto &rt_obj_instances = p_list_->rt_obj_instances[rt_index_];
    auto &rt_obj_instances_stage_buf = p_list_->rt_obj_instances_stage_buf[rt_index_];
    auto &rt_tlas_stage_buf = p_list_->swrt.rt_tlas_nodes_stage_buf[rt_index_];

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

        std::vector<gpu_bvh2_node_t> temp_bvh2_nodes;
        ConvertToBVH2(temp_nodes, temp_bvh2_nodes);

        for (uint32_t i = 0; i < uint32_t(temp_bvh2_nodes.size()); ++i) {
            gpu_bvh2_node_t &n = temp_bvh2_nodes[i];
            if ((n.left_child & BVH2_PRIM_COUNT_BITS) != 0) {
                n.left_child = (n.left_child & BVH2_PRIM_COUNT_BITS) | mi_indices[n.left_child & BVH2_PRIM_INDEX_BITS];
            }
            if ((n.right_child & BVH2_PRIM_COUNT_BITS) != 0) {
                n.right_child =
                    (n.right_child & BVH2_PRIM_COUNT_BITS) | mi_indices[n.right_child & BVH2_PRIM_INDEX_BITS];
            }
        }

        std::vector<gpu_mesh_instance_t> mesh_instances;
        mesh_instances.reserve(rt_obj_instances.count);

        for (int i = 0; i < int(rt_obj_instances.count); ++i) {
            const auto &inst = rt_obj_instances.data[i];
            const auto *acc = reinterpret_cast<const Ren::AccStructureSW *>(inst.blas_ref);
            const mesh_t &mesh = rt_meshes_[acc->mesh_index];
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
            uint8_t *stage_mem = rt_obj_instances_stage_buf->MapRange(
                ctx.backend_frame() * SWRTObjInstancesBufChunkSize, SWRTObjInstancesBufChunkSize);
            const uint32_t rt_obj_instances_mem_size = uint32_t(mesh_instances.size()) * sizeof(gpu_mesh_instance_t);
            if (stage_mem) {
                memcpy(stage_mem, mesh_instances.data(), rt_obj_instances_mem_size);
                rt_obj_instances_stage_buf->Unmap();
            } else {
                builder.log()->Error("ExBuildAccStructures: Failed to map rt obj instance buffer!");
            }

            CopyBufferToBuffer(*rt_obj_instances_stage_buf, ctx.backend_frame() * SWRTObjInstancesBufChunkSize,
                               *rt_obj_instances_buf.ref, 0, rt_obj_instances_mem_size, ctx.current_cmd_buf());
        }

        { // update nodes buf
            uint8_t *stage_mem =
                rt_tlas_stage_buf->MapRange(ctx.backend_frame() * SWRTTLASNodesBufChunkSize, SWRTTLASNodesBufChunkSize);
            const uint32_t rt_nodes_mem_size = uint32_t(temp_bvh2_nodes.size()) * sizeof(gpu_bvh2_node_t);
            if (stage_mem) {
                memcpy(stage_mem, temp_bvh2_nodes.data(), rt_nodes_mem_size);
                rt_tlas_stage_buf->Unmap();
            } else {
                builder.log()->Error("ExBuildAccStructures: Failed to map rt tlas stage buffer!");
            }

            CopyBufferToBuffer(*rt_tlas_stage_buf, ctx.backend_frame() * SWRTTLASNodesBufChunkSize, *rt_tlas_buf.ref, 0,
                               rt_nodes_mem_size, ctx.current_cmd_buf());
        }
    } else {
        const gpu_bvh2_node_t dummy_node = {};
        rt_tlas_buf.ref->UpdateImmediate(0, sizeof(dummy_node), &dummy_node, ctx.current_cmd_buf());
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

// TODO: avoid duplication with SceneManager::ConvertToBVH2
uint32_t Eng::ExBuildAccStructures::ConvertToBVH2(Ren::Span<const gpu_bvh_node_t> nodes,
                                                  std::vector<gpu_bvh2_node_t> &out_nodes) {
    const uint32_t out_index = uint32_t(out_nodes.size());

    if (nodes.size() == 1) {
        assert((nodes[0].prim_index & LEAF_NODE_BIT) != 0);
        gpu_bvh2_node_t root_node = {};

        root_node.left_child = nodes[0].prim_index & PRIM_INDEX_BITS;
        const uint32_t prim_count = std::max(nodes[0].prim_count & PRIM_COUNT_BITS, 2u);
        assert(prim_count <= 8);
        root_node.left_child |= (prim_count - 1) << 29;
        root_node.right_child = root_node.left_child;

        const gpu_bvh_node_t &ch0 = nodes[0];

        root_node.ch_data0[0] = ch0.bbox_min[0];
        root_node.ch_data0[1] = ch0.bbox_max[0];
        root_node.ch_data0[2] = ch0.bbox_min[1];
        root_node.ch_data0[3] = ch0.bbox_max[1];
        root_node.ch_data2[0] = ch0.bbox_min[2];
        root_node.ch_data2[1] = ch0.bbox_max[2];

        out_nodes.push_back(root_node);

        return out_index;
    }

    std::vector<uint32_t> compacted_indices;
    compacted_indices.resize(nodes.size());
    uint32_t compacted_count = 0;
    for (int i = 0; i < int(nodes.size()); ++i) {
        compacted_indices[i] = compacted_count;
        if ((nodes[i].prim_index & LEAF_NODE_BIT) == 0) {
            ++compacted_count;
        }
    }
    out_nodes.reserve(out_nodes.size() + compacted_count);

    const uint32_t offset = uint32_t(out_nodes.size());
    for (const gpu_bvh_node_t &n : nodes) {
        gpu_bvh2_node_t new_node = {};
        new_node.left_child = n.left_child;
        new_node.right_child = n.right_child & RIGHT_CHILD_BITS;
        if ((n.prim_index & LEAF_NODE_BIT) == 0) {
            const gpu_bvh_node_t &ch0 = nodes[new_node.left_child];
            const gpu_bvh_node_t &ch1 = nodes[new_node.right_child];

            if ((ch0.prim_index & LEAF_NODE_BIT) != 0) {
                new_node.left_child = ch0.prim_index & PRIM_INDEX_BITS;

                const uint32_t prim_count = std::max(ch0.prim_count & PRIM_COUNT_BITS, 2u);
                assert(prim_count <= 8);
                new_node.left_child |= (prim_count - 1) << 29;
                assert((new_node.left_child & BVH2_PRIM_COUNT_BITS) != 0);
            } else {
                new_node.left_child = compacted_indices[new_node.left_child];
                new_node.left_child += offset;
                assert((new_node.left_child & BVH2_PRIM_COUNT_BITS) == 0);
                assert(new_node.left_child < out_nodes.capacity());
            }
            if ((ch1.prim_index & LEAF_NODE_BIT) != 0) {
                new_node.right_child = ch1.prim_index & PRIM_INDEX_BITS;

                const uint32_t prim_count = std::max(ch1.prim_count & PRIM_COUNT_BITS, 2u);
                assert(prim_count <= 8);
                new_node.right_child |= (prim_count - 1) << 29;
                assert((new_node.right_child & BVH2_PRIM_COUNT_BITS) != 0);
            } else {
                new_node.right_child = compacted_indices[new_node.right_child];
                new_node.right_child += offset;
                assert((new_node.right_child & BVH2_PRIM_COUNT_BITS) == 0);
                assert(new_node.right_child < out_nodes.capacity());
            }

            new_node.ch_data0[0] = ch0.bbox_min[0];
            new_node.ch_data0[1] = ch0.bbox_max[0];
            new_node.ch_data0[2] = ch0.bbox_min[1];
            new_node.ch_data0[3] = ch0.bbox_max[1];

            new_node.ch_data1[0] = ch1.bbox_min[0];
            new_node.ch_data1[1] = ch1.bbox_max[0];
            new_node.ch_data1[2] = ch1.bbox_min[1];
            new_node.ch_data1[3] = ch1.bbox_max[1];

            new_node.ch_data2[0] = ch0.bbox_min[2];
            new_node.ch_data2[1] = ch0.bbox_max[2];
            new_node.ch_data2[2] = ch1.bbox_min[2];
            new_node.ch_data2[3] = ch1.bbox_max[2];

            out_nodes.push_back(new_node);
        }
    }

    assert(out_nodes.size() == out_nodes.capacity());

    return out_index;
}