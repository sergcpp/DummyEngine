#pragma once

#include "../Renderer_DrawList.h"
#include "../graph/SubPass.h"

namespace Eng {
class PrimDraw;

struct RpDebugProbesData {
    RpResRef shared_data;
    RpResRef offset_tex;
    RpResRef irradiance_tex;
    RpResRef distance_tex;
    RpResRef exposure_tex;
    RpResRef depth_tex;
    RpResRef output_tex;

    int volume_to_debug = 0;
    Ren::Span<const ProbeVolume> probe_volumes;
};

class RpDebugProbes : public RpExecutor {
    PrimDraw &prim_draw_;
    bool initialized = false;

    // temp data (valid only between Setup and Execute calls)
    const ViewState *view_state_ = nullptr;
    const RpDebugProbesData *pass_data_ = nullptr;

    void LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh);

    // lazily initialized data
    Ren::ProgramRef prog_probe_debug_;

  public:
    RpDebugProbes(PrimDraw &prim_draw) : prim_draw_(prim_draw) {}

    void Setup(RpBuilder &builder, const DrawList &list, const ViewState *view_state,
               const RpDebugProbesData *pass_data) {
        view_state_ = view_state;
        pass_data_ = pass_data;
    }
    void Execute(RpBuilder &builder) override;
};
} // namespace Eng