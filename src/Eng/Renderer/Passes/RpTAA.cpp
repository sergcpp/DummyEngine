#include "RpTAA.h"

#include <Ren/Context.h>
#include <Ren/RastState.h>
#include <Ren/Texture.h>

#include "../../Utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

void RpTAA::Setup(Graph::RpBuilder &builder, const ViewState *view_state,
                  Ren::TexHandle depth_tex, Ren::TexHandle clean_tex,
                  Ren::TexHandle history_tex, Ren::TexHandle velocity_tex,
                  float reduced_average, float max_exposure,
                  Graph::ResourceHandle in_shared_data_buf, Ren::TexHandle output_tex) {
    view_state_ = view_state;
    depth_tex_ = depth_tex;
    clean_tex_ = clean_tex;
    history_tex_ = history_tex;
    velocity_tex_ = velocity_tex;
    output_tex_ = output_tex;

    reduced_average_ = reduced_average;
    max_exposure_ = max_exposure;

    input_[0] = builder.ReadBuffer(in_shared_data_buf);
    input_count_ = 1;

    // output_[0] = builder.WriteBuffer(input_[0], *this);
    output_count_ = 0;
}

void RpTAA::Execute(Graph::RpBuilder &builder) {
    LazyInit(builder.ctx(), builder.sh());

    Graph::AllocatedBuffer &unif_shared_data_buf = builder.GetReadBuffer(input_[0]);

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

        const PrimDraw::Binding bindings[] = {{Ren::eBindTarget::Tex2D, 0, depth_tex_},
                                              {Ren::eBindTarget::UBuf,
                                               REN_UB_SHARED_DATA_LOC,
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
            {Ren::eBindTarget::Tex2D, 0, clean_tex_},
            {Ren::eBindTarget::Tex2D, 1, history_tex_},
            {Ren::eBindTarget::Tex2D, 2, depth_tex_},
            {Ren::eBindTarget::Tex2D, 3, velocity_tex_}};

        const PrimDraw::Uniform uniforms[] = {
            {0, Ren::Vec4f{applied_state.viewport}},
            {13,
             Ren::Vec2f{float(view_state_->act_res[0]), float(view_state_->act_res[1])}},
            {14, exposure}};

        prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, {resolve_fb_.id(), 0}, blit_prog,
                            bindings, 4, uniforms, 3);
    }
}

void RpTAA::LazyInit(Ren::Context &ctx, ShaderLoader &sh) {
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

    if (!velocity_fb_.Setup(&velocity_tex_, 1, {}, depth_tex_, false)) {
        ctx.log()->Error("RpTAA: velocity_fb_ init failed!");
    }

    {
        const Ren::TexHandle textures[] = {output_tex_, history_tex_};
        if (!resolve_fb_.Setup(textures, 2, {}, {}, false)) {
            ctx.log()->Error("RpTAA: resolve_fb_ init failed!");
        }
    }
}
