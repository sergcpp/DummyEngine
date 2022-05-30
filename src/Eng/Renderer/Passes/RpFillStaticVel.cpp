#include "RpFillStaticVel.h"

#include <Ren/Context.h>
#include <Ren/RastState.h>
#include <Ren/Texture.h>

#include "../../Utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

#include "../assets/shaders/internal/blit_static_vel_interface.glsl"

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

    const Ren::Binding bindings[] = {
        {Ren::eBindTarget::Tex2D, BlitStaticVel::DEPTH_TEX_SLOT, *depth_tex.ref},
        {Ren::eBindTarget::UBuf, REN_UB_SHARED_DATA_LOC, 0, sizeof(SharedDataBlock), *unif_shared_data_buf.ref}};

    const Ren::RenderTarget render_targets[] = {{velocity_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store}};
    const Ren::RenderTarget depth_target = {depth_tex.ref, Ren::eLoadOp::None, Ren::eStoreOp::None, Ren::eLoadOp::Load,
                                            Ren::eStoreOp::None};

    BlitStaticVel::Params uniform_params;
    uniform_params.transform = Ren::Vec4f{0.0f, 0.0f, float(view_state_->act_res[0]), float(view_state_->act_res[1])};

    prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, blit_static_vel_prog_, render_targets, depth_target, rast_state,
                        builder.rast_state(), bindings, &uniform_params, sizeof(BlitStaticVel::Params), 0);
}

void RpFillStaticVel::LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocTex &depth_tex, RpAllocTex &velocity_tex) {
    if (!initialized) {
        blit_static_vel_prog_ = sh.LoadProgram(ctx, "blit_static_vel_prog", "internal/blit_static_vel.vert.glsl",
                                               "internal/blit_static_vel.frag.glsl");
        assert(blit_static_vel_prog_->ready());

        initialized = true;
    }
}
