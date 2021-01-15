#include "RpSkydome.h"

#include "../../Utils/ShaderLoader.h"
#include "../Renderer_Names.h"
#include "../Renderer_Structs.h"

namespace RpSkydomeInternal {
#include "__skydome_mesh.inl"
}

void RpSkydome::Setup(RpBuilder &builder, const DrawList &list,
                      const ViewState *view_state, const int orphan_index,
                      Ren::TexHandle color_tex, Ren::TexHandle spec_tex,
                      Ren::TexHandle depth_tex) {
    orphan_index_ = orphan_index;

    color_tex_ = color_tex;
    spec_tex_ = spec_tex;
    depth_tex_ = depth_tex;

    view_state_ = view_state;

    env_ = &list.env;
    draw_cam_pos_ = list.draw_cam.world_position();

    input_[0] = builder.ReadBuffer(SHARED_DATA_BUF);
    input_count_ = 1;

    // output_[0] = builder.WriteBuffer(input_[0], *this);
    output_count_ = 0;
}

void RpSkydome::Execute(RpBuilder &builder) {
    LazyInit(builder.ctx(), builder.sh());
    DrawSkydome(builder);
}

void RpSkydome::LazyInit(Ren::Context &ctx, ShaderLoader &sh) {
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

    const Ren::TexHandle color_attachments[] = {color_tex_, {}, spec_tex_};
    if (!cached_fb_.Setup(color_attachments, 3, depth_tex_, depth_tex_,
                          view_state_->is_multisampled)) {
        ctx.log()->Error("RpSkydome: fbo init failed!");
    }
}

RpSkydome::~RpSkydome() {
#if defined(USE_GL_RENDER)
#endif
}