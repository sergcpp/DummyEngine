#include "RpFillStaticVel.h"

#include <Ren/Context.h>
#include <Ren/RastState.h>
#include <Ren/Texture.h>

#include "../../Utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

#include "../assets/shaders/internal/blit_static_vel_interface.glsl"

void RpFillStaticVel::Setup(RpBuilder &builder, const ViewState *view_state, const char shared_data_buf[],
                            const char depth_tex[], const char velocity_tex[]) {
    view_state_ = view_state;

    shared_data_buf_ = builder.ReadBuffer(shared_data_buf, Ren::eResState::UniformBuffer,
                                          Ren::eStageBits::VertexShader | Ren::eStageBits::FragmentShader, *this);

    depth_tex_ = builder.ReadTexture(depth_tex, Ren::eResState::StencilTestDepthFetch,
                                     Ren::eStageBits::DepthAttachment | Ren::eStageBits::FragmentShader, *this);
    velocity_tex_ =
        builder.WriteTexture(velocity_tex, Ren::eResState::RenderTarget, Ren::eStageBits::ColorAttachment, *this);
}

void RpFillStaticVel::Execute(RpBuilder &builder) {
    RpAllocBuf &unif_shared_data_buf = builder.GetReadBuffer(shared_data_buf_);

    RpAllocTex &depth_tex = builder.GetReadTexture(depth_tex_);
    RpAllocTex &velocity_tex = builder.GetWriteTexture(velocity_tex_);

    LazyInit(builder.ctx(), builder.sh(), depth_tex, velocity_tex);

    Ren::RastState rast_state;
    rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);
    rast_state.stencil.enabled = true;
    rast_state.stencil.write_mask = 0x00;
    rast_state.stencil.compare_op = unsigned(Ren::eCompareOp::Equal);

    rast_state.viewport[2] = view_state_->act_res[0];
    rast_state.viewport[3] = view_state_->act_res[1];

    Ren::Program *blit_prog = blit_static_vel_prog_.get();

    const Ren::Binding bindings[] = {
        {Ren::eBindTarget::Tex2D, BlitStaticVel::DEPTH_TEX_SLOT, *depth_tex.ref},
        {Ren::eBindTarget::UBuf, REN_UB_SHARED_DATA_LOC, 0, sizeof(SharedDataBlock), *unif_shared_data_buf.ref}};

    BlitStaticVel::Params uniform_params;
    uniform_params.transform = Ren::Vec4f{0.0f, 0.0f, float(view_state_->act_res[0]), float(view_state_->act_res[1])};

    prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, blit_static_vel_prog_, velocity_fb_, render_pass_, rast_state,
                        builder.rast_state(), bindings, 2, &uniform_params, sizeof(BlitStaticVel::Params), 0);
}

void RpFillStaticVel::LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocTex &depth_tex, RpAllocTex &velocity_tex) {
    if (!initialized) {
        blit_static_vel_prog_ = sh.LoadProgram(ctx, "blit_static_vel_prog", "internal/blit_static_vel.vert.glsl",
                                               "internal/blit_static_vel.frag.glsl");
        assert(blit_static_vel_prog_->ready());

        initialized = true;
    }

    const Ren::RenderTarget render_targets[] = {{velocity_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store}};
    const Ren::RenderTarget depth_target = {depth_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store, Ren::eLoadOp::Load,
                                            Ren::eStoreOp::Store};

    if (!render_pass_.Setup(ctx.api_ctx(), render_targets, 1, depth_target, ctx.log())) {
        ctx.log()->Error("RpFillStaticVel: render_pass_ init failed!");
    }

    if (!velocity_fb_.Setup(ctx.api_ctx(), render_pass_, velocity_tex.desc.w, velocity_tex.desc.h, render_targets, 1,
                            depth_target, depth_target)) {
        ctx.log()->Error("RpFillStaticVel: output_fb_ init failed!");
    }
}
