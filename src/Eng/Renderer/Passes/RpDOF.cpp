#include "RpDOF.h"

#include <Ren/Camera.h>
#include <Ren/Context.h>
#include <Ren/GL.h>
#include <Ren/RastState.h>
#include <Ren/Texture.h>

#include "../../Utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

void RpDOF::Setup(RpBuilder &builder, const Ren::Camera *draw_cam,
                  const ViewState *view_state, const int orphan_index,
                  const char shared_data_buf[],
                  const char color_tex_name[], const char depth_tex_name[], 
                  const char depth_down_2x_name[], const char depth_down_4x_name[],
                  Ren::TexHandle down_buf_4x,
                  const char output_tex_name[]) {
    draw_cam_ = draw_cam;
    view_state_ = view_state;
    orphan_index_ = orphan_index;
    down_buf_4x_ = down_buf_4x;

    shared_data_buf_ = builder.ReadBuffer(shared_data_buf, *this);

    color_tex_ = builder.ReadTexture(color_tex_name, *this);
    depth_tex_ = builder.ReadTexture(depth_tex_name, *this);
    down_depth_2x_tex_ = builder.ReadTexture(depth_down_2x_name, *this);
    { // Buffer that holds 4x downsampled linear depth
        Ren::Tex2DParams params;
        params.w = view_state->scr_res[0] / 4;
        params.h = view_state->scr_res[1] / 4;
        params.format = Ren::eTexFormat::RawR32F;
        params.filter = Ren::eTexFilter::NoFilter;
        params.repeat = Ren::eTexRepeat::ClampToEdge;

        down_depth_4x_tex_ = builder.WriteTexture(depth_down_4x_name, params, *this);
    }
    blur_temp_4x_[0] = builder.WriteTexture("DOF temp 1", *this);
    blur_temp_4x_[1] = builder.WriteTexture("DOF temp 2", *this);
    { // Texture that holds near circle of confusion values
        Ren::Tex2DParams params;
        params.w = view_state->scr_res[0] / 4;
        params.h = view_state->scr_res[1] / 4;
        params.format = Ren::eTexFormat::RawR8;
        params.filter = Ren::eTexFilter::BilinearNoMipmap;
        params.repeat = Ren::eTexRepeat::ClampToEdge;

        down_tex_coc_[0] = builder.WriteTexture("DOF coc 1", params, *this);
        down_tex_coc_[1] = builder.WriteTexture("DOF coc 2", params, *this);
    }
    output_tex_ = builder.WriteTexture(output_tex_name, *this);
}

