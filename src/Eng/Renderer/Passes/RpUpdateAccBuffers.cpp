#include "RpUpdateAccBuffers.h"

void RpUpdateAccBuffers::Setup(RpBuilder &builder, const DrawList &list, const char rt_obj_instances_buf[]) {
    rt_obj_instances_ = list.rt_obj_instances;
    rt_obj_instances_stage_buf_ = list.rt_obj_instances_stage_buf;

    { // create obj instances buffer
        RpBufDesc desc;
        desc.type = Ren::eBufType::Storage;
        desc.size = RTObjInstancesBufChunkSize;
        rt_obj_instances_buf_ =
            builder.WriteBuffer(rt_obj_instances_buf, desc, Ren::eResState::CopyDst, Ren::eStageBits::Transfer, *this);
    }
}