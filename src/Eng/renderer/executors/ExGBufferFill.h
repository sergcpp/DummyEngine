#pragma once

#include "../Renderer_DrawList.h"
#include "../framegraph/FgNode.h"

#include <Ren/VertexInput.h>

namespace Eng {
class ExGBufferFill final : public FgExecutor {
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
    FgResRef cells_buf_;
    FgResRef items_buf_;
    FgResRef lights_buf_;
    FgResRef decals_buf_;
    FgResRef noise_tex_;
    FgResRef dummy_white_;
    FgResRef dummy_black_;

    FgResRef out_albedo_tex_;
    FgResRef out_normal_tex_;
    FgResRef out_spec_tex_;
    FgResRef out_depth_tex_;

    void LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh, FgAllocBuf &vtx_buf1, FgAllocBuf &vtx_buf2,
                  FgAllocBuf &ndx_buf, FgAllocTex &albedo_tex, FgAllocTex &normal_tex, FgAllocTex &spec_tex,
                  FgAllocTex &depth_tex);
    void DrawOpaque(FgContext &ctx);

  public:
    ExGBufferFill(const DrawList **p_list, const view_state_t *view_state, const FgResRef vtx_buf1,
                  const FgResRef vtx_buf2, const FgResRef ndx_buf, const FgResRef materials_buf,
                  const FgResRef textures_buf, const BindlessTextureData *bindless_tex, const FgResRef noise_tex,
                  const FgResRef dummy_white, const FgResRef dummy_black, const FgResRef instances_buf,
                  const FgResRef instance_indices_buf, const FgResRef shared_data_buf, const FgResRef cells_buf,
                  const FgResRef items_buf, const FgResRef decals_buf, const FgResRef out_albedo,
                  const FgResRef out_normals, const FgResRef out_spec, const FgResRef out_depth) {
        view_state_ = view_state;
        bindless_tex_ = bindless_tex;

        p_list_ = p_list;

        vtx_buf1_ = vtx_buf1;
        vtx_buf2_ = vtx_buf2;
        ndx_buf_ = ndx_buf;
        instances_buf_ = instances_buf;
        instance_indices_buf_ = instance_indices_buf;
        shared_data_buf_ = shared_data_buf;
        cells_buf_ = cells_buf;
        items_buf_ = items_buf;
        decals_buf_ = decals_buf;
        materials_buf_ = materials_buf;

        noise_tex_ = noise_tex;
        dummy_white_ = dummy_white;
        dummy_black_ = dummy_black;

        textures_buf_ = textures_buf;

        out_albedo_tex_ = out_albedo;
        out_normal_tex_ = out_normals;
        out_spec_tex_ = out_spec;
        out_depth_tex_ = out_depth;
    }

    void Execute(FgContext &ctx) override;
};
} // namespace Eng