void RpDOF::Execute(RpBuilder &builder) {
    RpAllocTex &color_tex = builder.GetReadTexture(color_tex_);
    RpAllocTex &depth_tex = builder.GetReadTexture(depth_tex_);
    RpAllocTex &down_depth_2x_tex = builder.GetReadTexture(down_depth_2x_tex_);
    RpAllocTex &down_depth_4x_tex = builder.GetWriteTexture(down_depth_4x_tex_);
    RpAllocTex &blur1_4x_tex = builder.GetWriteTexture(blur_temp_4x_[0]);
    RpAllocTex &blur2_4x_tex = builder.GetWriteTexture(blur_temp_4x_[1]);
    RpAllocTex &coc1_tex = builder.GetWriteTexture(down_tex_coc_[0]);
    RpAllocTex &coc2_tex = builder.GetWriteTexture(down_tex_coc_[1]);
    RpAllocTex &output_tex = builder.GetWriteTexture(output_tex_);

    LazyInit(builder.ctx(), builder.sh(), down_depth_4x_tex, blur1_4x_tex, blur2_4x_tex,
             coc1_tex, coc2_tex, output_tex);

    const int hres_w = view_state_->scr_res[0] / 2, hres_h = view_state_->scr_res[1] / 2;
    const int qres_w = view_state_->scr_res[0] / 4, qres_h = view_state_->scr_res[1] / 4;

    RpAllocBuf &unif_shared_data_buf = builder.GetReadBuffer(shared_data_buf_);

    Ren::RastState rast_state;
    rast_state.cull_face.enabled = true;

    rast_state.viewport[2] = qres_w;
    rast_state.viewport[3] = qres_h;

    rast_state.Apply();
    Ren::RastState applied_state = rast_state;

    { // prepare coc buffer
        const PrimDraw::Binding binding = {Ren::eBindTarget::Tex2D, REN_BASE0_TEX_SLOT,
                                           down_depth_2x_tex.ref->handle()};

        const PrimDraw::Uniform uniforms[] = {
            {0, Ren::Vec4f{0.0f, 0.0f, 1.0f, 1.0f}},
            {1, Ren::Vec4f{-draw_cam_->focus_near_mul,
                           draw_cam_->focus_distance - 0.5f * draw_cam_->focus_depth,
                           0.0f, 0.0f}}};

        prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, {coc_fb_[0].id(), 0},
                            blit_dof_init_coc_prog_.get(), &binding, 1, uniforms, 2);
    }

    { // blur coc buffer
        PrimDraw::Binding binding = {Ren::eBindTarget::Tex2D, REN_BASE0_TEX_SLOT,
                                     coc1_tex.ref->handle()};

        PrimDraw::Uniform uniforms[] = {
            {0, Ren::Vec4f{0.0f, 0.0f, float(qres_w), float(qres_h)}}, {1, 0.0f}};

        prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, {blur_fb_[1].id(), 0},
                            blit_gauss_prog_.get(), &binding, 1, uniforms, 2);

        binding = {Ren::eBindTarget::Tex2D, REN_BASE0_TEX_SLOT,
                   blur2_4x_tex.ref->handle()};

        uniforms[0] = {0, Ren::Vec4f{0.0f, 0.0f, float(qres_w), float(qres_h)}};
        uniforms[1] = {1, 1.0f};

        prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, {coc_fb_[1].id(), 0},
                            blit_gauss_prog_.get(), &binding, 1, uniforms, 2);
    }

    { // downsample depth (once more)
        const PrimDraw::Binding bindings[] = {
            {Ren::eBindTarget::Tex2D, REN_BASE0_TEX_SLOT,
             down_depth_2x_tex.ref->handle()},
            {Ren::eBindTarget::UBuf, REN_UB_SHARED_DATA_LOC,
             orphan_index_ * SharedDataBlockSize, sizeof(SharedDataBlock),
             unif_shared_data_buf.ref->handle()}};

        const PrimDraw::Uniform uniforms[2] = {
            {0, Ren::Vec4f{0.0f, 0.0f, float(hres_w), float(hres_h)}}, {1, 0.0f}};

        prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, {depth_4x_fb_.id(), 0},
                            blit_down_depth_prog_.get(), bindings, 2, uniforms, 2);
    }

    { // calc near coc
        // TODO: hdr buf is unnecessary here
        const PrimDraw::Binding bindings[2] = {
            {Ren::eBindTarget::Tex2D, REN_BASE0_TEX_SLOT, coc1_tex.ref->handle()},
            {Ren::eBindTarget::Tex2D, REN_BASE1_TEX_SLOT, coc2_tex.ref->handle()}};

        const PrimDraw::Uniform uniforms[] = {
            {0, Ren::Vec4f{0.0f, 0.0f, float(qres_w), float(qres_h)}}};

        prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, {blur_fb_[1].id(), 0},
                            blit_dof_calc_near_prog_.get(), bindings, 2, uniforms, 1);
    }

    { // apply 3x3 blur to coc
        const PrimDraw::Binding binding = {Ren::eBindTarget::Tex2D, REN_BASE0_TEX_SLOT,
                                           blur2_4x_tex.ref->handle()};

        const PrimDraw::Uniform uniforms[] = {
            {0, Ren::Vec4f{0.0f, 0.0f, float(qres_w), float(qres_h)}}};

        prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, {coc_fb_[0].id(), 0},
                            blit_dof_small_blur_prog_.get(), &binding, 1, uniforms, 1);
    }

    { // blur color buffer
        glUseProgram(blit_dof_bilateral_prog_->id());

        PrimDraw::Binding bindings[2] = {
            {Ren::eBindTarget::Tex2D, REN_BASE0_TEX_SLOT,
             down_depth_4x_tex.ref->handle()},
            {Ren::eBindTarget::Tex2D, REN_BASE1_TEX_SLOT, down_buf_4x_}};

        PrimDraw::Uniform uniforms[3] = {
            {0, Ren::Vec4f{0.0f, 0.0f, float(qres_w), float(qres_h)}},
            {1, 0.0f},
            {2, draw_cam_->focus_distance}};

        prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, {blur_fb_[0].id(), 0},
                            blit_dof_bilateral_prog_.get(), bindings, 2, uniforms, 2);

        bindings[1] = {Ren::eBindTarget::Tex2D, REN_BASE1_TEX_SLOT,
                       blur1_4x_tex.ref->handle()};

        uniforms[0] = {0, Ren::Vec4f{0.0f, 0.0f, float(qres_w), float(qres_h)}};
        uniforms[1] = {1, 1.0f};

        prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, {blur_fb_[1].id(), 0},
                            blit_dof_bilateral_prog_.get(), bindings, 2, uniforms, 2);
    }

    { // apply 3x3 blur to color
        const PrimDraw::Binding binding = {Ren::eBindTarget::Tex2D, REN_BASE0_TEX_SLOT,
                                           down_buf_4x_};

        const PrimDraw::Uniform uniforms[] = {
            {0, Ren::Vec4f{0.0f, 0.0f, float(qres_w), float(qres_h)}}};

        prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, {blur_fb_[0].id(), 0},
                            blit_dof_small_blur_prog_.get(), &binding, 1, uniforms, 1);
    }

    rast_state.viewport[2] = view_state_->scr_res[0];
    rast_state.viewport[3] = view_state_->scr_res[1];

    rast_state.ApplyChanged(applied_state);
    applied_state = rast_state;

    { // combine dof buffers, apply blur
        Ren::Program *dof_combine_prog = view_state_->is_multisampled
                                             ? blit_dof_combine_ms_prog_.get()
                                             : blit_dof_combine_prog_.get();
        Ren::Vec4f dof_lerp_scale, dof_lerp_bias;
        {                            // calc dof lerp parameters
            const float d0 = 0.333f; // unblurred to small blur distance
            const float d1 = 0.333f; // small to medium blur distance
            const float d2 = 0.333f; // medium to large blur distance

            dof_lerp_scale = Ren::Vec4f{-1.0f / d0, -1.0f / d1, -1.0f / d2, 1.0f / d2};
            dof_lerp_bias =
                Ren::Vec4f{1.0f, (1.0f - d2) / d1, 1.0f / d2, (d2 - 1.0f) / d2};
        }

        const PrimDraw::Uniform uniforms[] = {
            {0, Ren::Vec4f{0.0f, 0.0f, float(view_state_->scr_res[0]),
                           float(view_state_->scr_res[1])}},
            {1, Ren::Vec3f{draw_cam_->focus_far_mul,
                           -(draw_cam_->focus_distance + 0.5f * draw_cam_->focus_depth),
                           1.0f}},
            {2, dof_lerp_scale},
            {3, dof_lerp_bias}};

        PrimDraw::Binding bindings[6];

        bindings[0] = {Ren::eBindTarget::UBuf, REN_UB_SHARED_DATA_LOC,
                       orphan_index_ * SharedDataBlockSize, sizeof(SharedDataBlock),
                       unif_shared_data_buf.ref->handle()};
        bindings[1] = {Ren::eBindTarget::Tex2D, REN_BASE0_TEX_SLOT,
                       color_tex.ref->handle()};

        if (view_state_->is_multisampled) {
            bindings[2] = {Ren::eBindTarget::Tex2DMs, 3, depth_tex.ref->handle()};
        } else {
            bindings[2] = {Ren::eBindTarget::Tex2D, 3, depth_tex.ref->handle()};
        }

        bindings[3] = {Ren::eBindTarget::Tex2D, REN_BASE1_TEX_SLOT,
                       blur1_4x_tex.ref->handle()};
        bindings[4] = {Ren::eBindTarget::Tex2D, REN_BASE2_TEX_SLOT,
                       blur2_4x_tex.ref->handle()};
        bindings[5] = {Ren::eBindTarget::Tex2D, 4, coc1_tex.ref->handle()};

        prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, {dof_fb_.id(), 0}, dof_combine_prog,
                            bindings, 6, uniforms, 4);
    }
}

