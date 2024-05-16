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

    Ren::Vec3f draw_cam_pos_;

    // lazily initialized data
    Ren::ProgramRef prog_skydome_;

    RpResRef shared_data_buf_;
    RpResRef env_tex_;

    RpResRef color_tex_;
    RpResRef depth_tex_;

    void LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh);
    void DrawSkydome(RpBuilder &builder, RpAllocBuf &vtx_buf1, RpAllocBuf &vtx_buf2, RpAllocBuf &ndx_buf,
                     RpAllocTex &color_tex, RpAllocTex &depth_tex);

  public:
    RpSkydome(PrimDraw &prim_draw) : prim_draw_(prim_draw) {}

    void Setup(const DrawList &list, const ViewState *view_state, const RpResRef shared_data_buf,
               const RpResRef env_tex, const RpResRef color_tex, const RpResRef depth_tex) {
        view_state_ = view_state;

        draw_cam_pos_ = list.draw_cam.world_position();

        shared_data_buf_ = shared_data_buf;
        env_tex_ = env_tex;

        color_tex_ = color_tex;
        depth_tex_ = depth_tex;
    }

    void Execute(RpBuilder &builder) override;
};
} // namespace Eng