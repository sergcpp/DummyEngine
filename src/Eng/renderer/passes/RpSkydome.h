#pragma once

#include "../Renderer_DrawList.h"
#include "../graph/SubPass.h"

#include <Ren/Pipeline.h>
#include <Ren/RastState.h>
#include <Ren/RenderPass.h>
#include <Ren/VertexInput.h>

namespace Eng {
class PrimDraw;

class RpSkydome : public RpExecutor {
    PrimDraw &prim_draw_;
    bool initialized = false;

    // temp data (valid only between Setup and Execute calls)
    const ViewState *view_state_ = nullptr;
    bool clear_ = false;

    Ren::Vec3f draw_cam_pos_;

    // lazily initialized data
    Ren::RenderPass render_pass_[2];
    Ren::VertexInput vtx_input_;
    Ren::Pipeline pipeline_[2];
    Ren::Framebuffer framebuf_[Ren::MaxFramesInFlight][2];
    int fb_to_use_ = 0;

    RpResRef shared_data_buf_;
    RpResRef env_tex_;
    RpResRef vtx_buf1_;
    RpResRef vtx_buf2_;
    RpResRef ndx_buf_;

    RpResRef color_tex_;
    RpResRef depth_tex_;

    void LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh, RpAllocBuf &vtx_buf1, RpAllocBuf &vtx_buf2,
                  RpAllocBuf &ndx_buf, RpAllocTex &color_tex, RpAllocTex &depth_tex);
    void DrawSkydome(RpBuilder &builder, RpAllocBuf &vtx_buf1, RpAllocBuf &vtx_buf2, RpAllocBuf &ndx_buf,
                     RpAllocTex &color_tex, RpAllocTex &depth_tex);

  public:
    RpSkydome(PrimDraw &prim_draw) : prim_draw_(prim_draw) {}
    ~RpSkydome();

    void Setup(const DrawList &list, const ViewState *view_state, bool clear, const RpResRef vtx_buf1,
               const RpResRef vtx_buf2, const RpResRef ndx_buf, const RpResRef shared_data_buf, const RpResRef env_tex,
               const RpResRef color_tex, const RpResRef depth_tex) {
        view_state_ = view_state;
        clear_ = clear;

        draw_cam_pos_ = list.draw_cam.world_position();

        shared_data_buf_ = shared_data_buf;
        env_tex_ = env_tex;
        vtx_buf1_ = vtx_buf1;
        vtx_buf2_ = vtx_buf2;
        ndx_buf_ = ndx_buf;

        color_tex_ = color_tex;
        depth_tex_ = depth_tex;
    }

    void Execute(RpBuilder &builder) override;
};
} // namespace Eng