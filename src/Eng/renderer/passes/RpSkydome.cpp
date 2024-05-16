#include "RpSkydome.h"

#include <Ren/Context.h>
#include <Ren/DrawCall.h>

#include "../../utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"
#include "../shaders/skydome_interface.h"

void Eng::RpSkydome::Execute(RpBuilder &builder) {
    LazyInit(builder.ctx(), builder.sh());

    RpAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(shared_data_buf_);
    RpAllocTex &env_tex = builder.GetReadTexture(env_tex_);

    RpAllocTex &color_tex = builder.GetWriteTexture(color_tex_);
    RpAllocTex &depth_tex = builder.GetWriteTexture(depth_tex_);

    Ren::RastState rast_state;
    rast_state.poly.cull = uint8_t(Ren::eCullFace::Front);
    rast_state.viewport[2] = view_state_->act_res[0];
    rast_state.viewport[3] = view_state_->act_res[1];
    rast_state.depth.test_enabled = true;
    rast_state.depth.compare_op = unsigned(Ren::eCompareOp::Less);
    rast_state.blend.enabled = false;

    rast_state.stencil.enabled = true;
    rast_state.stencil.write_mask = 0xff;
    rast_state.stencil.pass = unsigned(Ren::eStencilOp::Replace);

    const Ren::Binding bindings[] = {{Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, *unif_sh_data_buf.ref},
                                     {Ren::eBindTarget::Tex2D, Skydome::ENV_TEX_SLOT, *env_tex.ref}};

    Ren::Mat4f translate_matrix;
    translate_matrix = Translate(translate_matrix, draw_cam_pos_);

    Ren::Mat4f scale_matrix;
    scale_matrix = Scale(scale_matrix, Ren::Vec3f{5000.0f, 5000.0f, 5000.0f});

    Skydome::Params uniform_params = {};
    uniform_params.xform = translate_matrix * scale_matrix;

    const Ren::RenderTarget color_targets[] = {{color_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store}};
    const Ren::RenderTarget depth_target = {depth_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store, Ren::eLoadOp::Load,
                                            Ren::eStoreOp::Store};

    prim_draw_.DrawPrim(PrimDraw::ePrim::Sphere, prog_skydome_, color_targets, depth_target, rast_state,
                        builder.rast_state(), bindings, &uniform_params, sizeof(uniform_params), 0);
}

void Eng::RpSkydome::LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh) {
    if (!initialized) {
        prog_skydome_ = sh.LoadProgram(ctx, "skydome", "internal/skydome.vert.glsl", "internal/skydome.frag.glsl");
        assert(prog_skydome_->ready());

        initialized = true;
    }
}
