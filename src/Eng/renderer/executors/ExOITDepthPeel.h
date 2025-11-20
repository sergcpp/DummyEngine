#pragma once

#include "../Renderer_DrawList.h"
#include "../framegraph/FgNode.h"

#include <Ren/VertexInput.h>

namespace Eng {
class PrimDraw;

class ExOITDepthPeel final : public FgExecutor {
    Ren::PipelineRef pi_simple_[3];

    // lazily initialized data
    Ren::Framebuffer main_draw_fb_[Ren::MaxFramesInFlight][2];
    int fb_to_use_ = 0;

    // temp data (valid only between Setup and Execute calls)
    const view_state_t *view_state_ = nullptr;
    const BindlessTextureData *bindless_tex_ = nullptr;

    const DrawList **p_list_ = nullptr;

    FgResRef vtx_buf1_;
    FgResRef vtx_buf2_;
    FgResRef ndx_buf_;
    FgResRef instances_buf_;
    FgResRef instance_indices_buf_;
    FgResRef shared_data_buf_;
    FgResRef materials_buf_;
    FgResRef dummy_white_;

    FgResRef depth_tex_;
    FgResRef out_depth_buf_;

    void LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh, const Ren::WeakBufRef &vtx_buf1,
                  const Ren::WeakBufRef &vtx_buf2, const Ren::WeakBufRef &ndx_buf, Ren::WeakImgRef depth_tex);
    void DrawTransparent(FgContext &fg);

  public:
    ExOITDepthPeel(const DrawList **p_list, const view_state_t *view_state, const FgResRef vtx_buf1,
                   const FgResRef vtx_buf2, const FgResRef ndx_buf, const FgResRef materials_buf,
                   const BindlessTextureData *bindless_tex, const FgResRef dummy_white, const FgResRef instances_buf,
                   const FgResRef instance_indices_buf, const FgResRef shared_data_buf, const FgResRef depth_tex,
                   const FgResRef out_depth_buf);

    void Execute(FgContext &fg) override;
};
} // namespace Eng