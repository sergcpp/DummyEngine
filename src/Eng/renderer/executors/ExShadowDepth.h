#pragma once

#include "../Renderer_DrawList.h"
#include "../framegraph/FgNode.h"

#include <Ren/VertexInput.h>

namespace Eng {
class ExShadowDepth final : public FgExecutor {
    bool initialized = false;
    int w_, h_;

    // lazily initialized data
    Ren::PipelineRef pi_solid_[3], pi_alpha_[3];
    Ren::PipelineRef pi_vege_solid_, pi_vege_alpha_;

    Ren::Framebuffer shadow_fb_;

    // temp data (valid only between Setup and Execute calls)
    const DrawList **p_list_ = nullptr;
    const BindlessTextureData *bindless_tex_ = nullptr;

    // inputs
    FgResRef vtx_buf1_;
    FgResRef vtx_buf2_;
    FgResRef ndx_buf_;
    FgResRef instances_buf_;
    FgResRef instance_indices_buf_;
    FgResRef shared_data_buf_;
    FgResRef materials_buf_;
    FgResRef noise_tex_;

    // outputs
    FgResRef shadow_depth_tex_;

    void LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh, FgAllocBuf &vtx_buf1, FgAllocBuf &vtx_buf2,
                  FgAllocBuf &ndx_buf, FgAllocTex &shadow_depth_tex);
    void DrawShadowMaps(FgContext &fg);

  public:
    ExShadowDepth(const int w, const int h, const DrawList **p_list, const FgResRef vtx_buf1, const FgResRef vtx_buf2,
                  const FgResRef ndx_buf, const FgResRef materials_buf, const BindlessTextureData *bindless_tex,
                  const FgResRef instances_buf, const FgResRef instance_indices_buf, const FgResRef shared_data_buf,
                  const FgResRef noise_tex, const FgResRef shadow_depth_tex)
        : w_(w), h_(h) {
        p_list_ = p_list;
        bindless_tex_ = bindless_tex;

        vtx_buf1_ = vtx_buf1;
        vtx_buf2_ = vtx_buf2;
        ndx_buf_ = ndx_buf;

        instances_buf_ = instances_buf;
        instance_indices_buf_ = instance_indices_buf;
        shared_data_buf_ = shared_data_buf;
        materials_buf_ = materials_buf;
        noise_tex_ = noise_tex;

        shadow_depth_tex_ = shadow_depth_tex;
    }

    void Execute(FgContext &fg) override;
};
} // namespace Eng