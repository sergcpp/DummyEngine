#include "RpBuildAccStructures.h"

#include "../../Utils/BVHSplit.h"

void RpBuildAccStructuresExecutor::Execute(RpBuilder &builder) {
    if (builder.ctx().capabilities.raytracing) {
#if !defined(USE_GL_RENDER)
        Execute_HWRT(builder);
#endif
    } else {
        Execute_SWRT(builder);
    }
}

void RpBuildAccStructuresExecutor::Execute_SWRT(RpBuilder &builder) {
    RpAllocBuf &rt_obj_instances_buf = builder.GetWriteBuffer(rt_obj_instances_buf_);
    RpAllocBuf &rt_tlas_buf = builder.GetWriteBuffer(rt_tlas_buf_);
    // RpAllocBuf &rt_tlas_build_scratch_buf = builder.GetWriteBuffer(rt_tlas_build_scratch_buf_);

    Ren::Context &ctx = builder.ctx();

    std::vector<gpu_bvh_node_t> nodes;
    Ren::SmallVector<gpu_mesh_instance_t, REN_MAX_RT_OBJ_INSTANCES> mesh_instances;

    const auto &rt_obj_instances = p_list_->rt_obj_instances[rt_index_];
    auto &rt_obj_instances_stage_buf = p_list_->rt_obj_instances_stage_buf[rt_index_];

    auto &rt_tlas_stage_buf = p_list_->swrt.rt_tlas_nodes_stage_buf;

    if (rt_obj_instances.count) {
        std::vector<prim_t> temp_primitives;

        for (uint32_t i = 0; i < rt_obj_instances.count; ++i) {
            const auto &inst = rt_obj_instances.data[i];

            temp_primitives.emplace_back();
            auto &new_prim = temp_primitives.back();

            memcpy(&new_prim.bbox_min[0], inst.bbox_min_ws, 3 * sizeof(float));
            memcpy(&new_prim.bbox_max[0], inst.bbox_max_ws, 3 * sizeof(float));
        }

        std::vector<uint32_t> mi_indices;

        split_settings_t s;
        const uint32_t nodes_count = PreprocessPrims_SAH(temp_primitives, s, nodes, mi_indices);
        assert(nodes_count <= REN_MAX_RT_TLAS_NODES);

        for (const uint32_t i : mi_indices) {
            const auto &inst = rt_obj_instances.data[i];
            auto &new_mi = mesh_instances.emplace_back();

            memcpy(&new_mi.bbox_min[0], inst.bbox_min_ws, 3 * sizeof(float));
            memcpy(&new_mi.bbox_max[0], inst.bbox_max_ws, 3 * sizeof(float));

            new_mi.geo_index = inst.geo_index;

            const auto *acc = reinterpret_cast<const Ren::AccStructureSW *>(inst.blas_ref);
            new_mi.mesh_index = acc->mesh_index;

            Ren::Mat4f transform;
            memcpy(&transform[0][0], inst.xform, 12 * sizeof(float));

            // TODO: !!!!!!
            transform = Ren::Inverse(transform);

            memcpy(&new_mi.inv_transform[0][0], ValuePtr(transform), 12 * sizeof(float));
        }

        { // update instances buf
            uint8_t *stage_mem = rt_obj_instances_stage_buf->MapRange(
                Ren::BufMapWrite, ctx.backend_frame() * SWRTObjInstancesBufChunkSize, SWRTObjInstancesBufChunkSize);
            const uint32_t rt_obj_instances_mem_size = uint32_t(mesh_instances.size()) * sizeof(gpu_mesh_instance_t);
            if (stage_mem) {
                memcpy(stage_mem, mesh_instances.data(), rt_obj_instances_mem_size);

                rt_obj_instances_stage_buf->FlushMappedRange(
                    0, rt_obj_instances_stage_buf->AlignMapOffset(rt_obj_instances_mem_size));
                rt_obj_instances_stage_buf->Unmap();
            } else {
                builder.log()->Error("RpUpdateAccStructures: Failed to map rt obj instance buffer!");
            }

            Ren::CopyBufferToBuffer(*rt_obj_instances_stage_buf, ctx.backend_frame() * SWRTObjInstancesBufChunkSize,
                                    *rt_obj_instances_buf.ref, 0, rt_obj_instances_mem_size, ctx.current_cmd_buf());
        }

        { // update nodes buf
            uint8_t *stage_mem = rt_tlas_stage_buf->MapRange(
                Ren::BufMapWrite, ctx.backend_frame() * SWRTTLASNodesBufChunkSize, SWRTTLASNodesBufChunkSize);
            const uint32_t rt_nodes_mem_size = uint32_t(nodes.size()) * sizeof(gpu_bvh_node_t);
            if (stage_mem) {
                memcpy(stage_mem, nodes.data(), rt_nodes_mem_size);

                rt_tlas_stage_buf->FlushMappedRange(0, rt_tlas_stage_buf->AlignMapOffset(rt_nodes_mem_size));
                rt_tlas_stage_buf->Unmap();
            } else {
                builder.log()->Error("RpUpdateAccStructures: Failed to map rt tlas stage buffer!");
            }

            Ren::CopyBufferToBuffer(*rt_tlas_stage_buf, ctx.backend_frame() * SWRTTLASNodesBufChunkSize,
                                    *rt_tlas_buf.ref, 0, rt_nodes_mem_size, ctx.current_cmd_buf());
        }
    }
}

// TODO: avoid duplication with SceneManager::PreprocessPrims_SAH
uint32_t RpBuildAccStructuresExecutor::PreprocessPrims_SAH(Ren::Span<const prim_t> prims, const split_settings_t &s,
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