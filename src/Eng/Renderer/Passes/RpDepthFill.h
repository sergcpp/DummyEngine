#pragma once

#include "../Graph/GraphBuilder.h"
#include "../Renderer_DrawList.h"

#if defined(USE_GL_RENDER)
#include <Ren/VaoGL.h>
#endif

class RpDepthFill : public RenderPassBase {
    bool initialized = false;
    int orphan_index_ = 0;

    // lazily initialized data
    Ren::ProgramRef fillz_solid_prog_, fillz_solid_mov_prog_, fillz_vege_solid_prog_,
        fillz_vege_solid_vel_prog_, fillz_vege_solid_vel_mov_prog_, fillz_transp_prog_,
        fillz_transp_mov_prog_, fillz_vege_transp_prog_, fillz_vege_transp_vel_prog_,
        fillz_vege_transp_vel_mov_prog_, fillz_skin_solid_prog_,
        fillz_skin_solid_vel_prog_, fillz_skin_solid_vel_mov_prog_,
        fillz_skin_transp_prog_, fillz_skin_transp_vel_prog_,
        fillz_skin_transp_vel_mov_prog_;
    Ren::Tex2DRef dummy_white_;

    // temp data (valid only between Setup and Execute calls)
    const ViewState *view_state_ = nullptr;

    uint32_t render_flags_ = 0;
    DynArrayConstRef<uint32_t> zfill_batch_indices;
    DynArrayConstRef<DepthDrawBatch> zfill_batches;

    RpResource instances_buf_;
    RpResource shared_data_buf_;

    RpResource depth_tex_;
    RpResource velocity_tex_;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocTex &depth_tex,
                  RpAllocTex &velocity_tex);
    void DrawDepth(RpBuilder &builder);

#if defined(USE_GL_RENDER)
    Ren::Vao depth_pass_solid_vao_, depth_pass_vege_solid_vao_, depth_pass_transp_vao_,
        depth_pass_vege_transp_vao_, depth_pass_skin_solid_vao_,
        depth_pass_skin_transp_vao_;

    Ren::Framebuffer depth_fill_fb_, depth_fill_vel_fb_;
#endif
  public:
    void Setup(RpBuilder &builder, const DrawList &list, const ViewState *view_state,
               int orphan_index, const char instances_buf[], const char shared_data_buf[],
               const char main_depth_tex[], const char main_velocity_tex[]);
    void Execute(RpBuilder &builder) override;

    const char *name() const override { return "DEPTH FILL"; }
};