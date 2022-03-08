#pragma once

#include "../Graph/GraphBuilder.h"
#include "../Renderer_DrawList.h"

class RpBuildAccStructures : public RenderPassBase {
    // temp data (valid only between Setup and Execute calls)
    uint32_t instance_count_ = 0;
    const AccelerationStructureData *acc_struct_data_;

    RpResource rt_obj_instances_buf_;
    RpResource rt_tlas_buf_;
    RpResource rt_tlas_build_scratch_buf_;

  public:
    void Setup(RpBuilder &builder, const char rt_obj_instances_buf[], uint32_t instance_count,
               const char rt_tlas_scratch_buf_name[], const AccelerationStructureData *acc_struct_data);
    void Execute(RpBuilder &builder) override;

    const char *name() const override { return "BUILD ACC STRCTS"; }
};