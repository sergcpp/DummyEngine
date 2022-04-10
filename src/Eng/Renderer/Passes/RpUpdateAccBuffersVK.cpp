#include "RpUpdateAccBuffers.h"

void RpUpdateAccBuffersExecutor::Execute(RpBuilder &builder) {
    RpAllocBuf &rt_obj_instances_buf = builder.GetWriteBuffer(rt_obj_instances_buf_);

    Ren::Context &ctx = builder.ctx();

    if (p_list_->rt_obj_instances.count) {
        uint8_t *stage_mem = p_list_->rt_obj_instances_stage_buf->MapRange(
            Ren::BufMapWrite, ctx.backend_frame() * RTObjInstancesBufChunkSize, RTObjInstancesBufChunkSize);
        const uint32_t rt_obj_instances_mem_size =
            p_list_->rt_obj_instances.count * sizeof(VkAccelerationStructureInstanceKHR);
        if (stage_mem) {
            auto *out_instances = reinterpret_cast<VkAccelerationStructureInstanceKHR *>(stage_mem);
            for (uint32_t i = 0; i < p_list_->rt_obj_instances.count; ++i) {
                auto &new_instance = out_instances[i];
                memcpy(new_instance.transform.matrix, p_list_->rt_obj_instances.data[i].xform, 12 * sizeof(float));
                new_instance.instanceCustomIndex = p_list_->rt_obj_instances.data[i].custom_index;
                new_instance.mask = p_list_->rt_obj_instances.data[i].mask;
                new_instance.flags = 0;
                new_instance.accelerationStructureReference =
                    reinterpret_cast<const Ren::AccStructureVK *>(p_list_->rt_obj_instances.data[i].blas_ref)
                        ->vk_device_address();
            }

            p_list_->rt_obj_instances_stage_buf->FlushMappedRange(
                0, p_list_->rt_obj_instances_stage_buf->AlignMapOffset(rt_obj_instances_mem_size));
            p_list_->rt_obj_instances_stage_buf->Unmap();
        } else {
            builder.log()->Error("RpUpdateAccStructures: Failed to map rt obj instance buffer!");
        }

        Ren::CopyBufferToBuffer(*p_list_->rt_obj_instances_stage_buf, ctx.backend_frame() * RTObjInstancesBufChunkSize,
                                *rt_obj_instances_buf.ref, 0, rt_obj_instances_mem_size, ctx.current_cmd_buf());
    }
}