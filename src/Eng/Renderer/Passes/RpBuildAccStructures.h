#pragma once

#include "../Graph/GraphBuilder.h"
#include "../Renderer_DrawList.h"

class RpBuildAccStructuresExecutor : public RpExecutor {
    const DrawList *&p_list_;
    int rt_index_;
    const AccelerationStructureData *acc_struct_data_;

    RpResRef rt_obj_instances_buf_;
    RpResRef rt_tlas_buf_;
    RpResRef rt_tlas_build_scratch_buf_;

  public:
    RpBuildAccStructuresExecutor(const DrawList *&p_list, int rt_index, const RpResRef rt_obj_instances_buf,
                                 const AccelerationStructureData *acc_struct_data, const RpResRef rt_tlas_buf,
                                 const RpResRef rt_tlas_scratch_buf)
        : p_list_(p_list), rt_index_(rt_index), rt_obj_instances_buf_(rt_obj_instances_buf),
          acc_struct_data_(acc_struct_data), rt_tlas_buf_(rt_tlas_buf),
          rt_tlas_build_scratch_buf_(rt_tlas_scratch_buf) {}

    void Execute(RpBuilder &builder) override;
};