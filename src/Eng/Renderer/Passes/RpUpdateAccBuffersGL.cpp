#include "RpUpdateAccBuffers.h"

void RpUpdateAccBuffers::Execute(RpBuilder &builder) {
    RpAllocBuf &rt_obj_instances_buf = builder.GetWriteBuffer(rt_obj_instances_buf_);

    Ren::Context &ctx = builder.ctx();
    Ren::ApiContext *api_ctx = ctx.api_ctx();

    /*if (rt_obj_instances_.count) {
        uint8_t *stage_mem = rt_obj_instances_stage_buf_->MapRange(
            Ren::BufMapWrite, ctx.backend_frame() * RTObjInstancesBufChunkSize, RTObjInstancesBufChunkSize);
        const uint32_t rt_obj_instances_mem_size = rt_obj_instances_.count * sizeof(VkAccelerationStructureInstanceKHR);
        if (stage_mem) {
            auto *out_instances = reinterpret_cast<VkAccelerationStructureInstanceKHR *>(stage_mem);
            for (uint32_t i = 0; i < rt_obj_instances_.count; ++i) {
                auto &new_instance = out_instances[i];
                memcpy(new_instance.transform.matrix, rt_obj_instances_.data[i].xform, 12 * sizeof(float));
                new_instance.instanceCustomIndex = rt_obj_instances_.data[i].custom_index;
                new_instance.mask = rt_obj_instances_.data[i].mask;
                new_instance.flags = 0;
                new_instance.accelerationStructureReference =
                    reinterpret_cast<const Ren::AccStructureVK *>(rt_obj_instances_.data[i].blas_ref)
                        ->vk_device_address();
            }

            memcpy(stage_mem, rt_obj_instances_.data, rt_obj_instances_mem_size);
            rt_obj_instances_stage_buf_->FlushMappedRange(
                0, rt_obj_instances_stage_buf_->AlignMapOffset(rt_obj_instances_mem_size));
            rt_obj_instances_stage_buf_->Unmap();
        } else {
            builder.log()->Error("RpUpdateAccStructures: Failed to map rt obj instance buffer!");
        }

        Ren::CopyBufferToBuffer(*rt_obj_instances_stage_buf_, api_ctx->backend_frame * RTObjInstancesBufChunkSize,
                                *rt_obj_instances_buf.ref, 0, rt_obj_instances_mem_size, cmd_buf);
    }*/
}