#include "PrimDraw.h"

#include <Ren/ApiContext.h>
#include <Ren/Context.h>

#include "Renderer_Structs.h"

namespace PrimDrawInternal {
extern const float fs_quad_positions[] = {-1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f};
#if defined(REN_VK_BACKEND)
extern const float fs_quad_norm_uvs[] = {0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f};
#else
extern const float fs_quad_norm_uvs[] = {0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f};
#endif
extern const uint16_t fs_quad_indices[] = {0, 1, 2, 0, 2, 3};
const int TempBufSize = 256;
#include "precomputed/__sphere_mesh.inl"

extern const int SphereIndicesCount = __sphere_indices_count;

// aligned to vertex stride
const size_t SphereVerticesSize = sizeof(__sphere_positions) + (16 - sizeof(__sphere_positions) % 16);

bool framebuffer_eq(const Ren::Framebuffer &fb, const Ren::RenderPass &rp, const Ren::WeakImgRef &depth_attachment,
                    const Ren::WeakImgRef &stencil_attachment,
                    const Ren::Span<const Ren::RenderTarget> color_attachments) {
    return !fb.Changed(rp, depth_attachment, stencil_attachment, color_attachments);
}
bool framebuffer_lt(const Ren::Framebuffer &fb, const Ren::RenderPass &rp, const Ren::WeakImgRef &depth_attachment,
                    const Ren::WeakImgRef &stencil_attachment,
                    const Ren::Span<const Ren::RenderTarget> color_attachments) {
    return fb.LessThan(rp, depth_attachment, stencil_attachment, color_attachments);
}
} // namespace PrimDrawInternal

bool Eng::PrimDraw::LazyInit(Ren::Context &ctx) {
    using namespace PrimDrawInternal;

    Ren::BufRef vtx_buf1 = ctx.default_vertex_buf1(), vtx_buf2 = ctx.default_vertex_buf2(),
                ndx_buf = ctx.default_indices_buf();

    if (!initialized_) {
        Ren::CommandBuffer cmd_buf = ctx.api_ctx()->BegSingleTimeCommands();
        { // Allocate quad vertices
            uint32_t mem_required = sizeof(fs_quad_positions) + sizeof(fs_quad_norm_uvs);
            mem_required += (16 - mem_required % 16); // align to vertex stride

            Ren::Buffer temp_stage_buf("Temp prim buf", ctx.api_ctx(), Ren::eBufType::Upload, sizeof(__sphere_indices));

            { // copy quad vertices
                uint8_t *mapped_ptr = temp_stage_buf.Map();
                memcpy(mapped_ptr, fs_quad_positions, sizeof(fs_quad_positions));
                memcpy(mapped_ptr + sizeof(fs_quad_positions), fs_quad_norm_uvs, sizeof(fs_quad_norm_uvs));
                temp_stage_buf.Unmap();
            }

            quad_vtx1_ = vtx_buf1->AllocSubRegion(mem_required, 16, "quad", &temp_stage_buf, cmd_buf);
            quad_vtx2_ = vtx_buf2->AllocSubRegion(mem_required, 16, "quad", nullptr);
            assert(quad_vtx1_.offset == quad_vtx2_.offset && "Offsets do not match!");
        }
        { // Allocate quad indices
            Ren::Buffer temp_stage_buf("Temp prim buf", ctx.api_ctx(), Ren::eBufType::Upload, 6 * sizeof(uint16_t));

            { // copy quad indices
                uint8_t *mapped_ptr = temp_stage_buf.Map();
                memcpy(mapped_ptr, fs_quad_indices, 6 * sizeof(uint16_t));
                temp_stage_buf.Unmap();
            }

            quad_ndx_ = ndx_buf->AllocSubRegion(6 * sizeof(uint16_t), 4, "quad", &temp_stage_buf, cmd_buf);
        }
        { // Allocate sphere positions
            Ren::Buffer temp_stage_buf("Temp prim buf", ctx.api_ctx(), Ren::eBufType::Upload, SphereVerticesSize);

            { // copy sphere positions
                uint8_t *mapped_ptr = temp_stage_buf.Map();
                memcpy(mapped_ptr, __sphere_positions, sizeof(__sphere_positions));
                temp_stage_buf.Unmap();
            }

            // Allocate sphere vertices
            sphere_vtx1_ = vtx_buf1->AllocSubRegion(SphereVerticesSize, 16, "sphere", &temp_stage_buf, cmd_buf);
            sphere_vtx2_ = vtx_buf2->AllocSubRegion(SphereVerticesSize, 16, "sphere", nullptr);
            assert(sphere_vtx1_.offset == sphere_vtx2_.offset && "Offsets do not match!");
        }
        { // Allocate sphere indices
            Ren::Buffer temp_stage_buf("Temp prim buf", ctx.api_ctx(), Ren::eBufType::Upload, sizeof(__sphere_indices));

            { // copy sphere indices
                uint8_t *mapped_ptr = temp_stage_buf.Map();
                memcpy(mapped_ptr, __sphere_indices, sizeof(__sphere_indices));
                temp_stage_buf.Unmap();
            }
            sphere_ndx_ = ndx_buf->AllocSubRegion(sizeof(__sphere_indices), 4, "sphere", &temp_stage_buf, cmd_buf);

            // Allocate temporary buffer
            temp_vtx1_ = vtx_buf1->AllocSubRegion(TempBufSize, 16, "temp");
            temp_vtx2_ = vtx_buf2->AllocSubRegion(TempBufSize, 16, "temp");
            assert(temp_vtx1_.offset == temp_vtx1_.offset && "Offsets do not match!");
            temp_ndx_ = ndx_buf->AllocSubRegion(TempBufSize, 4, "temp");
        }
        ctx.api_ctx()->EndSingleTimeCommands(cmd_buf);

        ctx_ = &ctx;
        initialized_ = true;
    }

    { // setup quad vertices
        const Ren::VtxAttribDesc attribs[] = {
            {vtx_buf1, VTX_POS_LOC, 2, Ren::eType::Float32, 0, quad_vtx1_.offset},
            {vtx_buf1, VTX_UV1_LOC, 2, Ren::eType::Float32, 0, uint32_t(quad_vtx1_.offset + 8 * sizeof(float))}};
        fs_quad_vtx_input_ = ctx_->LoadVertexInput(attribs, ndx_buf);
    }

    { // setup sphere vertices
        const Ren::VtxAttribDesc attribs[] = {{vtx_buf1, VTX_POS_LOC, 3, Ren::eType::Float32, 0, sphere_vtx1_.offset}};
        sphere_vtx_input_ = ctx_->LoadVertexInput(attribs, ndx_buf);
    }

    return true;
}

