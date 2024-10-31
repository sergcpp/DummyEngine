#pragma once

#include <Ren/RenderPass.h>
#include <Ren/Texture.h>

#include "../framegraph/FgNode.h"

namespace Eng {
class PrimDraw;
struct ViewState;

class ExPostprocess final : public FgExecutor {
  public:
    struct Args {
        FgResRef exposure_tex;
        FgResRef color_tex;
        FgResRef bloom_tex;
        FgResRef output_tex;
        FgResRef output_tex2;

        Ren::WeakTex3DRef lut_tex;

        int tonemap_mode = 1;
        float inv_gamma = 1.0f, fade = 0.0f;
        float aberration = 1.0f;
        float purkinje = 1.0f;
    };

    explicit ExPostprocess(PrimDraw &prim_draw) : prim_draw_(prim_draw) {}

    void Setup(const ViewState *view_state, const Args *args) {
        view_state_ = view_state;
        args_ = args;
    }
    void Execute(FgBuilder &builder) override;

  private:
    PrimDraw &prim_draw_;
    bool initialized = false;

    // lazily initialized data
    Ren::ProgramRef blit_postprocess_prog_[2][2];

    // temp data (valid only between Setup and Execute calls)
    const ViewState *view_state_ = nullptr;
    const Args *args_ = nullptr;

    void LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh);
};
} // namespace Eng