#pragma once

#include <Ren/Image.h>
#include <Ren/RenderPass.h>

#include "../framegraph/FgNode.h"

namespace Eng {
class PrimDraw;
struct view_state_t;

class ExPostprocess final : public FgExecutor {
  public:
    struct Args {
        FgResRef exposure_tex;
        FgResRef color_tex;
        FgResRef bloom_tex;
        FgResRef lut_tex;
        FgResRef output_tex;
        FgResRef output_tex2;

        Ren::SamplerRef linear_sampler;

        int tonemap_mode = 1;
        float inv_gamma = 1.0f, fade = 0.0f;
        float aberration = 1.0f;
        float purkinje = 1.0f;
    };

    ExPostprocess(PrimDraw &prim_draw, ShaderLoader &sh, const view_state_t *view_state, const Args *args);

    void Execute(FgContext &fg) override;

  private:
    PrimDraw &prim_draw_;

    Ren::ProgramRef blit_postprocess_prog_[2][2];

    // temp data (valid only between Setup and Execute calls)
    const view_state_t *view_state_ = nullptr;
    const Args *args_ = nullptr;
};
} // namespace Eng