void Eng::PrimDraw::CleanUp() {
    using namespace PrimDrawInternal;

    if (quad_vtx1_.offset != 0xffffffff) {
        Ren::BufRef vtx_buf1 = ctx_->default_vertex_buf1(), vtx_buf2 = ctx_->default_vertex_buf2(),
                    ndx_buf = ctx_->default_indices_buf();

        vtx_buf1->FreeSubRegion(quad_vtx1_);
        assert(quad_vtx2_.offset != 0xffffffff);
        vtx_buf2->FreeSubRegion(quad_vtx2_);
        assert(quad_ndx_.offset != 0xffffffff);
        ndx_buf->FreeSubRegion(quad_ndx_);
    }

    if (sphere_vtx1_.offset != 0xffffffff) {
        Ren::BufRef vtx_buf1 = ctx_->default_vertex_buf1(), vtx_buf2 = ctx_->default_vertex_buf2(),
                    ndx_buf = ctx_->default_indices_buf();

        vtx_buf1->FreeSubRegion(sphere_vtx1_);
        assert(sphere_vtx2_.offset != 0xffffffff);
        vtx_buf2->FreeSubRegion(sphere_vtx2_);
        assert(sphere_ndx_.offset != 0xffffffff);
        ndx_buf->FreeSubRegion(sphere_ndx_);
    }

    if (temp_vtx1_.offset != 0xffffffff) {
        Ren::BufRef vtx_buf1 = ctx_->default_vertex_buf1(), vtx_buf2 = ctx_->default_vertex_buf2(),
                    ndx_buf = ctx_->default_indices_buf();

        vtx_buf1->FreeSubRegion(temp_vtx1_);
        assert(temp_vtx2_.offset != 0xffffffff);
        vtx_buf2->FreeSubRegion(temp_vtx2_);
        assert(temp_ndx_.offset != 0xffffffff);
        ndx_buf->FreeSubRegion(temp_ndx_);
    }
}

const Ren::Framebuffer *Eng::PrimDraw::FindOrCreateFramebuffer(const Ren::RenderPass *rp,
                                                               const Ren::RenderTarget depth_target,
                                                               const Ren::RenderTarget stencil_target,
                                                               Ren::Span<const Ren::RenderTarget> color_targets) {
    using namespace PrimDrawInternal;

    Ren::WeakImgRef depth_ref = depth_target.ref, stencil_ref = stencil_target.ref;

    int start = 0, count = int(framebuffers_.size());
    while (count > 0) {
        const int step = count / 2;
        const int index = start + step;
        if (framebuffer_lt(framebuffers_[index], *rp, depth_ref, stencil_ref, color_targets)) {
            start = index + 1;
            count -= step + 1;
        } else {
            count = step;
        }
    }

    if (start < int(framebuffers_.size()) &&
        framebuffer_eq(framebuffers_[start], *rp, depth_ref, stencil_ref, color_targets)) {
        return &framebuffers_[start];
    }

    Ren::ApiContext *api_ctx = ctx_->api_ctx();

    int w = -1, h = -1;

    for (const auto &rt : color_targets) {
        if (rt.ref) {
            w = rt.ref->params.w;
            h = rt.ref->params.h;
            break;
        }
    }

    if (w == -1 && depth_ref) {
        w = depth_ref->params.w;
        h = depth_ref->params.h;
    }

    if (w == -1 && stencil_ref) {
        w = stencil_ref->params.w;
        h = stencil_ref->params.h;
    }

    Ren::Framebuffer new_framebuffer;
    if (!new_framebuffer.Setup(api_ctx, *rp, w, h, depth_target, stencil_target, color_targets, ctx_->log())) {
        ctx_->log()->Error("Failed to create framebuffer!");
        framebuffers_.pop_back();
        return nullptr;
    }
    framebuffers_.insert(begin(framebuffers_) + start, std::move(new_framebuffer));
    return &framebuffers_[start];
}
