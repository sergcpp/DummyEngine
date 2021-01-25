#include "RpDownColor.h"

#include <Ren/Context.h>
#include <Ren/Program.h>
#include <Ren/RastState.h>

#include "../../Utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

void RpDownColor::Setup(RpBuilder &builder, const ViewState *view_state, int orphan_index,
                        const char shared_data_buf[], const char color_tex_name[],
                        Ren::TexHandle output_tex) {
    output_tex_ = output_tex;
    view_state_ = view_state;
    orphan_index_ = orphan_index;

    shared_data_buf_ = builder.ReadBuffer(shared_data_buf, *this);
    color_tex_ = builder.ReadTexture(color_tex_name, *this);
}

void RpDownColor::Execute(RpBuilder &builder) {
    LazyInit(builder.ctx(), builder.sh());

    Ren::RastState rast_state;

    rast_state.viewport[2] = view_state_->act_res[0] / 4;
    rast_state.viewport[3] = view_state_->act_res[1] / 4;

    rast_state.Apply();
    Ren::RastState applied_state = rast_state;

    Ren::Program *down_prog = blit_down_prog_.get();

    RpAllocBuf &unif_shared_data_buf = builder.GetReadBuffer(shared_data_buf_);
    RpAllocTex &color_tex = builder.GetReadTexture(color_tex_);

    PrimDraw::Binding bindings[2];

    bindings[0] = {Ren::eBindTarget::Tex2D, REN_BASE0_TEX_SLOT, color_tex.ref->handle()};
    bindings[1] = {Ren::eBindTarget::UBuf, REN_UB_SHARED_DATA_LOC,
                   orphan_index_ * SharedDataBlockSize, sizeof(SharedDataBlock),
                   unif_shared_data_buf.ref->handle()};

    const PrimDraw::Uniform uniforms[] = {
        {0, Ren::Vec4f{0.0f, 0.0f,
                       float(view_state_->act_res[0]) / float(view_state_->scr_res[0]),
                       float(view_state_->act_res[1]) / float(view_state_->scr_res[1])}}};

    prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, {draw_fb_.id(), 0}, down_prog, bindings, 2,
                        uniforms, 1);
}

void RpDownColor::LazyInit(Ren::Context &ctx, ShaderLoader &sh) {
    if (!initialized) {
        blit_down_prog_ = sh.LoadProgram(ctx, "blit_down", "internal/blit.vert.glsl",
                                         "internal/blit_down.frag.glsl");
        assert(blit_down_prog_->ready());

        initialized = true;
    }

    if (!draw_fb_.Setup(&output_tex_, 1, {}, {}, false)) {
        ctx.log()->Error("RpDownColor: draw_fb_ init failed!");
    }
}
