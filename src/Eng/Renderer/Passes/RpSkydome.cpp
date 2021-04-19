#include "RpSkydome.h"

#include <Ren/Context.h>

#include "../Renderer_Structs.h"
#include "../../Utils/ShaderLoader.h"

namespace RpSkydomeInternal {
#include "__skydome_mesh.inl"
}

void RpSkydome::Setup(RpBuilder &builder, const DrawList &list,
                      const ViewState *view_state, const int orphan_index,
                      const char shared_data_buf[], const char color_tex[],
                      const char spec_tex[], const char depth_tex[]) {
    orphan_index_ = orphan_index;
    view_state_ = view_state;

    env_ = &list.env;
    draw_cam_pos_ = list.draw_cam.world_position();

    shared_data_buf_ = builder.ReadBuffer(shared_data_buf, *this);

    { // Main color
        Ren::Tex2DParams params;
        params.w = view_state->scr_res[0];
        params.h = view_state->scr_res[1];
#if (REN_OIT_MODE == REN_OIT_WEIGHTED_BLENDED) ||                                        \
    (REN_OIT_MODE == REN_OIT_MOMENT_BASED && REN_OIT_MOMENT_RENORMALIZE)
        // renormalization requires buffer with alpha channel
        params.format = Ren::eTexFormat::RawRGBA16F;
#else
        params.format = Ren::eTexFormat::RawRG11F_B10F;
#endif
        params.sampling.repeat = Ren::eTexRepeat::ClampToEdge;
        params.samples = view_state->is_multisampled ? 4 : 1;

        color_tex_ = builder.WriteTexture(color_tex, params, *this);
    }
    { // 4-component specular (alpha is roughness)
        Ren::Tex2DParams params;
        params.w = view_state->scr_res[0];
        params.h = view_state->scr_res[1];
        params.format = Ren::eTexFormat::RawRGBA8888;
        params.sampling.repeat = Ren::eTexRepeat::ClampToEdge;
        params.samples = view_state->is_multisampled ? 4 : 1;

        spec_tex_ = builder.WriteTexture(spec_tex, params, *this);
    }
    { // 24-bit depth
        Ren::Tex2DParams params;
        params.w = view_state->scr_res[0];
        params.h = view_state->scr_res[1];
        params.format = Ren::eTexFormat::Depth24Stencil8;
        params.sampling.repeat = Ren::eTexRepeat::ClampToEdge;
        params.samples = view_state->is_multisampled ? 4 : 1;

        depth_tex_ = builder.WriteTexture(depth_tex, params, *this);
    }
}

void RpSkydome::Execute(RpBuilder &builder) {
    RpAllocTex &color_tex = builder.GetWriteTexture(color_tex_);
    RpAllocTex &spec_tex = builder.GetWriteTexture(spec_tex_);
    RpAllocTex &depth_tex = builder.GetWriteTexture(depth_tex_);

    LazyInit(builder.ctx(), builder.sh(), color_tex, spec_tex, depth_tex);
    DrawSkydome(builder);
}

void RpSkydome::LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocTex &color_tex,
                         RpAllocTex &spec_tex, RpAllocTex &depth_tex) {
    using namespace RpSkydomeInternal;

    if (!initialized) {
        skydome_prog_ = sh.LoadProgram(ctx, "skydome", "internal/skydome.vert.glsl",
                                       "internal/skydome.frag.glsl");
        assert(skydome_prog_->ready());

        Ren::eMeshLoadStatus status;
        skydome_mesh_ =
            ctx.LoadMesh("__skydome", __skydome_positions, __skydome_vertices_count,
                         __skydome_indices, __skydome_indices_count, &status);
        assert(status == Ren::eMeshLoadStatus::CreatedFromData);

        initialized = true;
    }

    const int buf1_stride = 16, buf2_stride = 16;

    const Ren::VtxAttribDesc attribs[] = {
        {skydome_mesh_->attribs_buf1_handle(), REN_VTX_POS_LOC, 3, Ren::eType::Float32,
         buf1_stride, uintptr_t(skydome_mesh_->attribs_buf1().offset)}};
    if (!skydome_vao_.Setup(attribs, 1, skydome_mesh_->indices_buf_handle())) {
        ctx.log()->Error("RpSkydome: vao init failed!");
    }

    const Ren::TexHandle color_attachments[] = {
        color_tex.ref->handle(), {}, spec_tex.ref->handle()};
    if (!cached_fb_.Setup(color_attachments, 3, depth_tex.ref->handle(),
                          depth_tex.ref->handle(), view_state_->is_multisampled)) {
        ctx.log()->Error("RpSkydome: fbo init failed!");
    }
}

RpSkydome::~RpSkydome() = default;