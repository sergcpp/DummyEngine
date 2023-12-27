#pragma once

#include "../Graph/SubPass.h"
#include "../Renderer_DrawList.h"

#include <Ren/Pipeline.h>
#include <Ren/VertexInput.h>

class RpDepthFill : public RpExecutor {
    bool initialized = false;

    // lazily initialized data

    // temp data (valid only between Setup and Execute calls)
    const ViewState *view_state_ = nullptr;
    const BindlessTextureData *bindless_tex_ = nullptr;
    bool clear_depth_ = false;

    const DrawList **p_list_;

    RpResRef vtx_buf1_;
    RpResRef vtx_buf2_;
    RpResRef ndx_buf_;
    RpResRef instances_buf_;
    RpResRef instance_indices_buf_;
    RpResRef shared_data_buf_;
    RpResRef materials_buf_;
    RpResRef textures_buf_;
    RpResRef noise_tex_;

    RpResRef depth_tex_;
    RpResRef velocity_tex_;

    void LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh, RpAllocBuf &vtx_buf1, RpAllocBuf &vtx_buf2,
                  RpAllocBuf &ndx_buf, RpAllocTex &depth_tex, RpAllocTex &velocity_tex);
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

    Ren::Framebuffer depth_fill_fb_[Ren::MaxFramesInFlight][2], depth_fill_vel_fb_[Ren::MaxFramesInFlight][2];
    int fb_to_use_ = 0;

  public:
    void Setup(RpBuilder &builder, const DrawList &list, const ViewState *view_state, bool clear_depth,
               const Ren::BufferRef &vtx_buf1, const Ren::BufferRef &vtx_buf2, const Ren::BufferRef &ndx_buf,
               const Ren::BufferRef &materials_buf, const BindlessTextureData *bindless_tex, const char instances_buf[],
               const char instance_indices_buf[], const char shared_data_buf[], const Ren::Tex2DRef &noise_tex,
               const char main_depth_tex[], const char main_velocity_tex[]);

    void Setup(const DrawList **list, const ViewState *view_state, bool clear_depth, const RpResRef vtx_buf1,
               const RpResRef vtx_buf2, const RpResRef ndx_buf, const RpResRef materials_buf,
               const RpResRef textures_buf, const BindlessTextureData *bindless_tex, const RpResRef instances_buf,
               const RpResRef instance_indices_buf, const RpResRef shared_data_buf, const RpResRef noise_tex,
               const RpResRef depth_tex, const RpResRef velocity_tex) {
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

    void Execute(RpBuilder &builder) override;
};