#include "ExUpdateAccBuffers.h"

#include <Ren/Context.h>

void Eng::ExUpdateAccBuffers::Execute(FgBuilder &builder) {
    if (builder.ctx().capabilities.hwrt) {
#if !defined(USE_GL_RENDER)
        Execute_HWRT(builder);
#endif
    } else {
        Execute_SWRT(builder);
    }
}

void Eng::ExUpdateAccBuffers::Execute_SWRT(FgBuilder &builder) {
    FgAllocBuf &rt_geo_instances_buf = builder.GetWriteBuffer(rt_geo_instances_buf_);

    Ren::Context &ctx = builder.ctx();

    const auto &rt_geo_instances = p_list_->rt_geo_instances[rt_index_];
    auto &rt_geo_instances_stage_buf = p_list_->rt_geo_instances_stage_buf[rt_index_];

    if (rt_geo_instances.count) {
        const uint32_t rt_geo_instances_mem_size = rt_geo_instances.count * sizeof(RTGeoInstance);

        uint8_t *stage_mem = rt_geo_instances_stage_buf->MapRange(ctx.backend_frame() * RTGeoInstancesBufChunkSize,
                                                                  RTGeoInstancesBufChunkSize);
        memcpy(stage_mem, rt_geo_instances.data, rt_geo_instances_mem_size);
        rt_geo_instances_stage_buf->Unmap();

        CopyBufferToBuffer(*rt_geo_instances_stage_buf, ctx.backend_frame() * RTGeoInstancesBufChunkSize,
                           *rt_geo_instances_buf.ref, 0, rt_geo_instances_mem_size, ctx.current_cmd_buf());
    }
}