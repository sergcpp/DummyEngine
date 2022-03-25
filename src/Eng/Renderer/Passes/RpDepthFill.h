#pragma once

#include "../Graph/GraphBuilder.h"
#include "../Renderer_DrawList.h"

#include <Ren/Pipeline.h>
#include <Ren/VertexInput.h>

class RpDepthFill : public RenderPassBase {
    bool initialized = false;

    // lazily initialized data

    // temp data (valid only between Setup and Execute calls)
    const ViewState *view_state_ = nullptr;
    const BindlessTextureData *bindless_tex_ = nullptr;
    bool clear_depth_ = false;

    uint64_t render_flags_ = 0;
    const Ren::MaterialStorage *materials_ = nullptr;
    DynArrayConstRef<uint32_t> zfill_batch_indices;
    DynArrayConstRef<BasicDrawBatch> zfill_batches;

    RpResource vtx_buf1_;
    RpResource vtx_buf2_;
    RpResource ndx_buf_;
    RpResource instances_buf_;
    RpResource instance_indices_buf_;
    RpResource shared_data_buf_;
    RpResource materials_buf_;
    RpResource textures_buf_;
    RpResource noise_tex_;

    RpResource depth_tex_;
    RpResource velocity_tex_;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocBuf &vtx_buf1, RpAllocBuf &vtx_buf2, RpAllocBuf &ndx_buf,
                  RpAllocTex &depth_tex, RpAllocTex &velocity_tex);
    void DrawDepth(RpBuilder &builder, RpAllocBuf &vtx_buf1, RpAllocBuf &vtx_buf2, RpAllocBuf &ndx_buf);

    Ren::RenderPass rp_depth_only_[2], rp_depth_velocity_[2];

    Ren::VertexInput vi_solid_, vi_vege_solid_, vi_transp_, vi_vege_transp_, vi_skin_solid_, vi_skin_transp_;

    Ren::Pipeline pi_static_solid_[2], pi_static_transp_[2];
    Ren::Pipeline pi_moving_solid_[2], pi_moving_transp_[2];
    Ren::Pipeline pi_vege_static_solid_[2], pi_vege_static_solid_vel_[2];
    Ren::Pipeline pi_vege_static_transp_[2], pi_vege_static_transp_vel_[2];
    Ren::Pipeline pi_vege_moving_solid_vel_[2], pi_vege_moving_transp_vel_[2];
    Ren::Pipeline pi_skin_static_solid_[2], pi_skin_static_solid_vel_[2];
    Ren::Pipeline pi_skin_static_transp_[2], pi_skin_static_transp_vel_[2];
    Ren::Pipeline pi_skin_moving_solid_vel_[2], pi_skin_moving_transp_vel_[2];

    Ren::Framebuffer depth_fill_fb_[Ren::MaxFramesInFlight], depth_fill_vel_fb_[Ren::MaxFramesInFlight];

  public:
    void Setup(RpBuilder &builder, const DrawList &list, const ViewState *view_state, bool clear_depth,
               const Ren::BufferRef &vtx_buf1, const Ren::BufferRef &vtx_buf2, const Ren::BufferRef &ndx_buf,
               const Ren::BufferRef &materials_buf, const BindlessTextureData *bindless_tex, const char instances_buf[],
               const char instance_indices_buf[], const char shared_data_buf[], const Ren::Tex2DRef &noise_tex,
               const char main_depth_tex[], const char main_velocity_tex[]);
    void Execute(RpBuilder &builder) override;

    const char *name() const override { return "DEPTH FILL"; }
};