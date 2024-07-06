#pragma once
#pragma once

#include "../Renderer_DrawList.h"
#include "../graph/SubPass.h"

#include <Ren/Pipeline.h>
#include <Ren/RastState.h>
#include <Ren/RenderPass.h>
#include <Ren/VertexInput.h>

namespace Eng {
class PrimDraw;

struct RpSkydomeCubeData {
    RpResRef shared_data;
    RpResRef transmittance_lut;
    RpResRef multiscatter_lut;
    RpResRef moon_tex;
    RpResRef weather_tex;
    RpResRef cirrus_tex;
    RpResRef noise3d_tex;

    RpResRef color_tex;
};

class RpSkydomeCube : public RpExecutor {
    PrimDraw &prim_draw_;
    bool initialized = false;

    // temp data (valid only between Setup and Execute calls)
    const ViewState *view_state_ = nullptr;
    const RpSkydomeCubeData *pass_data_ = nullptr;

    // lazily initialized data
    Ren::ProgramRef prog_skydome_phys_;
    Ren::Pipeline pi_skydome_downsample_;

    void LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh);

  public:
    RpSkydomeCube(PrimDraw &prim_draw) : prim_draw_(prim_draw) {}

    void Setup(const ViewState *view_state, const RpSkydomeCubeData *pass_data) {
        view_state_ = view_state;
        pass_data_ = pass_data;
    }

    void Execute(RpBuilder &builder) override;
};

struct RpSkydomeScreenData {
    eSkyQuality sky_quality = eSkyQuality::Low;

    RpResRef shared_data;
    RpResRef env_tex;
    struct {
        RpResRef transmittance_lut;
        RpResRef multiscatter_lut;
        RpResRef moon_tex;
        RpResRef weather_tex;
        RpResRef cirrus_tex;
        RpResRef noise3d_tex;
    } phys;

    RpResRef color_tex;
    RpResRef depth_tex;
};

class RpSkydomeScreen : public RpExecutor {
    PrimDraw &prim_draw_;
    bool initialized = false;

    // temp data (valid only between Setup and Execute calls)
    const ViewState *view_state_ = nullptr;
    const RpSkydomeScreenData *pass_data_ = nullptr;

    // lazily initialized data
    Ren::ProgramRef prog_skydome_simple_, prog_skydome_phys_;

    void LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh);

  public:
    RpSkydomeScreen(PrimDraw &prim_draw) : prim_draw_(prim_draw) {}

    void Setup(const ViewState *view_state, const RpSkydomeScreenData *pass_data) {
        view_state_ = view_state;
        pass_data_ = pass_data;
    }

    void Execute(RpBuilder &builder) override;
};
} // namespace Eng