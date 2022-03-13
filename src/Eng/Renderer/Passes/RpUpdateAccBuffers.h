#pragma once

#include "../Graph/GraphBuilder.h"
#include "../Renderer_DrawList.h"

class RpUpdateAccBuffers : public RenderPassBase {
    // temp data (valid only between Setup and Execute calls)
    DynArrayConstRef<RTObjInstance> rt_obj_instances_;
    Ren::WeakBufferRef rt_obj_instances_stage_buf_;

    RpResource rt_obj_instances_buf_;

  public:
    void Setup(RpBuilder &builder, const DrawList &list, const char rt_obj_instances_buf[]);
    void Execute(RpBuilder &builder) override;

    const char *name() const override { return "UPDATE ACC BUFS"; }
};