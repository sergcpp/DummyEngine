#include "RpDebugProbes.h"
#if 0
#include <Ren/Context.h>
#include <Ren/RastState.h>

#include "../../Utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"
#include "../assets/shaders/internal/probe_interface.h"

void RpDebugProbes::Setup(RpBuilder &builder, const DrawList &list, const ViewState *view_state,
                          const char shared_data_buf_name[], const char output_tex_name[]) {

    view_state_ = view_state;

    probe_storage_ = list.probe_storage;
    probes_ = list.probes;

    shared_data_buf_ = builder.ReadBuffer(shared_data_buf_name, Ren::eResState::UniformBuffer,
                                          Ren::eStageBits::VertexShader | Ren::eStageBits::FragmentShader, *this);

    output_tex_ =
        builder.WriteTexture(output_tex_name, Ren::eResState::RenderTarget, Ren::eStageBits::ColorAttachment, *this);
}

void RpDebugProbes::Execute(RpBuilder &builder) {
    RpAllocTex &output_tex = builder.GetWriteTexture(output_tex_);

    LazyInit(builder.ctx(), builder.sh(), output_tex);
    DrawProbes(builder);
}

void RpDebugProbes::LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh, RpAllocTex &output_tex) {
    if (!initialized) {
        probe_prog_ = sh.LoadProgram(ctx, "probe_prog", "internal/probe.vert.glsl", "internal/probe.frag.glsl");
        assert(probe_prog_->ready());

        initialized = true;
    }

    if (!draw_fb_.Setup(ctx.api_ctx(), {}, output_tex.desc.w, output_tex.desc.h, output_tex.ref, {}, {},
                        view_state_->is_multisampled)) {
        ctx.log()->Error("RpDebugProbes: draw_fb_ init failed!");
    }
}

void RpDebugProbes::DrawProbes(RpBuilder &builder) {
    Ren::RastState rast_state;
    rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);
    rast_state.viewport[2] = view_state_->act_res[0];
    rast_state.viewport[3] = view_state_->act_res[1];

    rast_state.ApplyChanged(builder.rast_state());
    builder.rast_state() = rast_state;

    RpAllocBuf &unif_shared_data_buf = builder.GetReadBuffer(shared_data_buf_);

    const Ren::Binding bindings[] = {
        {Ren::eBindTarget::TexCubeArray, BIND_BASE0_TEX, *probe_storage_},
        {Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, 0, sizeof(SharedDataBlock), *unif_shared_data_buf.ref}};

    debug_roughness_ += 0.1f;
    if (debug_roughness_ > 8.0f) {
        debug_roughness_ = 0.0f;
    }

    for (int i = 0; i < int(probes_.count); i++) {
        const ProbeItem &pr = probes_.data[i];

        const Ren::Mat4f world_from_object =
            Ren::Translate(Ren::Mat4f{}, Ren::Vec3f{pr.position[0], pr.position[1], pr.position[2]});

        const PrimDraw::Uniform uniforms[] = {
            {Probe::U_M_MATRIX_LOC, &world_from_object}, {1, debug_roughness_}, {2, i}};

        prim_draw_.DrawPrim(PrimDraw::ePrim::Sphere, {&draw_fb_, 0}, probe_prog_.get(), bindings,
                            sizeof(bindings) / sizeof(bindings[0]), uniforms, sizeof(uniforms) / sizeof(uniforms[0]));
    }
}
#endif