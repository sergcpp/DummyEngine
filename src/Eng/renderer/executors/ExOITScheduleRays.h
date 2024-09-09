#pragma once

#include "../Renderer_DrawList.h"
#include "../framegraph/FgNode.h"

#include <Ren/VertexInput.h>

namespace Eng {
class PrimDraw;

class ExOITScheduleRays final : public FgExecutor {
    bool initialized = false;

    // lazily initialized data
    Ren::RenderPass rp_oit_schedule_rays_;
    Ren::VertexInput vi_simple_, vi_vegetation_;
    Ren::Pipeline pi_simple_[3];
    Ren::Pipeline pi_vegetation_[2];
    Ren::Framebuffer main_draw_fb_[Ren::MaxFramesInFlight][2];
    int fb_to_use_ = 0;

    // temp data (valid only between Setup and Execute calls)
    const ViewState *view_state_ = nullptr;
    const BindlessTextureData *bindless_tex_ = nullptr;

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
    FgResRef oit_depth_buf_;
    FgResRef ray_counter_;
    FgResRef ray_list_;

    FgResRef depth_tex_;

    void LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh, FgAllocBuf &vtx_buf1, FgAllocBuf &vtx_buf2,
                  FgAllocBuf &ndx_buf, FgAllocTex &depth_tex);
    void DrawTransparent(FgBuilder &builder, FgAllocTex &depth_tex);

  public:
    void Setup(const DrawList **p_list, const ViewState *view_state, const FgResRef vtx_buf1, const FgResRef vtx_buf2,
               const FgResRef ndx_buf, const FgResRef materials_buf, const FgResRef textures_buf,
               const BindlessTextureData *bindless_tex, const FgResRef noise_tex, const FgResRef dummy_white,
               const FgResRef instances_buf, const FgResRef instance_indices_buf, const FgResRef shared_data_buf,
               const FgResRef depth_tex, const FgResRef oit_depth_buf, const FgResRef ray_counter,
               const FgResRef ray_list) {
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
        textures_buf_ = textures_buf;
        noise_tex_ = noise_tex;
        dummy_white_ = dummy_white;
        oit_depth_buf_ = oit_depth_buf;
        ray_counter_ = ray_counter;
        ray_list_ = ray_list;

        depth_tex_ = depth_tex;
    }

    void Execute(FgBuilder &builder) override;
};
} // namespace Eng