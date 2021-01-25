#include "RpDownDepth.h"

#include <Ren/Context.h>
#include <Ren/Program.h>
#include <Ren/RastState.h>

#include "../PrimDraw.h"
#include "../Renderer_Structs.h"
#include "../../Utils/ShaderLoader.h"

void RpDownDepth::Setup(RpBuilder &builder, const ViewState *view_state,
                        const int orphan_index, const char shared_data_buf[],
                        const char depth_tex[], const char output_tex[]) {
    view_state_ = view_state;
    orphan_index_ = orphan_index;

    shared_data_buf_ = builder.ReadBuffer(shared_data_buf, *this);

    depth_tex_ = builder.ReadTexture(depth_tex, *this);
    { // Texture that holds 2x downsampled linear depth
        Ren::Tex2DParams params;
        params.w = view_state->scr_res[0] / 2;
        params.h = view_state->scr_res[1] / 2;
        params.format = Ren::eTexFormat::RawR32F;
        params.filter = Ren::eTexFilter::NoFilter;
        params.repeat = Ren::eTexRepeat::ClampToEdge;

        depth_down_2x_tex_ = builder.WriteTexture(output_tex, params, *this);
    }
}

void RpDownDepth::Execute(RpBuilder &builder) {
    RpAllocBuf &unif_shared_data_buf = builder.GetReadBuffer(shared_data_buf_);
    RpAllocTex &depth_tex = builder.GetReadTexture(depth_tex_);
    RpAllocTex &down_depth_2x_tex = builder.GetWriteTexture(depth_down_2x_tex_);

    LazyInit(builder.ctx(), builder.sh(), down_depth_2x_tex);

    Ren::RastState rast_state;

    rast_state.viewport[2] = view_state_->act_res[0] / 2;
    rast_state.viewport[3] = view_state_->act_res[1] / 2;

    rast_state.Apply();
    Ren::RastState applied_state = rast_state;

    Ren::Program *down_depth_prog = view_state_->is_multisampled
                                        ? blit_down_depth_ms_prog_.get()
                                        : blit_down_depth_prog_.get();

    const PrimDraw::Binding bindings[] = {
        {view_state_->is_multisampled ? Ren::eBindTarget::Tex2DMs
                                      : Ren::eBindTarget::Tex2D,
         REN_BASE0_TEX_SLOT, depth_tex.ref->handle()},
        {Ren::eBindTarget::UBuf, REN_UB_SHARED_DATA_LOC,
         orphan_index_ * SharedDataBlockSize, sizeof(SharedDataBlock),
         unif_shared_data_buf.ref->handle()}};

    const PrimDraw::Uniform uniforms[] = {
        {0, Ren::Vec4f{0.0f, 0.0f, float(view_state_->act_res[0]),
                       float(view_state_->act_res[1])}},
        {1, 1.0f}};

    prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, {depth_down_fb_.id(), 0}, down_depth_prog,
                        bindings, 2, uniforms, 2);
}

void RpDownDepth::LazyInit(Ren::Context &ctx, ShaderLoader &sh,
                           RpAllocTex &down_depth_2x_tex) {
    if (!initialized) {
        blit_down_depth_prog_ =
            sh.LoadProgram(ctx, "blit_down_depth", "internal/blit.vert.glsl",
                           "internal/blit_down_depth.frag.glsl");
        assert(blit_down_depth_prog_->ready());
        blit_down_depth_ms_prog_ =
            sh.LoadProgram(ctx, "blit_down_depth_ms", "internal/blit.vert.glsl",
                           "internal/blit_down_depth.frag.glsl@MSAA_4");
        assert(blit_down_depth_ms_prog_->ready());

        initialized = true;
    }

    if (!depth_down_fb_.Setup(&down_depth_2x_tex.ref->handle(), 1, {}, {}, false)) {
        ctx.log()->Error("RpDownDepth: depth_down_fb_ init failed!");
    }
}
