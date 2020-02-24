#include "RpFXAA.h"

#include <Ren/Context.h>
#include <Ren/RastState.h>
#include <Ren/Texture.h>

#include "../../Utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

void RpFXAA::Setup(Graph::RpBuilder &builder, const ViewState *view_state,
                   Ren::TexHandle color_tex, Graph::ResourceHandle in_shared_data_buf,
                   Ren::TexHandle output_tex) {
    view_state_ = view_state;
    color_tex_ = color_tex;
    output_tex_ = output_tex;

    input_[0] = builder.ReadBuffer(in_shared_data_buf);
    input_count_ = 1;

    // output_[0] = builder.WriteBuffer(input_[0], *this);
    output_count_ = 0;
}

void RpFXAA::Execute(Graph::RpBuilder &builder) {
    LazyInit(builder.ctx(), builder.sh());

    Graph::AllocatedBuffer &unif_shared_data_buf = builder.GetReadBuffer(input_[0]);

    Ren::RastState rast_state;
    rast_state.cull_face.enabled = true;

    rast_state.viewport[2] = view_state_->act_res[0];
    rast_state.viewport[3] = view_state_->act_res[1];

    rast_state.Apply();
    Ren::RastState applied_state = rast_state;

    Ren::Program *blit_prog = blit_fxaa_prog_.get();

    const PrimDraw::Binding bindings[] = {
        {Ren::eBindTarget::Tex2D, REN_BASE0_TEX_SLOT, color_tex_},
        {Ren::eBindTarget::UBuf, REN_UB_SHARED_DATA_LOC,
         unif_shared_data_buf.ref->handle()}};

    const PrimDraw::Uniform uniforms[] = {
        {0, Ren::Vec4f{0.0f, 0.0f, 1.0f, 1.0f}},
        {12, Ren::Vec2f{1.0f / float(view_state_->scr_res[0]),
                        1.0f / float(view_state_->scr_res[1])}}};

    prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, {output_fb_.id(), 0}, blit_prog, bindings,
                        2, uniforms, 2);
}

void RpFXAA::LazyInit(Ren::Context &ctx, ShaderLoader &sh) {
    if (!initialized) {
        blit_fxaa_prog_ = sh.LoadProgram(ctx, "blit_fxaa_prog", "internal/blit.vert.glsl",
                                         "internal/blit_fxaa.frag.glsl");
        assert(blit_fxaa_prog_->ready());

        initialized = true;
    }

    if (!output_fb_.Setup(&output_tex_, 1, {}, {}, false)) {
        ctx.log()->Error("RpFXAA: output_fb_ init failed!");
    }
}
