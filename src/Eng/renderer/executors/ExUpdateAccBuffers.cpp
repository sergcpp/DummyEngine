#include "ExUpdateAccBuffers.h"

#include <Ren/Context.h>

void Eng::ExUpdateAccBuffers::Execute(FgContext &fg) {
    if (fg.ren_ctx().capabilities.hwrt) {
#if !defined(REN_GL_BACKEND)
        Execute_HWRT(fg);
#endif
    } else {
        Execute_SWRT(fg);
    }
}

void Eng::ExUpdateAccBuffers::Execute_SWRT(FgContext &fg) {
    Ren::Buffer &rt_geo_instances_buf = fg.AccessRWBuffer(rt_geo_instances_buf_);

    const auto &rt_geo_instances = p_list_->rt_geo_instances[rt_index_];
    auto &rt_geo_instances_stage_buf = p_list_->rt_geo_instances_stage_buf[rt_index_];

    if (rt_geo_instances.count) {
        const uint32_t rt_geo_instances_mem_size = rt_geo_instances.count * sizeof(rt_geo_instance_t);

        uint8_t *stage_mem = rt_geo_instances_stage_buf->MapRange(fg.backend_frame() * RTGeoInstancesBufChunkSize,
                                                                  RTGeoInstancesBufChunkSize);
        memcpy(stage_mem, rt_geo_instances.data, rt_geo_instances_mem_size);
        rt_geo_instances_stage_buf->Unmap();

        CopyBufferToBuffer(*rt_geo_instances_stage_buf, fg.backend_frame() * RTGeoInstancesBufChunkSize,
                           rt_geo_instances_buf, 0, rt_geo_instances_mem_size, fg.cmd_buf());
    }
}