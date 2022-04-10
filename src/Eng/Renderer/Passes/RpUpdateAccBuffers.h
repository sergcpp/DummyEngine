#pragma once

#include "../Graph/GraphBuilder.h"
#include "../Renderer_DrawList.h"

class RpUpdateAccBuffersExecutor : public RenderPassExecutor {
    DynArrayConstRef<RTObjInstance> rt_obj_instances_;
    Ren::WeakBufferRef rt_obj_instances_stage_buf_;

    RpResRef rt_obj_instances_buf_;

  public:
    RpUpdateAccBuffersExecutor(const DynArray<RTObjInstance> &rt_obj_instances,
                               const Ren::BufferRef &rt_obj_instances_stage_buf, const RpResRef rt_obj_instances_buf)
        : rt_obj_instances_(rt_obj_instances), rt_obj_instances_stage_buf_(rt_obj_instances_stage_buf),
          rt_obj_instances_buf_(rt_obj_instances_buf) {}

    void Execute(RpBuilder &builder) override;
};