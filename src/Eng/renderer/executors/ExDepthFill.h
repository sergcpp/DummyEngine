#pragma once

#include "../Renderer_DrawList.h"
#include "../framegraph/FgNode.h"

#include <Ren/Pipeline.h>
#include <Ren/VertexInput.h>

namespace Eng {
class ExDepthFill final : public FgExecutor {
    bool initialized = false;

    // lazily initialized data

    // temp data (valid only between Setup and Execute calls)
    const view_state_t *view_state_ = nullptr;
    const BindlessTextureData *bindless_tex_ = nullptr;
    bool clear_depth_ = false;

    const DrawList **p_list_;

    FgResRef vtx_buf1_;
    FgResRef vtx_buf2_;
    FgResRef ndx_buf_;
    FgResRef instances_buf_;
    FgResRef instance_indices_buf_;
    FgResRef shared_data_buf_;
    FgResRef materials_buf_;
    FgResRef textures_buf_;
    FgResRef noise_tex_;

    FgResRef depth_tex_;
    FgResRef velocity_tex_;

    void LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh, FgAllocBuf &vtx_buf1, FgAllocBuf &vtx_buf2,
                  FgAllocBuf &ndx_buf, FgAllocTex &depth_tex, FgAllocTex &velocity_tex);
    void DrawDepth(FgContext &builder, FgAllocBuf &vtx_buf1, FgAllocBuf &vtx_buf2, FgAllocBuf &ndx_buf);

    Ren::RenderPassRef rp_depth_only_[2], rp_depth_velocity_[2];

    Ren::PipelineRef pi_static_solid_[3], pi_static_transp_[3];
    Ren::PipelineRef pi_moving_solid_[3], pi_moving_transp_[3];
    Ren::PipelineRef pi_vege_static_solid_[2], pi_vege_static_transp_[2];
    Ren::PipelineRef pi_vege_moving_solid_[2], pi_vege_moving_transp_[2];
    Ren::PipelineRef pi_skin_static_solid_[2], pi_skin_static_transp_[2];
    Ren::PipelineRef pi_skin_moving_solid_[2], pi_skin_moving_transp_[2];

    Ren::Framebuffer depth_fill_fb_[Ren::MaxFramesInFlight][2], depth_fill_vel_fb_[Ren::MaxFramesInFlight][2];
    int fb_to_use_ = 0;

  public:
    ExDepthFill(const DrawList **list, const view_state_t *view_state, bool clear_depth, const FgResRef vtx_buf1,
                const FgResRef vtx_buf2, const FgResRef ndx_buf, const FgResRef materials_buf,
                const FgResRef textures_buf, const BindlessTextureData *bindless_tex, const FgResRef instances_buf,
                const FgResRef instance_indices_buf, const FgResRef shared_data_buf, const FgResRef noise_tex,
                const FgResRef depth_tex, const FgResRef velocity_tex) {
        view_state_ = view_state;
        bindless_tex_ = bindless_tex;
        clear_depth_ = clear_depth;

        p_list_ = list;

        vtx_buf1_ = vtx_buf1;
        vtx_buf2_ = vtx_buf2;
        ndx_buf_ = ndx_buf;
        instances_buf_ = instances_buf;
        instance_indices_buf_ = instance_indices_buf;
        shared_data_buf_ = shared_data_buf;
        materials_buf_ = materials_buf;
        textures_buf_ = textures_buf;

        noise_tex_ = noise_tex;

        depth_tex_ = depth_tex;
        velocity_tex_ = velocity_tex;
    }

    void Execute(FgContext &ctx) override;
};
} // namespace Eng