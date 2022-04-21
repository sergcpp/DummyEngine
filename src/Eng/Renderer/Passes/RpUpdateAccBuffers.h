#pragma once

#include "../Graph/GraphBuilder.h"
#include "../Renderer_DrawList.h"

class RpUpdateAccBuffersExecutor : public RpExecutor {
    const DrawList *&p_list_;

    RpResRef rt_obj_instances_buf_;

  public:
    RpUpdateAccBuffersExecutor(const DrawList *&p_list, const RpResRef rt_obj_instances_buf)
        : p_list_(p_list), rt_obj_instances_buf_(rt_obj_instances_buf) {}

    void Execute(RpBuilder &builder) override;
};