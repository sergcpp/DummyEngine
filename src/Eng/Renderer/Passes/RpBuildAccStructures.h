#pragma once

#include "../Graph/GraphBuilder.h"
#include "../Renderer_DrawList.h"

class RpBuildAccStructuresExecutor : public RenderPassExecutor {
    uint32_t instance_count_ = 0;
    const AccelerationStructureData *acc_struct_data_;

    RpResRef rt_obj_instances_buf_;
    RpResRef rt_tlas_buf_;
    RpResRef rt_tlas_build_scratch_buf_;

  public:
    RpBuildAccStructuresExecutor(const RpResRef rt_obj_instances_buf, uint32_t instance_count,
                                 const AccelerationStructureData *acc_struct_data, const RpResRef rt_tlas_buf,
                                 const RpResRef rt_tlas_scratch_buf)
        : rt_obj_instances_buf_(rt_obj_instances_buf), instance_count_(instance_count),
          acc_struct_data_(acc_struct_data), rt_tlas_buf_(rt_tlas_buf),
          rt_tlas_build_scratch_buf_(rt_tlas_scratch_buf) {}

    void Execute(RpBuilder &builder) override;
};