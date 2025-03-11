#pragma once

#include "../Renderer_DrawList.h"
#include "../framegraph/FgNode.h"

#include <Ren/VertexInput.h>

namespace Eng {
class ExEmissive final : public FgExecutor {
    bool initialized = false;

    // lazily initialized data
    Ren::PipelineRef pi_simple_[3];
    Ren::PipelineRef pi_vegetation_[2];

    Ren::Framebuffer main_draw_fb_[Ren::MaxFramesInFlight][2];
    int fb_to_use_ = 0;

    // temp data (valid only between Setup and Execute calls)
    const view_state_t *view_state_ = nullptr;
    const BindlessTextureData *bindless_tex_ = nullptr;

    const Ren::TextureAtlas *decals_atlas_ = nullptr;

    const DrawList **p_list_ = nullptr;

    FgResRef vtx_buf1_;
    FgResRef vtx_buf2_;
    FgResRef ndx_buf_;
    FgResRef instances_buf_;
    FgResRef instance_indices_buf_;
    FgResRef shared_data_buf_;
    FgResRef materials_buf_;
    FgResRef textures_buf_;
    FgResRef noise_tex_;
    FgResRef dummy_white_;

    FgResRef out_color_tex_;
    FgResRef out_depth_tex_;

    void LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh, FgAllocBuf &vtx_buf1, FgAllocBuf &vtx_buf2,
                  FgAllocBuf &ndx_buf, FgAllocTex &color_tex, FgAllocTex &depth_tex);
    void DrawOpaque(FgBuilder &builder);

  public:
    void Setup(const DrawList **p_list, const view_state_t *view_state, const FgResRef vtx_buf1, const FgResRef vtx_buf2,
               const FgResRef ndx_buf, const FgResRef materials_buf, const FgResRef textures_buf,
               const BindlessTextureData *bindless_tex, const FgResRef noise_tex, const FgResRef dummy_white,
               const FgResRef instances_buf, const FgResRef instance_indices_buf,
               const FgResRef shared_data_buf, const FgResRef out_color, const FgResRef out_depth) {
        view_state_ = view_state;
        bindless_tex_ = bindless_tex;

        p_list_ = p_list;

        vtx_buf1_ = vtx_buf1;
        vtx_buf2_ = vtx_buf2;
        ndx_buf_ = ndx_buf;
        instances_buf_ = instances_buf;
        instance_indices_buf_ = instance_indices_buf;
        shared_data_buf_ = shared_data_buf;
        materials_buf_ = materials_buf;

        noise_tex_ = noise_tex;
        dummy_white_ = dummy_white;

        textures_buf_ = textures_buf;

        out_color_tex_ = out_color;
        out_depth_tex_ = out_depth;
    }

    void Execute(FgBuilder &builder) override;
};
} // namespace Eng