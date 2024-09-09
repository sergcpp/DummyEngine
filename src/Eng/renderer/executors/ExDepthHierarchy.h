#pragma once

#include <Ren/Pipeline.h>

#include "../framegraph/FgNode.h"

namespace Eng {
struct ViewState;
class ExDepthHierarchy final : public FgExecutor {
    bool initialized = false;

    // lazily initialized data
    Ren::Pipeline pi_depth_hierarchy_;

    // temp data (valid only between Setup and Execute calls)
    const ViewState *view_state_ = nullptr;

    FgResRef depth_tex_;
    FgResRef atomic_buf_;
    FgResRef output_tex_;

    void LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh);

  public:
    void Setup(FgBuilder &builder, const ViewState *view_state, const FgResRef depth_tex, const FgResRef atomic_counter,
               const FgResRef output_tex) {
        view_state_ = view_state;

        depth_tex_ = depth_tex;
        atomic_buf_ = atomic_counter;
        output_tex_ = output_tex;
    }

    static const int MipCount = 7;
    // TODO: check if it is actually makes sense to use padding
    static const int TileSize = 1 << (MipCount - 1);

    void Execute(FgBuilder &builder) override;
};
} // namespace Eng