#pragma once

#include "../framegraph/FgNode.h"

namespace Phy {
struct prim_t;
struct split_settings_t;
} // namespace Phy

namespace Eng {
struct gpu_bvh_node_t;
struct gpu_cwbvh_node_t;
struct DrawList;
struct mesh_t;

class ExBuildAccStructures final : public FgExecutor {
    const DrawList *&p_list_;
    int rt_index_;
    Ren::AccStructHandle rt_tlas_;
    Ren::Span<const mesh_t> rt_meshes_;

    FgBufROHandle rt_obj_instances_ro_;
    FgBufRWHandle rt_obj_instances_rw_;
    FgBufRWHandle rt_tlas_buf_;
    FgBufRWHandle rt_tlas_leaf_prims_buf_;
    FgBufRWHandle rt_tlas_build_scratch_;

    void Execute_HWRT(const FgContext &fg);
    void Execute_SWRT(const FgContext &fg);

    static uint32_t PreprocessPrims_SAH(Ren::Span<const Phy::prim_t> prims, const Phy::split_settings_t &s,
                                        std::vector<gpu_bvh_node_t> &out_nodes, std::vector<uint32_t> &out_indices);
    static uint32_t FlattenCompactBVH_r(Ren::Span<const gpu_bvh_node_t> nodes, uint32_t node_index,
                                        std::vector<gpu_cwbvh_node_t> &out_nodes,
                                        std::vector<uint32_t> &out_leaf_prims, int at_index = -1, uint32_t depth = 0);

  public:
    ExBuildAccStructures(const DrawList *&p_list, int rt_index, const FgBufROHandle rt_obj_instances,
                         Ren::AccStructHandle rt_tlas, Ren::Span<const mesh_t> rt_meshes,
                         const FgBufRWHandle rt_tlas_buf, const FgBufRWHandle rt_tlas_leaf_prims_buf,
                         const FgBufRWHandle rt_tlas_scratch)
        : p_list_(p_list), rt_index_(rt_index), rt_tlas_(rt_tlas), rt_meshes_(rt_meshes),
          rt_obj_instances_ro_(rt_obj_instances), rt_tlas_buf_(rt_tlas_buf),
          rt_tlas_leaf_prims_buf_(rt_tlas_leaf_prims_buf), rt_tlas_build_scratch_(rt_tlas_scratch) {}

    ExBuildAccStructures(const DrawList *&p_list, int rt_index, const FgBufRWHandle rt_obj_instances,
                         Ren::AccStructHandle rt_tlas, Ren::Span<const mesh_t> rt_meshes,
                         const FgBufRWHandle rt_tlas_buf, const FgBufRWHandle rt_tlas_leaf_prims_buf,
                         const FgBufRWHandle rt_tlas_scratch)
        : p_list_(p_list), rt_index_(rt_index), rt_tlas_(rt_tlas), rt_meshes_(rt_meshes),
          rt_obj_instances_rw_(rt_obj_instances), rt_tlas_buf_(rt_tlas_buf),
          rt_tlas_leaf_prims_buf_(rt_tlas_leaf_prims_buf), rt_tlas_build_scratch_(rt_tlas_scratch) {}

    void Execute(const FgContext &fg) override;
};
} // namespace Eng