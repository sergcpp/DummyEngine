#pragma once

#include <Ren/Span.h>

#include "../Renderer_DrawList.h"
#include "../framegraph/FgNode.h"

namespace Eng {
class ExUpdateAccBuffers final : public FgExecutor {
    const DrawList *&p_list_;
    int rt_index_;

    FgResRef rt_geo_instances_buf_;
    FgResRef rt_obj_instances_buf_;

    void Execute_HWRT(FgContext &fg);
    void Execute_SWRT(FgContext &fg);

  public:
    ExUpdateAccBuffers(const DrawList *&p_list, int rt_index, const FgResRef rt_geo_instances_buf,
                       const FgResRef rt_obj_instances_buf)
        : p_list_(p_list), rt_index_(rt_index), rt_geo_instances_buf_(rt_geo_instances_buf),
          rt_obj_instances_buf_(rt_obj_instances_buf) {}

    void Execute(FgContext &fg) override;
};
} // namespace Eng