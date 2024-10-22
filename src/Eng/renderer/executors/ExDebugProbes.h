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
        Ren::Span<const ProbeVolume> probe_volumes;
    };

    ExDebugProbes(PrimDraw &prim_draw) : prim_draw_(prim_draw) {}

    void Setup(FgBuilder &builder, const DrawList &list, const ViewState *view_state, const Args *args) {
        view_state_ = view_state;
        args_ = args;
    }
    void Execute(FgBuilder &builder) override;

  private:
    PrimDraw &prim_draw_;
    bool initialized = false;

    // temp data (valid only between Setup and Execute calls)
    const ViewState *view_state_ = nullptr;
    const Args *args_ = nullptr;

    void LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh);

    // lazily initialized data
    Ren::ProgramRef prog_probe_debug_;
};
} // namespace Eng