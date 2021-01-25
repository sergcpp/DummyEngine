#include "RpTAA.h"

#include <Ren/Context.h>
#include <Ren/RastState.h>
#include <Ren/Texture.h>

#include "../../Utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

void RpTAA::Setup(RpBuilder &builder, const ViewState *view_state, const int orphan_index,
                  Ren::TexHandle history_tex, float reduced_average, float max_exposure,
                  const char shared_data_buf[], const char color_tex[],
                  const char depth_tex[], const char velocity_tex[],
                  const char output_tex_name[]) {
    view_state_ = view_state;
    orphan_index_ = orphan_index;
    history_tex_ = history_tex;

    reduced_average_ = reduced_average;
    max_exposure_ = max_exposure;

    shared_data_buf_ = builder.ReadBuffer(shared_data_buf, *this);

    clean_tex_ = builder.ReadTexture(color_tex, *this);
    depth_tex_ = builder.ReadTexture(depth_tex, *this);
    velocity_tex_ = builder.ReadTexture(velocity_tex, *this);

    { // Texture that holds resolved color
        Ren::Tex2DParams params;
        params.w = view_state->scr_res[0];
        params.h = view_state->scr_res[1];
        params.format = Ren::eTexFormat::RawRG11F_B10F;
        params.filter = Ren::eTexFilter::BilinearNoMipmap;
        params.repeat = Ren::eTexRepeat::ClampToEdge;

        output_tex_ = builder.WriteTexture(output_tex_name, params, *this);
    }
}

void RpTAA::Execute(RpBuilder &builder) {
    RpAllocBuf &unif_shared_data_buf = builder.GetReadBuffer(shared_data_buf_);

    RpAllocTex &clean_tex = builder.GetReadTexture(clean_tex_);
    RpAllocTex &depth_tex = builder.GetReadTexture(depth_tex_);
    RpAllocTex &velocity_tex = builder.GetReadTexture(velocity_tex_);
    RpAllocTex &output_tex = builder.GetWriteTexture(output_tex_);

    LazyInit(builder.ctx(), builder.sh(), depth_tex, velocity_tex, output_tex);

    Ren::RastState rast_state;
    rast_state.cull_face.enabled = true;

    rast_state.viewport[2] = view_state_->act_res[0];
    rast_state.viewport[3] = view_state_->act_res[1];

    rast_state.Apply();
    Ren::RastState applied_state = rast_state;

    { // Init static objects' velocities
        rast_state.stencil.enabled = true;
        rast_state.stencil.mask = 0x00;
        rast_state.stencil.test_func = Ren::eTestFunc::Equal;

        rast_state.ApplyChanged(applied_state);
        applied_state = rast_state;

        Ren::Program *blit_prog = blit_static_vel_prog_.get();

        const PrimDraw::Binding bindings[] = {
            {Ren::eBindTarget::Tex2D, 0, depth_tex.ref->handle()},
            {Ren::eBindTarget::UBuf, REN_UB_SHARED_DATA_LOC,
             orphan_index_ * SharedDataBlockSize, sizeof(SharedDataBlock),
             unif_shared_data_buf.ref->handle()}};

        const PrimDraw::Uniform uniforms[] = {{0, Ren::Vec4f{applied_state.viewport}}};

        prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, {velocity_fb_.id(), 0}, blit_prog,
                            bindings, 2, uniforms, 2);
    }

    { // Blit taa
        rast_state.stencil.enabled = false;
        rast_state.ApplyChanged(applied_state);
        applied_state = rast_state;

        Ren::Program *blit_prog = blit_taa_prog_.get();

        // exposure from previous frame
        float exposure = reduced_average_ > std::numeric_limits<float>::epsilon()
                             ? (1.0f / reduced_average_)
                             : 1.0f;
        exposure = std::min(exposure, max_exposure_);

        const PrimDraw::Binding bindings[] = {
            {Ren::eBindTarget::Tex2D, 0, clean_tex.ref->handle()},
            {Ren::eBindTarget::Tex2D, 1, history_tex_},
            {Ren::eBindTarget::Tex2D, 2, depth_tex.ref->handle()},
            {Ren::eBindTarget::Tex2D, 3, velocity_tex.ref->handle()}};

        const PrimDraw::Uniform uniforms[] = {
            {0, Ren::Vec4f{applied_state.viewport}},
            {13,
             Ren::Vec2f{float(view_state_->act_res[0]), float(view_state_->act_res[1])}},
            {14, exposure}};

        prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, {resolve_fb_.id(), 0}, blit_prog,
                            bindings, 4, uniforms, 3);
    }
}

void RpTAA::LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocTex &depth_tex,
                     RpAllocTex &velocity_tex, RpAllocTex &output_tex) {
    if (!initialized) {
        blit_taa_prog_ = sh.LoadProgram(ctx, "blit_taa_prog", "internal/blit.vert.glsl",
                                        "internal/blit_taa.frag.glsl");
        assert(blit_taa_prog_->ready());
        blit_static_vel_prog_ =
            sh.LoadProgram(ctx, "blit_static_vel_prog", "internal/blit.vert.glsl",
                           "internal/blit_static_vel.frag.glsl");
        assert(blit_static_vel_prog_->ready());

        initialized = true;
    }

    if (!velocity_fb_.Setup(&velocity_tex.ref->handle(), 1, {}, depth_tex.ref->handle(),
                            false)) {
        ctx.log()->Error("RpTAA: velocity_fb_ init failed!");
    }

    {
        const Ren::TexHandle textures[] = {output_tex.ref->handle(), history_tex_};
        if (!resolve_fb_.Setup(textures, 2, {}, {}, false)) {
            ctx.log()->Error("RpTAA: resolve_fb_ init failed!");
        }
    }
}
