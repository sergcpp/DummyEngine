#include "RpDepthFill.h"

#include "../../Utils/ShaderLoader.h"
#include "../Renderer_Structs.h"

void RpDepthFill::Setup(Graph::RpBuilder &builder, const DrawList &list,
                        const ViewState *view_state, int orphan_index,
                        Ren::TexHandle depth_tex, Ren::TexHandle velocity_tex,
                        Graph::ResourceHandle in_instances_buf,
                        Ren::Tex1DRef instances_tbo,
                        Graph::ResourceHandle in_shared_data_buf) {
    orphan_index_ = orphan_index;

    depth_tex_ = depth_tex;
    velocity_tex_ = velocity_tex;
    view_state_ = view_state;
    instances_tbo_ = std::move(instances_tbo);

    render_flags_ = list.render_flags;
    zfill_batch_indices = list.zfill_batch_indices;
    zfill_batches = list.zfill_batches;

    input_[0] = builder.ReadBuffer(in_instances_buf);
    input_[1] = builder.ReadBuffer(in_shared_data_buf);
    input_count_ = 2;

    // output_[0] = builder.WriteBuffer(input_[0], *this);
    output_count_ = 0;
}

void RpDepthFill::Execute(Graph::RpBuilder &builder) {
    LazyInit(builder.ctx(), builder.sh());
    DrawDepth(builder);
}

