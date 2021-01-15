#include "RpDOF.h"

#include <Ren/Context.h>
#include <Ren/RastState.h>
#include <Ren/Texture.h>

#include "../../Utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

void RpDOF::Setup(RpBuilder &builder, const Ren::Camera *draw_cam,
                  const ViewState *view_state, const int orphan_index,
                  Ren::TexHandle color_tex, Ren::TexHandle depth_tex,
                  Ren::TexHandle down_buf_4x, Ren::TexHandle down_depth_2x,
                  Ren::TexHandle down_depth_4x, Ren::TexHandle down_tex_coc[2],
                  Ren::TexHandle blur_temp_4x[2], Ren::TexHandle dof_buf,
                  Ren::TexHandle output_tex) {
    draw_cam_ = draw_cam;
    view_state_ = view_state;
    orphan_index_ = orphan_index;
    color_tex_ = color_tex;
    depth_tex_ = depth_tex;
    down_buf_4x_ = down_buf_4x;
    down_depth_2x_ = down_depth_2x;
    down_depth_4x_ = down_depth_4x;
    down_tex_coc_[0] = down_tex_coc[0];
    down_tex_coc_[1] = down_tex_coc[1];
    blur_temp_4x_[0] = blur_temp_4x[0];
    blur_temp_4x_[1] = blur_temp_4x[1];
    output_tex_ = output_tex;

    input_[0] = builder.ReadBuffer(SHARED_DATA_BUF);
    input_count_ = 1;

    // output_[0] = builder.WriteBuffer(input_[0], *this);
    output_count_ = 0;
}

void RpDOF::Execute(RpBuilder &builder) {
    LazyInit(builder.ctx(), builder.sh());

    const int hres_w = view_state_->scr_res[0] / 2, hres_h = view_state_->scr_res[1] / 2;
    const int qres_w = view_state_->scr_res[0] / 4, qres_h = view_state_->scr_res[1] / 4;

    RpAllocBuf &unif_shared_data_buf = builder.GetReadBuffer(input_[0]);

    Ren::RastState rast_state;
    rast_state.cull_face.enabled = true;

    rast_state.viewport[2] = qres_w;
    rast_state.viewport[3] = qres_h;

    rast_state.Apply();
    Ren::RastState applied_state = rast_state;

    { // prepare coc buffer
        const PrimDraw::Binding binding = {Ren::eBindTarget::Tex2D, REN_BASE0_TEX_SLOT,
                                           down_depth_2x_};

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
                                     down_tex_coc_[0]};

        PrimDraw::Uniform uniforms[] = {
            {0, Ren::Vec4f{0.0f, 0.0f, float(qres_w), float(qres_h)}}, {1, 0.0f}};

        prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, {blur_fb_[1].id(), 0},
                            blit_gauss_prog_.get(), &binding, 1, uniforms, 2);

        binding = {Ren::eBindTarget::Tex2D, REN_BASE0_TEX_SLOT, blur_temp_4x_[1]};

        uniforms[0] = {0, Ren::Vec4f{0.0f, 0.0f, float(qres_w), float(qres_h)}};
        uniforms[1] = {1, 1.0f};

        prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, {coc_fb_[1].id(), 0},
                            blit_gauss_prog_.get(), &binding, 1, uniforms, 2);
    }

    { // downsample depth (once more)
        const PrimDraw::Binding bindings[] = {
            {Ren::eBindTarget::Tex2D, REN_BASE0_TEX_SLOT, down_depth_2x_},
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
            {Ren::eBindTarget::Tex2D, REN_BASE0_TEX_SLOT, down_tex_coc_[0]},
            {Ren::eBindTarget::Tex2D, REN_BASE1_TEX_SLOT, down_tex_coc_[1]}};

        const PrimDraw::Uniform uniforms[] = {
            {0, Ren::Vec4f{0.0f, 0.0f, float(qres_w), float(qres_h)}}};

        prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, {blur_fb_[1].id(), 0},
                            blit_dof_calc_near_prog_.get(), bindings, 2, uniforms, 1);
    }

    { // apply 3x3 blur to coc
        const PrimDraw::Binding binding = {Ren::eBindTarget::Tex2D, REN_BASE0_TEX_SLOT,
                                           blur_temp_4x_[1]};

        const PrimDraw::Uniform uniforms[] = {
            {0, Ren::Vec4f{0.0f, 0.0f, float(qres_w), float(qres_h)}}};

        prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, {coc_fb_[0].id(), 0},
                            blit_dof_small_blur_prog_.get(), &binding, 1, uniforms, 1);
    }

    { // blur color buffer
        glUseProgram(blit_dof_bilateral_prog_->id());

        PrimDraw::Binding bindings[2] = {
            {Ren::eBindTarget::Tex2D, REN_BASE0_TEX_SLOT, down_depth_4x_},
            {Ren::eBindTarget::Tex2D, REN_BASE1_TEX_SLOT, down_buf_4x_}};

        PrimDraw::Uniform uniforms[3] = {
            {0, Ren::Vec4f{0.0f, 0.0f, float(qres_w), float(qres_h)}},
            {1, 0.0f},
            {2, draw_cam_->focus_distance}};

        prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, {blur_fb_[0].id(), 0},
                            blit_dof_bilateral_prog_.get(), bindings, 2, uniforms, 2);

        bindings[1] = {Ren::eBindTarget::Tex2D, REN_BASE1_TEX_SLOT, blur_temp_4x_[0]};

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
        bindings[1] = {Ren::eBindTarget::Tex2D, REN_BASE0_TEX_SLOT, color_tex_};

        if (view_state_->is_multisampled) {
            bindings[2] = {Ren::eBindTarget::Tex2DMs, 3, depth_tex_};
        } else {
            bindings[2] = {Ren::eBindTarget::Tex2D, 3, depth_tex_};
        }

        bindings[3] = {Ren::eBindTarget::Tex2D, REN_BASE1_TEX_SLOT, blur_temp_4x_[0]};
        bindings[4] = {Ren::eBindTarget::Tex2D, REN_BASE2_TEX_SLOT, blur_temp_4x_[1]};
        bindings[5] = {Ren::eBindTarget::Tex2D, 4, down_tex_coc_[0]};

        prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, {dof_fb_.id(), 0}, dof_combine_prog,
                            bindings, 6, uniforms, 4);
    }
}

void RpDOF::LazyInit(Ren::Context &ctx, ShaderLoader &sh) {
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

    if (!coc_fb_[0].Setup(&down_tex_coc_[0], 1, {}, {}, false)) {
        ctx.log()->Error("RpDOF: coc_fb_[0] init failed!");
    }

    if (!coc_fb_[1].Setup(&down_tex_coc_[1], 1, {}, {}, false)) {
        ctx.log()->Error("RpDOF: coc_fb_[1] init failed!");
    }

    if (!blur_fb_[0].Setup(&blur_temp_4x_[0], 1, {}, {}, false)) {
        ctx.log()->Error("RpDOF: blur_fb_[0] init failed!");
    }

    if (!blur_fb_[1].Setup(&blur_temp_4x_[1], 1, {}, {}, false)) {
        ctx.log()->Error("RpDOF: blur_fb_[1] init failed!");
    }

    if (!depth_4x_fb_.Setup(&down_depth_4x_, 1, {}, {}, false)) {
        ctx.log()->Error("RpDOF: depth_4x_fb_ init failed!");
    }

    if (!dof_fb_.Setup(&output_tex_, 1, {}, {}, false)) {
        ctx.log()->Error("RpDOF: dof_fb_ init failed!");
    }
}
