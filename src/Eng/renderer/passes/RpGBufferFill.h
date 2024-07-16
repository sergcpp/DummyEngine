#pragma once

#include "../Renderer_DrawList.h"
#include "../graph/SubPass.h"

#include <Ren/VertexInput.h>

namespace Eng {
class RpGBufferFill : public RpExecutor {
    bool initialized = false;

    // lazily initialized data
    Ren::RenderPass rp_main_draw_;

    Ren::VertexInput vi_simple_, vi_vegetation_;

    Ren::Pipeline pi_simple_[3];
    Ren::Pipeline pi_vegetation_[2];

    Ren::Framebuffer main_draw_fb_[Ren::MaxFramesInFlight][2];
    int fb_to_use_ = 0;

    // temp data (valid only between Setup and Execute calls)
    const ViewState *view_state_ = nullptr;
    const BindlessTextureData *bindless_tex_ = nullptr;

    const Ren::TextureAtlas *decals_atlas_ = nullptr;

    const DrawList **p_list_ = nullptr;

    RpResRef vtx_buf1_;
    RpResRef vtx_buf2_;
    RpResRef ndx_buf_;
    RpResRef instances_buf_;
    RpResRef instance_indices_buf_;
    RpResRef shared_data_buf_;
    RpResRef materials_buf_;
    RpResRef textures_buf_;
    RpResRef cells_buf_;
    RpResRef items_buf_;
    RpResRef lights_buf_;
    RpResRef decals_buf_;
    RpResRef noise_tex_;
    RpResRef dummy_white_;
    RpResRef dummy_black_;

    RpResRef out_albedo_tex_;
    RpResRef out_normal_tex_;
    RpResRef out_spec_tex_;
    RpResRef out_depth_tex_;

    void LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh, RpAllocBuf &vtx_buf1, RpAllocBuf &vtx_buf2,
                  RpAllocBuf &ndx_buf, RpAllocTex &albedo_tex, RpAllocTex &normal_tex, RpAllocTex &spec_tex,
                  RpAllocTex &depth_tex);
    void DrawOpaque(RpBuilder &builder);

  public:
    void Setup(const DrawList **p_list, const ViewState *view_state, const RpResRef vtx_buf1, const RpResRef vtx_buf2,
               const RpResRef ndx_buf, const RpResRef materials_buf, const RpResRef textures_buf,
               const BindlessTextureData *bindless_tex, const RpResRef noise_tex, const RpResRef dummy_white,
               const RpResRef dummy_black, const RpResRef instances_buf, const RpResRef instance_indices_buf,
               const RpResRef shared_data_buf, const RpResRef cells_buf, const RpResRef items_buf,
               const RpResRef decals_buf, const RpResRef out_albedo, const RpResRef out_normals,
               const RpResRef out_spec, const RpResRef out_depth) {
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

    void Execute(RpBuilder &builder) override;
};
} // namespace Eng