void RpDOF::LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocTex &down_depth_4x_tex,
                     RpAllocTex &blur1_temp_4x, RpAllocTex &blur2_temp_4x,
                     RpAllocTex &coc1_tex, RpAllocTex &coc2_tex, RpAllocTex &output_tex) {
    if (!initialized) {
        blit_dof_init_coc_prog_ =
            sh.LoadProgram(ctx, "blit_dof_init_coc", "internal/blit.vert.glsl",
                           "internal/blit_dof_init_coc.frag.glsl");
        assert(blit_dof_init_coc_prog_->ready());
        blit_dof_bilateral_prog_ =
            sh.LoadProgram(ctx, "blit_dof_bilateral", "internal/blit.vert.glsl",
                           "internal/blit_dof_bilateral.frag.glsl");
        assert(blit_dof_bilateral_prog_->ready());
        blit_dof_calc_near_prog_ =
            sh.LoadProgram(ctx, "blit_dof_calc_near", "internal/blit.vert.glsl",
                           "internal/blit_dof_calc_near.frag.glsl");
        assert(blit_dof_calc_near_prog_->ready());
        blit_dof_small_blur_prog_ =
            sh.LoadProgram(ctx, "blit_dof_small_blur", "internal/blit.vert.glsl",
                           "internal/blit_dof_small_blur.frag.glsl");
        assert(blit_dof_small_blur_prog_->ready());
        blit_dof_combine_prog_ =
            sh.LoadProgram(ctx, "blit_dof_combine", "internal/blit.vert.glsl",
                           "internal/blit_dof_combine.frag.glsl");
        assert(blit_dof_combine_prog_->ready());
        blit_dof_combine_ms_prog_ =
            sh.LoadProgram(ctx, "blit_dof_combine_ms", "internal/blit.vert.glsl",
                           "internal/blit_dof_combine.frag.glsl@MSAA_4");
        assert(blit_dof_combine_ms_prog_->ready());
        blit_gauss_prog_ = sh.LoadProgram(ctx, "blit_gauss", "internal/blit.vert.glsl",
                                          "internal/blit_gauss.frag.glsl");
        assert(blit_gauss_prog_->ready());
        blit_down_depth_prog_ =
            sh.LoadProgram(ctx, "blit_down_depth", "internal/blit.vert.glsl",
                           "internal/blit_down_depth.frag.glsl");
        assert(blit_down_depth_prog_->ready());

        initialized = true;
    }

    if (!coc_fb_[0].Setup(coc1_tex.ref->handle(), {}, {}, false)) {
        ctx.log()->Error("RpDOF: coc_fb_[0] init failed!");
    }

    if (!coc_fb_[1].Setup(coc2_tex.ref->handle(), {}, {}, false)) {
        ctx.log()->Error("RpDOF: coc_fb_[1] init failed!");
    }

    if (!blur_fb_[0].Setup(blur1_temp_4x.ref->handle(), {}, {}, false)) {
        ctx.log()->Error("RpDOF: blur_fb_[0] init failed!");
    }

    if (!blur_fb_[1].Setup(blur2_temp_4x.ref->handle(), {}, {}, false)) {
        ctx.log()->Error("RpDOF: blur_fb_[1] init failed!");
    }

    if (!depth_4x_fb_.Setup(down_depth_4x_tex.ref->handle(), {}, {}, false)) {
        ctx.log()->Error("RpDOF: depth_4x_fb_ init failed!");
    }

    if (!dof_fb_.Setup(output_tex.ref->handle(), {}, {}, false)) {
        ctx.log()->Error("RpDOF: dof_fb_ init failed!");
    }
}
