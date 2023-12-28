#pragma once

#include <Ren/Pipeline.h>

#include "../graph/SubPass.h"

namespace Eng {
struct ViewState;
class RpDepthHierarchy : public RpExecutor {
    bool initialized = false;

    // lazily initialized data
    Ren::Pipeline pi_depth_hierarchy_;

    // temp data (valid only between Setup and Execute calls)
    const ViewState *view_state_ = nullptr;

    RpResRef depth_tex_;
    RpResRef atomic_buf_;
    RpResRef output_tex_;

    void LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh);

  public:
    void Setup(RpBuilder &builder, const ViewState *view_state, const RpResRef depth_tex, const RpResRef atomic_counter,
               const RpResRef output_tex) {
        view_state_ = view_state;

        depth_tex_ = depth_tex;
        atomic_buf_ = atomic_counter;
        output_tex_ = output_tex;
    }

    static const int MipCount = 7;
    // TODO: check if it is actually makes sense to use padding
    static const int TileSize = 1 << (MipCount - 1);

    void Execute(RpBuilder &builder) override;
};
} // namespace Eng