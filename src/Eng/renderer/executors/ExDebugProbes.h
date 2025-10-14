#pragma once

#include "../Renderer_DrawList.h"
#include "../framegraph/FgNode.h"

namespace Eng {
class PrimDraw;

class ExDebugProbes final : public FgExecutor {
  public:
    struct Args {
        FgResRef shared_data;
        FgResRef offset_tex;
        FgResRef irradiance_tex;
        FgResRef distance_tex;
        FgResRef depth_tex;
        FgResRef output_tex;

        int volume_to_debug = 0;
        Ren::Span<const probe_volume_t> probe_volumes;
    };

    ExDebugProbes(PrimDraw &prim_draw, FgContext &ctx, const DrawList &list, const view_state_t *view_state,
                  const Args *args);

    void Execute(FgContext &ctx) override;

  private:
    PrimDraw &prim_draw_;

    // temp data (valid only between Setup and Execute calls)
    const view_state_t *view_state_ = nullptr;
    const Args *args_ = nullptr;

    // lazily initialized data
    Ren::ProgramRef prog_probe_debug_;
};
} // namespace Eng