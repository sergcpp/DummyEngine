#include "RpBuildAccStructures.h"

void RpBuildAccStructuresExecutor::Execute(RpBuilder &builder) {
    RpAllocBuf &rt_obj_instances_buf = builder.GetReadBuffer(rt_obj_instances_buf_);
    RpAllocBuf &rt_tlas_buf = builder.GetWriteBuffer(rt_tlas_buf_);
    RpAllocBuf &rt_tlas_build_scratch_buf = builder.GetWriteBuffer(rt_tlas_build_scratch_buf_);
}
