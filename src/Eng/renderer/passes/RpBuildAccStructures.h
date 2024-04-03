#pragma once

#include "../graph/SubPass.h"
#include "../Renderer_DrawList.h"

namespace Phy {
struct prim_t;
struct split_settings_t;
}

namespace Eng {
class RpBuildAccStructuresExecutor : public RpExecutor {
    const DrawList *&p_list_;
    int rt_index_;
    const AccelerationStructureData *acc_struct_data_;

    RpResRef rt_obj_instances_buf_;
    RpResRef rt_tlas_buf_;
    RpResRef rt_tlas_build_scratch_buf_;

    void Execute_HWRT(RpBuilder &builder);
    void Execute_SWRT(RpBuilder &builder);

    uint32_t PreprocessPrims_SAH(Ren::Span<const Phy::prim_t> prims, const Phy::split_settings_t &s,
                                 std::vector<gpu_bvh_node_t> &out_nodes, std::vector<uint32_t> &out_indices);

  public:
    RpBuildAccStructuresExecutor(const DrawList *&p_list, int rt_index, const RpResRef rt_obj_instances_buf,
                                 const AccelerationStructureData *acc_struct_data, const RpResRef rt_tlas_buf,
                                 const RpResRef rt_tlas_scratch_buf)
        : p_list_(p_list), rt_index_(rt_index), rt_obj_instances_buf_(rt_obj_instances_buf),
          acc_struct_data_(acc_struct_data), rt_tlas_buf_(rt_tlas_buf),
          rt_tlas_build_scratch_buf_(rt_tlas_scratch_buf) {}

    void Execute(RpBuilder &builder) override;
};
} // namespace Eng