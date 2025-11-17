#include "ExUpdateAccBuffers.h"

#include <Ren/Context.h>

void Eng::ExUpdateAccBuffers::Execute_HWRT(FgContext &fg) {
    Ren::Buffer &rt_geo_instances_buf = fg.AccessRWBuffer(rt_geo_instances_buf_);
    Ren::Buffer &rt_obj_instances_buf = fg.AccessRWBuffer(rt_obj_instances_buf_);

    const auto &rt_geo_instances = p_list_->rt_geo_instances[rt_index_];
    auto &rt_geo_instances_stage_buf = p_list_->rt_geo_instances_stage_buf[rt_index_];

    if (rt_geo_instances.count) {
        const uint32_t rt_geo_instances_mem_size = rt_geo_instances.count * sizeof(rt_geo_instance_t);
        assert(rt_geo_instances_mem_size < RTGeoInstancesBufChunkSize);

        uint8_t *stage_mem = rt_geo_instances_stage_buf->MapRange(fg.backend_frame() * RTGeoInstancesBufChunkSize,
                                                                  RTGeoInstancesBufChunkSize);
        memcpy(stage_mem, rt_geo_instances.data, rt_geo_instances_mem_size);
        rt_geo_instances_stage_buf->Unmap();

        CopyBufferToBuffer(*rt_geo_instances_stage_buf, fg.backend_frame() * RTGeoInstancesBufChunkSize,
                           rt_geo_instances_buf, 0, rt_geo_instances_mem_size, fg.cmd_buf());
    }

    const auto &rt_obj_instances = p_list_->rt_obj_instances[rt_index_];
    auto &rt_obj_instances_stage_buf = p_list_->rt_obj_instances_stage_buf[rt_index_];

    if (rt_obj_instances.count) {
        const uint32_t rt_obj_instances_mem_size = rt_obj_instances.count * sizeof(VkAccelerationStructureInstanceKHR);
        assert(rt_obj_instances_mem_size <= HWRTObjInstancesBufChunkSize);

        uint8_t *stage_mem = rt_obj_instances_stage_buf->MapRange(fg.backend_frame() * HWRTObjInstancesBufChunkSize,
                                                                  HWRTObjInstancesBufChunkSize);
        if (stage_mem) {
            auto *out_instances = reinterpret_cast<VkAccelerationStructureInstanceKHR *>(stage_mem);
            for (uint32_t i = 0; i < rt_obj_instances.count; ++i) {
                auto &new_instance = out_instances[i];
                memcpy(new_instance.transform.matrix, rt_obj_instances.data[i].xform, 12 * sizeof(float));
                new_instance.instanceCustomIndex = rt_obj_instances.data[i].geo_index;
                new_instance.mask = rt_obj_instances.data[i].mask;
                new_instance.flags = 0;
                new_instance.accelerationStructureReference =
                    reinterpret_cast<const Ren::AccStructureVK *>(rt_obj_instances.data[i].blas_ref)
                        ->vk_device_address();
            }

            rt_obj_instances_stage_buf->Unmap();
        } else {
            fg.log()->Error("ExUpdateAccBuffers: Failed to map rt obj instance buffer!");
        }

        CopyBufferToBuffer(*rt_obj_instances_stage_buf, fg.backend_frame() * HWRTObjInstancesBufChunkSize,
                           rt_obj_instances_buf, 0, rt_obj_instances_mem_size, fg.cmd_buf());
    }
}