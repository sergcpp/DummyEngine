#pragma once

#include "../Renderer_DrawList.h"
#include "../framegraph/FgNode.h"

namespace Phy {
struct prim_t;
struct split_settings_t;
} // namespace Phy

namespace Eng {
class ExBuildAccStructures final : public FgExecutor {
    const DrawList *&p_list_;
    int rt_index_;
    const AccelerationStructureData *acc_struct_data_;

    FgResRef rt_obj_instances_buf_;
    FgResRef rt_tlas_buf_;
    FgResRef rt_tlas_build_scratch_buf_;

    void Execute_HWRT(FgBuilder &builder);
    void Execute_SWRT(FgBuilder &builder);

    static uint32_t PreprocessPrims_SAH(Ren::Span<const Phy::prim_t> prims, const Phy::split_settings_t &s,
                                        std::vector<gpu_bvh_node_t> &out_nodes, std::vector<uint32_t> &out_indices);

  public:
    ExBuildAccStructures(const DrawList *&p_list, int rt_index, const FgResRef rt_obj_instances_buf,
                         const AccelerationStructureData *acc_struct_data, const FgResRef rt_tlas_buf,
                         const FgResRef rt_tlas_scratch_buf)
        : p_list_(p_list), rt_index_(rt_index), rt_obj_instances_buf_(rt_obj_instances_buf),
          acc_struct_data_(acc_struct_data), rt_tlas_buf_(rt_tlas_buf),
          rt_tlas_build_scratch_buf_(rt_tlas_scratch_buf) {}

    void Execute(FgBuilder &builder) override;
};
} // namespace Eng