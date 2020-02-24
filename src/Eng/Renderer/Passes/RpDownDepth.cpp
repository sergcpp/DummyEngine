#include "RpDownDepth.h"

#include "../../Utils/ShaderLoader.h"
#include "../Renderer_Structs.h"

void RpDownDepth::Setup(Graph::RpBuilder &builder, const ViewState *view_state,
                        Ren::TexHandle depth_tex, Ren::TexHandle down_depth_2x_tex,
                        Graph::ResourceHandle in_shared_data_buf) {
    depth_tex_ = depth_tex;
    down_depth_2x_tex_ = down_depth_2x_tex;
    view_state_ = view_state;

    input_[0] = builder.ReadBuffer(in_shared_data_buf);
    input_count_ = 1;

    // output_[0] = builder.WriteBuffer(input_[0], *this);
    output_count_ = 0;
}

void RpDownDepth::Execute(Graph::RpBuilder &builder) {
    LazyInit(builder.ctx(), builder.sh());

    Ren::RastState rast_state;

    rast_state.viewport[2] = view_state_->act_res[0] / 2;
    rast_state.viewport[3] = view_state_->act_res[1] / 2;

    rast_state.Apply();
    Ren::RastState applied_state = rast_state;

    Ren::Program *down_depth_prog = view_state_->is_multisampled
                                        ? blit_down_depth_ms_prog_.get()
                                        : blit_down_depth_prog_.get();

    Graph::AllocatedBuffer &unif_shared_data_buf = builder.GetReadBuffer(input_[0]);

    const PrimDraw::Binding bindings[] = {{view_state_->is_multisampled
                                               ? Ren::eBindTarget::Tex2DMs
                                               : Ren::eBindTarget::Tex2D,
                                           REN_BASE0_TEX_SLOT, depth_tex_},
                                          {Ren::eBindTarget::UBuf, REN_UB_SHARED_DATA_LOC,
                                           unif_shared_data_buf.ref->handle()}};

    const PrimDraw::Uniform uniforms[] = {
        {0, Ren::Vec4f{0.0f, 0.0f, float(view_state_->act_res[0]),
                       float(view_state_->act_res[1])}},
        {1, 1.0f}};

    prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, {depth_down_fb_.id(), 0}, down_depth_prog,
                        bindings, 2, uniforms, 2);
}

void RpDownDepth::LazyInit(Ren::Context &ctx, ShaderLoader &sh) {
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

    if (!depth_down_fb_.Setup(&down_depth_2x_tex_, 1, {}, {}, false)) {
        ctx.log()->Error("RpDownDepth: depth_down_fb_ init failed!");
    }
}
