#include "RpShadowMaps.h"

#include <Ren/Context.h>

#include "../Renderer_Structs.h"
#include "../../Utils/ShaderLoader.h"

void RpShadowMaps::Setup(RpBuilder &builder, const DrawList &list,
                         const int orphan_index, const char instances_buf[],
                         const char shadowmap_tex[]) {
    orphan_index_ = orphan_index;

    shadow_batches_ = list.shadow_batches;
    shadow_batch_indices_ = list.shadow_batch_indices;
    shadow_lists_ = list.shadow_lists;
    shadow_regions_ = list.shadow_regions;

    instances_buf_ = builder.ReadBuffer(instances_buf, *this);

    { // shadow map buffer
        Ren::Tex2DParams params;
        params.w = w_;
        params.h = h_;
        params.format = Ren::eTexFormat::Depth16;
        params.filter = Ren::eTexFilter::BilinearNoMipmap;
        params.repeat = Ren::eTexRepeat::ClampToEdge;
        params.compare = Ren::eTexCompare::LEqual;

        shadowmap_tex_ = builder.WriteTexture(shadowmap_tex, params, *this);
    }
}

void RpShadowMaps::Execute(RpBuilder &builder) {
    RpAllocTex &shadowmap_tex = builder.GetWriteTexture(shadowmap_tex_);

    LazyInit(builder.ctx(), builder.sh(), shadowmap_tex.ref->handle());
    DrawShadowMaps(builder);
}

void RpShadowMaps::LazyInit(Ren::Context &ctx, ShaderLoader &sh,
                            Ren::TexHandle shadow_tex) {
    if (!initialized) {
        shadow_solid_prog_ =
            sh.LoadProgram(ctx, "shadow_solid", "internal/shadow.vert.glsl",
                           "internal/shadow.frag.glsl");
        assert(shadow_solid_prog_->ready());
        shadow_vege_solid_prog_ =
            sh.LoadProgram(ctx, "shadow_vege_solid", "internal/shadow_vege.vert.glsl",
                           "internal/shadow.frag.glsl");
        assert(shadow_vege_solid_prog_->ready());
        shadow_transp_prog_ = sh.LoadProgram(
            ctx, "shadow_transp", "internal/shadow.vert.glsl@TRANSPARENT_PERM",
            "internal/shadow.frag.glsl@TRANSPARENT_PERM");
        assert(shadow_transp_prog_->ready());
        shadow_vege_transp_prog_ = sh.LoadProgram(
            ctx, "shadow_vege_transp", "internal/shadow_vege.vert.glsl@TRANSPARENT_PERM",
            "internal/shadow.frag.glsl@TRANSPARENT_PERM");
        assert(shadow_vege_transp_prog_->ready());
        initialized = true;
    }

    if (!shadow_fb_.Setup(nullptr, 0, shadow_tex, {}, false)) {
        ctx.log()->Error("RpShadowMaps: shadow_fb_ init failed!");
    }

    const int buf1_stride = 16, buf2_stride = 16;

    { // VAO for solid shadow pass (uses position attribute only)
        const Ren::VtxAttribDesc attribs[] = {{ctx.default_vertex_buf1()->handle(),
                                               REN_VTX_POS_LOC, 3, Ren::eType::Float32,
                                               buf1_stride, 0}};
        if (!depth_pass_solid_vao_.Setup(attribs, 1,
                                         ctx.default_indices_buf()->handle())) {
            ctx.log()->Error("RpShadowMaps: depth_pass_solid_vao_ init failed!");
        }
    }

    { // VAO for solid shadow pass of vegetation (uses position and color attributes)
        const Ren::VtxAttribDesc attribs[] = {
            {ctx.default_vertex_buf1()->handle(), REN_VTX_POS_LOC, 3, Ren::eType::Float32,
             buf1_stride, 0},
            {ctx.default_vertex_buf2()->handle(), REN_VTX_AUX_LOC, 1, Ren::eType::Uint32,
             buf2_stride, uintptr_t(6 * sizeof(uint16_t))}};
        if (!depth_pass_vege_solid_vao_.Setup(attribs, 2,
                                              ctx.default_indices_buf()->handle())) {
            ctx.log()->Error("RpShadowMaps: depth_pass_solid_vao_ init failed!");
        }
    }

    { // VAO for alpha-tested shadow pass (uses position and uv attributes)
        const Ren::VtxAttribDesc attribs[] = {
            {ctx.default_vertex_buf1()->handle(), REN_VTX_POS_LOC, 3, Ren::eType::Float32,
             buf1_stride, 0},
            {ctx.default_vertex_buf1()->handle(), REN_VTX_UV1_LOC, 2, Ren::eType::Float16,
             buf1_stride, uintptr_t(3 * sizeof(float))}};
        if (!depth_pass_transp_vao_.Setup(attribs, 2,
                                          ctx.default_indices_buf()->handle())) {
            ctx.log()->Error("RpShadowMaps: depth_pass_transp_vao_ init failed!");
        }
    }

    { // VAO for solid shadow pass of vegetation (uses position and
      // color attributes)
        const Ren::VtxAttribDesc attribs[] = {
            {ctx.default_vertex_buf1()->handle(), REN_VTX_POS_LOC, 3, Ren::eType::Float32,
             buf1_stride, 0},
            {ctx.default_vertex_buf2()->handle(), REN_VTX_AUX_LOC, 1, Ren::eType::Uint32,
             buf2_stride, uintptr_t(6 * sizeof(uint16_t))}};
        if (!depth_pass_vege_solid_vao_.Setup(attribs, 2,
                                              ctx.default_indices_buf()->handle())) {
            ctx.log()->Error("RpShadowMaps: depth_pass_vege_solid_vao_ init failed!");
        }
    }

    { // VAO for transparent shadow pass of vegetation (uses position, color and
      // uv attributes)
        const Ren::VtxAttribDesc attribs[] = {
            {ctx.default_vertex_buf1()->handle(), REN_VTX_POS_LOC, 3, Ren::eType::Float32,
             buf1_stride, 0},
            {ctx.default_vertex_buf1()->handle(), REN_VTX_UV1_LOC, 2, Ren::eType::Float16,
             buf1_stride, uintptr_t(3 * sizeof(float))},
            {ctx.default_vertex_buf2()->handle(), REN_VTX_AUX_LOC, 1, Ren::eType::Uint32,
             buf2_stride, uintptr_t(6 * sizeof(uint16_t))}};
        if (!depth_pass_vege_transp_vao_.Setup(attribs, 3,
                                               ctx.default_indices_buf()->handle())) {
            ctx.log()->Error("RpShadowMaps: depth_pass_vege_transp_vao_ init failed!");
        }
    }
}
