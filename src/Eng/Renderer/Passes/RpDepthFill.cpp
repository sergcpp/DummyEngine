#include "RpDepthFill.h"

#include <Ren/Context.h>

#include "../../Utils/ShaderLoader.h"
#include "../Renderer_Structs.h"

void RpDepthFill::Setup(RpBuilder &builder, const DrawList &list,
                        const ViewState *view_state, int orphan_index,
                        const char instances_buf[], const char shared_data_buf[],
                        const char main_depth_tex[], const char main_velocity_tex[]) {
    orphan_index_ = orphan_index;
    view_state_ = view_state;

    render_flags_ = list.render_flags;
    zfill_batch_indices = list.zfill_batch_indices;
    zfill_batches = list.zfill_batches;

    instances_buf_ = builder.ReadBuffer(instances_buf, *this);
    shared_data_buf_ = builder.ReadBuffer(shared_data_buf, *this);

    { // 24-bit depth
        Ren::Tex2DParams params;
        params.w = view_state->scr_res[0];
        params.h = view_state->scr_res[1];
        params.format = Ren::eTexFormat::Depth24Stencil8;
        params.filter = Ren::eTexFilter::NoFilter;
        params.repeat = Ren::eTexRepeat::ClampToEdge;
        params.samples = view_state->is_multisampled ? 4 : 1;

        depth_tex_ = builder.WriteTexture(main_depth_tex, params, *this);
    }
    { // Texture that holds 2D velocity
        Ren::Tex2DParams params;
        params.w = view_state->scr_res[0];
        params.h = view_state->scr_res[1];
        params.format = Ren::eTexFormat::RawRG16;
        params.filter = Ren::eTexFilter::NoFilter;
        params.repeat = Ren::eTexRepeat::ClampToEdge;
        params.samples = view_state->is_multisampled ? 4 : 1;

        velocity_tex_ = builder.WriteTexture(main_velocity_tex, params, *this);
    }
}

void RpDepthFill::Execute(RpBuilder &builder) {
    RpAllocTex &depth_tex = builder.GetWriteTexture(depth_tex_);
    RpAllocTex &velocity_tex = builder.GetWriteTexture(velocity_tex_);

    LazyInit(builder.ctx(), builder.sh(), depth_tex, velocity_tex);
    DrawDepth(builder);
}

void RpDepthFill::LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocTex &depth_tex,
                           RpAllocTex &velocity_tex) {
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
            Ren::Tex2DParams p;
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

    if (!depth_fill_fb_.Setup(nullptr, 0, depth_tex.ref->handle(),
                              depth_tex.ref->handle(), view_state_->is_multisampled)) {
        ctx.log()->Error("RpDepthFill: depth_fill_fb_ init failed!");
    }

    if (!depth_fill_vel_fb_.Setup(velocity_tex.ref->handle(), depth_tex.ref->handle(),
                                  depth_tex.ref->handle(),
                                  view_state_->is_multisampled)) {
        ctx.log()->Error("RpDepthFill: depth_fill_vel_fb_ init failed!");
    }
}