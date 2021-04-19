#include "RpDebugProbes.h"

#include <Ren/Context.h>
#include <Ren/RastState.h>

#include "../../Utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

void RpDebugProbes::Setup(RpBuilder &builder, const DrawList &list,
                          const ViewState *view_state, const int orphan_index,
                          const char shared_data_buf_name[],
                          const char output_tex_name[]) {

    view_state_ = view_state;
    orphan_index_ = orphan_index;

    probe_storage_ = list.probe_storage;
    probes_ = list.probes;

    shared_data_buf_ = builder.ReadBuffer(shared_data_buf_name, *this);

    output_tex_ = builder.WriteTexture(output_tex_name, *this);
}

void RpDebugProbes::Execute(RpBuilder &builder) {
    RpAllocTex &output_tex = builder.GetWriteTexture(output_tex_);

    LazyInit(builder.ctx(), builder.sh(), output_tex);
    DrawProbes(builder);
}

void RpDebugProbes::LazyInit(Ren::Context &ctx, ShaderLoader &sh,
                             RpAllocTex &output_tex) {
    if (!initialized) {
        probe_prog_ = sh.LoadProgram(ctx, "probe_prog", "internal/probe.vert.glsl",
                                     "internal/probe.frag.glsl");
        assert(probe_prog_->ready());

        initialized = true;
    }

    if (!draw_fb_.Setup(output_tex.ref->handle(), {}, {},
                        view_state_->is_multisampled)) {
        ctx.log()->Error("RpDebugProbes: draw_fb_ init failed!");
    }
}

void RpDebugProbes::DrawProbes(RpBuilder &builder) {
    Ren::RastState rast_state;
    rast_state.cull_face.enabled = true;
    rast_state.viewport[2] = view_state_->act_res[0];
    rast_state.viewport[3] = view_state_->act_res[1];

    rast_state.Apply();
    Ren::RastState applied_state = rast_state;

    RpAllocBuf &unif_shared_data_buf = builder.GetReadBuffer(shared_data_buf_);

    const PrimDraw::Binding bindings[] = {
        {Ren::eBindTarget::TexCubeArray, REN_BASE0_TEX_SLOT, probe_storage_->handle()},
        {Ren::eBindTarget::UBuf, REN_UB_SHARED_DATA_LOC,
         orphan_index_ * SharedDataBlockSize, sizeof(SharedDataBlock),
         unif_shared_data_buf.ref->handle()}};

    debug_roughness_ += 0.1f;
    if (debug_roughness_ > 8.0f) {
        debug_roughness_ = 0.0f;
    }

    for (int i = 0; i < int(probes_.count); i++) {
        const ProbeItem &pr = probes_.data[i];

        const Ren::Mat4f world_from_object = Ren::Translate(
            Ren::Mat4f{}, Ren::Vec3f{pr.position[0], pr.position[1], pr.position[2]});

        const PrimDraw::Uniform uniforms[] = {
            {REN_U_M_MATRIX_LOC, &world_from_object}, {1, debug_roughness_}, {2, i}};

        prim_draw_.DrawPrim(PrimDraw::ePrim::Sphere, {draw_fb_.id(), 0},
                            probe_prog_.get(), bindings,
                            sizeof(bindings) / sizeof(bindings[0]), uniforms,
                            sizeof(uniforms) / sizeof(uniforms[0]));
    }
}
