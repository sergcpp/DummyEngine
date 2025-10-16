#pragma once

#include <Ren/Pipeline.h>

#include "../framegraph/FgNode.h"

namespace Eng {
struct view_state_t;
class ExDepthHierarchy final : public FgExecutor {
    Ren::PipelineRef pi_depth_hierarchy_;

    const view_state_t *view_state_ = nullptr;

    FgResRef depth_tex_;
    FgResRef atomic_buf_;
    FgResRef output_tex_;

  public:
    ExDepthHierarchy(FgContext &fg, const view_state_t *view_state, FgResRef depth_tex, FgResRef atomic_counter,
                     FgResRef output_tex);

    static const int MipCount = 7;
    // TODO: check if it is actually makes sense to use padding
    static const int TileSize = 1 << (MipCount - 1);

    void Execute(FgContext &fg) override;
};
} // namespace Eng