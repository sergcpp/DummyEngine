#pragma once

#include "../Renderer_DrawList.h"
#include "../framegraph/FgNode.h"

#include <Ren/VertexInput.h>

namespace Eng {
class PrimDraw;

class ExOITScheduleRays final : public FgExecutor {
    bool initialized = false;

    // lazily initialized data
    Ren::PipelineRef pi_simple_[3];
    Ren::PipelineRef pi_vegetation_[2];
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
    FgResRef noise_tex_;
    FgResRef dummy_white_;
    FgResRef oit_depth_buf_;
    FgResRef ray_counter_;
    FgResRef ray_list_;

    FgResRef depth_tex_;

    void LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh, FgAllocBuf &vtx_buf1, FgAllocBuf &vtx_buf2,
                  FgAllocBuf &ndx_buf, FgAllocTex &depth_tex);
    void DrawTransparent(FgContext &fg, FgAllocTex &depth_tex);

  public:
    ExOITScheduleRays(const DrawList **p_list, const view_state_t *view_state, const FgResRef vtx_buf1,
                      const FgResRef vtx_buf2, const FgResRef ndx_buf, const FgResRef materials_buf,
                      const BindlessTextureData *bindless_tex, const FgResRef noise_tex, const FgResRef dummy_white,
                      const FgResRef instances_buf, const FgResRef instance_indices_buf, const FgResRef shared_data_buf,
                      const FgResRef depth_tex, const FgResRef oit_depth_buf, const FgResRef ray_counter,
                      const FgResRef ray_list);

    void Execute(FgContext &fg) override;
};
} // namespace Eng