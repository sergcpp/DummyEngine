#pragma once

#include <Ren/Span.h>

#include "../Graph/SubPass.h"
#include "../Renderer_DrawList.h"

namespace Eng {
class RpUpdateAccBuffersExecutor : public RpExecutor {
    const DrawList *&p_list_;
    int rt_index_;

    RpResRef rt_obj_instances_buf_;

    void Execute_HWRT(RpBuilder &builder);
    void Execute_SWRT(RpBuilder &builder);

  public:
    RpUpdateAccBuffersExecutor(const DrawList *&p_list, int rt_index, const RpResRef rt_obj_instances_buf)
        : p_list_(p_list), rt_index_(rt_index), rt_obj_instances_buf_(rt_obj_instances_buf) {}

    void Execute(RpBuilder &builder) override;
};
} // namespace Eng