void RpDepthFill::LazyInit(Ren::Context &ctx, ShaderLoader &sh) {
    if (!initialized) {
        fillz_solid_prog_ = sh.LoadProgram(ctx, "fillz_solid", "internal/fillz.vert.glsl",
                                           "internal/fillz.frag.glsl");
        assert(fillz_solid_prog_->ready());
        fillz_solid_mov_prog_ =
            sh.LoadProgram(ctx, "fillz_solid_mov", "internal/fillz.vert.glsl@MOVING_PERM",
                           "internal/fillz.frag.glsl@OUTPUT_VELOCITY");
        assert(fillz_solid_mov_prog_->ready());
        fillz_vege_solid_prog_ =
            sh.LoadProgram(ctx, "fillz_vege_solid", "internal/fillz_vege.vert.glsl",
                           "internal/fillz.frag.glsl");
        assert(fillz_vege_solid_prog_->ready());
        fillz_vege_solid_vel_prog_ = sh.LoadProgram(
            ctx, "fillz_vege_solid_vel", "internal/fillz_vege.vert.glsl@OUTPUT_VELOCITY",
            "internal/fillz.frag.glsl@OUTPUT_VELOCITY");
        assert(fillz_vege_solid_vel_prog_->ready());
        fillz_vege_solid_vel_mov_prog_ =
            sh.LoadProgram(ctx, "fillz_vege_solid_vel_mov",
                           "internal/fillz_vege.vert.glsl@OUTPUT_VELOCITY;MOVING_PERM",
                           "internal/fillz.frag.glsl@OUTPUT_VELOCITY");
        assert(fillz_vege_solid_vel_mov_prog_->ready());
        fillz_transp_prog_ = sh.LoadProgram(ctx, "fillz_transp",
                                            "internal/fillz.vert.glsl@TRANSPARENT_PERM",
                                            "internal/fillz.frag.glsl@TRANSPARENT_PERM");
        assert(fillz_transp_prog_->ready());
        fillz_transp_mov_prog_ =
            sh.LoadProgram(ctx, "fillz_transp_mov",
                           "internal/fillz.vert.glsl@TRANSPARENT_PERM;MOVING_PERM",
                           "internal/fillz.frag.glsl@TRANSPARENT_PERM;OUTPUT_VELOCITY");
        assert(fillz_transp_mov_prog_->ready());
        fillz_vege_transp_prog_ = sh.LoadProgram(
            ctx, "fillz_vege_transp", "internal/fillz_vege.vert.glsl@TRANSPARENT_PERM",
            "internal/fillz.frag.glsl@TRANSPARENT_PERM");
        assert(fillz_vege_transp_prog_->ready());
        fillz_vege_transp_vel_prog_ = sh.LoadProgram(
            ctx, "fillz_vege_transp_vel",
            "internal/fillz_vege.vert.glsl@TRANSPARENT_PERM;OUTPUT_VELOCITY",
            "internal/fillz.frag.glsl@TRANSPARENT_PERM;OUTPUT_VELOCITY");
        assert(fillz_vege_transp_vel_prog_->ready());
        fillz_vege_transp_vel_mov_prog_ = sh.LoadProgram(
            ctx, "fillz_vege_transp_vel_mov",
            "internal/fillz_vege.vert.glsl@TRANSPARENT_PERM;OUTPUT_VELOCITY;MOVING_PERM",
            "internal/fillz.frag.glsl@TRANSPARENT_PERM;OUTPUT_VELOCITY");
        assert(fillz_vege_transp_vel_mov_prog_->ready());
        fillz_skin_solid_prog_ =
            sh.LoadProgram(ctx, "fillz_skin_solid", "internal/fillz_skin.vert.glsl",
                           "internal/fillz.frag.glsl");
        assert(fillz_skin_solid_prog_->ready());
        fillz_skin_solid_vel_prog_ = sh.LoadProgram(
            ctx, "fillz_skin_solid_vel", "internal/fillz_skin.vert.glsl@OUTPUT_VELOCITY",
            "internal/fillz.frag.glsl@OUTPUT_VELOCITY");
        assert(fillz_skin_solid_vel_prog_->ready());
        fillz_skin_solid_vel_mov_prog_ =
            sh.LoadProgram(ctx, "fillz_skin_solid_vel_mov",
                           "internal/fillz_skin.vert.glsl@OUTPUT_VELOCITY;MOVING_PERM",
                           "internal/fillz.frag.glsl@OUTPUT_VELOCITY");
        assert(fillz_skin_solid_vel_mov_prog_->ready());
        fillz_skin_transp_prog_ = sh.LoadProgram(
            ctx, "fillz_skin_transp", "internal/fillz_skin.vert.glsl@TRANSPARENT_PERM",
            "internal/fillz.frag.glsl@TRANSPARENT_PERM");
        assert(fillz_skin_transp_prog_->ready());
        fillz_skin_transp_vel_prog_ = sh.LoadProgram(
            ctx, "fillz_skin_transp_vel",
            "internal/fillz_skin.vert.glsl@TRANSPARENT_PERM;OUTPUT_VELOCITY",
            "internal/fillz.frag.glsl@TRANSPARENT_PERM;OUTPUT_VELOCITY");
        assert(fillz_skin_transp_vel_prog_->ready());
        fillz_skin_transp_vel_mov_prog_ = sh.LoadProgram(
            ctx, "fillz_skin_transp_vel_mov",
            "internal/fillz_skin.vert.glsl@TRANSPARENT_PERM;OUTPUT_VELOCITY;MOVING_PERM",
            "internal/fillz.frag.glsl@TRANSPARENT_PERM;OUTPUT_VELOCITY");
        assert(fillz_skin_transp_vel_mov_prog_->ready());

        { // dummy 1px texture
            Ren::Texture2DParams p;
            p.w = p.h = 1;
            p.format = Ren::eTexFormat::RawRGBA8888;
            p.filter = Ren::eTexFilter::Bilinear;
            p.repeat = Ren::eTexRepeat::ClampToEdge;

            static const uint8_t white[] = {255, 255, 255, 255};

            Ren::eTexLoadStatus status;
            dummy_white_ =
                ctx.LoadTexture2D("dummy_white", white, sizeof(white), p, &status);
            assert(status == Ren::eTexLoadStatus::TexCreatedFromData ||
                   status == Ren::eTexLoadStatus::TexFound);
        }

        initialized = true;
    }

    Ren::BufHandle vtx_buf1 = ctx.default_vertex_buf1()->handle(),
                   vtx_buf2 = ctx.default_vertex_buf2()->handle(),
                   ndx_buf = ctx.default_indices_buf()->handle();

    const int buf1_stride = 16, buf2_stride = 16;

    { // VAO for solid depth-fill pass (uses position attribute only)
        const Ren::VtxAttribDesc attribs[] = {
            {vtx_buf1, REN_VTX_POS_LOC, 3, Ren::eType::Float32, buf1_stride, 0}};
        if (!depth_pass_solid_vao_.Setup(attribs, 1, ndx_buf)) {
            ctx.log()->Error("RpDepthFill: depth_pass_solid_vao_ init failed!");
        }
    }

    { // VAO for solid depth-fill pass of vegetation (uses position and color attributes
      // only)
        const Ren::VtxAttribDesc attribs[] = {
            {vtx_buf1, REN_VTX_POS_LOC, 3, Ren::eType::Float32, buf1_stride, 0},
            {vtx_buf2, REN_VTX_AUX_LOC, 1, Ren::eType::Uint32, buf2_stride,
             uintptr_t(6 * sizeof(uint16_t))}};
        if (!depth_pass_vege_solid_vao_.Setup(attribs, 2, ndx_buf)) {
            ctx.log()->Error("RpDepthFill: depth_pass_vege_solid_vao_ init failed!");
        }
    }

    { // VAO for alpha-tested depth-fill pass (uses position and uv
      // attributes)
        const Ren::VtxAttribDesc attribs[] = {
            {vtx_buf1, REN_VTX_POS_LOC, 3, Ren::eType::Float32, buf1_stride, 0},
            {vtx_buf1, REN_VTX_UV1_LOC, 2, Ren::eType::Float16, buf1_stride,
             uintptr_t(3 * sizeof(float))}};
        if (!depth_pass_transp_vao_.Setup(attribs, 2, ndx_buf)) {
            ctx.log()->Error("RpDepthFill: depth_pass_transp_vao_ init failed!");
        }
    }

    { // VAO for alpha-tested depth-fill pass of vegetation (uses position, uvs and
      // color attributes)
        const Ren::VtxAttribDesc attribs[] = {
            {vtx_buf1, REN_VTX_POS_LOC, 3, Ren::eType::Float32, buf1_stride, 0},
            {vtx_buf1, REN_VTX_UV1_LOC, 2, Ren::eType::Float16, buf1_stride,
             uintptr_t(3 * sizeof(float))},
            {vtx_buf2, REN_VTX_AUX_LOC, 1, Ren::eType::Uint32, buf2_stride,
             uintptr_t(6 * sizeof(uint16_t))}};
        if (!depth_pass_vege_transp_vao_.Setup(attribs, 3, ndx_buf)) {
            ctx.log()->Error("RpDepthFill: depth_pass_vege_transp_vao_ init failed!");
        }
    }

    { // VAO for depth-fill pass of skinned solid meshes (with velocity output)
        const Ren::VtxAttribDesc attribs[] = {
            {vtx_buf1, REN_VTX_POS_LOC, 3, Ren::eType::Float32, buf1_stride, 0},
            {vtx_buf1, REN_VTX_PRE_LOC, 3, Ren::eType::Float32, buf1_stride,
             uintptr_t(REN_MAX_SKIN_VERTICES_TOTAL * 16)}};
        if (!depth_pass_skin_solid_vao_.Setup(attribs, 2, ndx_buf)) {
            ctx.log()->Error("RpDepthFill: depth_pass_skin_solid_vao_ init failed!");
        }
    }

    { // VAO for depth-fill pass of skinned transparent meshes (with velocity output)
        const Ren::VtxAttribDesc attribs[] = {
            {vtx_buf1, REN_VTX_POS_LOC, 3, Ren::eType::Float32, buf1_stride, 0},
            {vtx_buf1, REN_VTX_UV1_LOC, 2, Ren::eType::Float16, buf1_stride,
             uintptr_t(3 * sizeof(float))},
            {vtx_buf1, REN_VTX_PRE_LOC, 3, Ren::eType::Float32, buf1_stride,
             uintptr_t(REN_MAX_SKIN_VERTICES_TOTAL * 16)}};
        if (!depth_pass_skin_transp_vao_.Setup(attribs, 3, ndx_buf)) {
            ctx.log()->Error("RpDepthFill: depth_pass_skin_transp_vao_ init failed!");
        }
    }

    if (!depth_fill_fb_.Setup(nullptr, 0, depth_tex_, depth_tex_,
                              view_state_->is_multisampled)) {
        ctx.log()->Error("RpDepthFill: depth_fill_fb_ init failed!");
    }

    if (!depth_fill_vel_fb_.Setup(&velocity_tex_, 1, depth_tex_, depth_tex_,
                                  view_state_->is_multisampled)) {
        ctx.log()->Error("RpDepthFill: depth_fill_vel_fb_ init failed!");
    }
}