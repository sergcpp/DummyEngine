#pragma once
#pragma once

#include "../Renderer_DrawList.h"
#include "../framegraph/FgNode.h"

#include <Ren/Pipeline.h>
#include <Ren/RastState.h>
#include <Ren/RenderPass.h>
#include <Ren/VertexInput.h>

namespace Eng {
class PrimDraw;
class ExSkydomeCube final : public FgExecutor {
  public:
    struct Args {
        FgResRef shared_data;
        FgResRef transmittance_lut;
        FgResRef multiscatter_lut;
        FgResRef moon_tex;
        FgResRef weather_tex;
        FgResRef cirrus_tex;
        FgResRef curl_tex;
        FgResRef noise3d_tex;

        FgResRef color_tex;
    };

    ExSkydomeCube(PrimDraw &prim_draw, const view_state_t *view_state, const Args *args)
        : prim_draw_(prim_draw), view_state_(view_state), args_(args) {}

    void Execute(FgBuilder &builder) override;

  private:
    PrimDraw &prim_draw_;
    bool initialized_ = false;
    uint32_t generation_ = 0xffffffff, generation_in_progress_ = 0xffffffff;
    int last_updated_faceq_ = 23;

    // temp data (valid only between Setup and Execute calls)
    const view_state_t *view_state_ = nullptr;
    const Args *args_ = nullptr;

    // lazily initialized data
    Ren::ProgramRef prog_skydome_phys_;
    Ren::PipelineRef pi_skydome_downsample_;

    void LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh);
};

class ExSkydomeScreen final : public FgExecutor {
  public:
    struct Args {
        eSkyQuality sky_quality = eSkyQuality::Medium;

        FgResRef shared_data;
        FgResRef env_tex;
        struct {
            FgResRef transmittance_lut;
            FgResRef multiscatter_lut;
            FgResRef moon_tex;
            FgResRef weather_tex;
            FgResRef cirrus_tex;
            FgResRef curl_tex;
            FgResRef noise3d_tex;
        } phys;

        FgResRef color_tex;
        FgResRef depth_tex;
    };

    ExSkydomeScreen(PrimDraw &prim_draw, const view_state_t *view_state, const Args *args)
        : prim_draw_(prim_draw), view_state_(view_state), args_(args) {}

    void Execute(FgBuilder &builder) override;

    static Ren::Vec2i sample_pos(int frame_index);

  private:
    PrimDraw &prim_draw_;
    bool initialized = false;

    // temp data (valid only between Setup and Execute calls)
    const view_state_t *view_state_ = nullptr;
    const Args *args_ = nullptr;

    // lazily initialized data
    Ren::ProgramRef prog_skydome_simple_, prog_skydome_phys_[2];

    void LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh);
};
} // namespace Eng