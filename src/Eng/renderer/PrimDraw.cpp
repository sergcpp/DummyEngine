#include "PrimDraw.h"

#include <Ren/Context.h>

#include "Renderer_Structs.h"

namespace PrimDrawInternal {
#if defined(USE_VK_RENDER)
extern const float fs_quad_positions[] = {-1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f};
extern const float fs_quad_norm_uvs[] = {0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f};
#else
extern const float fs_quad_positions[] = {-1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f};
extern const float fs_quad_norm_uvs[] = {0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f};
#endif
extern const uint16_t fs_quad_indices[] = {0, 1, 2, 0, 2, 3};
const int TempBufSize = 256;
#include "precomputed/__skydome_mesh.inl"
#include "precomputed/__sphere_mesh.inl"

extern const int SphereIndicesCount = __sphere_indices_count;

// aligned to vertex stride
const size_t SphereVerticesSize = sizeof(__sphere_positions) + (16 - sizeof(__sphere_positions) % 16);
} // namespace PrimDrawInternal

bool Eng::PrimDraw::LazyInit(Ren::Context &ctx) {
    using namespace PrimDrawInternal;

    Ren::BufferRef vtx_buf1 = ctx.default_vertex_buf1(), vtx_buf2 = ctx.default_vertex_buf2(),
                   ndx_buf = ctx.default_indices_buf();

    if (!initialized_) {
        { // Allocate quad vertices
            uint32_t mem_required = sizeof(fs_quad_positions) + sizeof(fs_quad_norm_uvs);
            mem_required += (16 - mem_required % 16); // align to vertex stride

            Ren::Buffer temp_stage_buf("Temp prim buf", ctx.api_ctx(), Ren::eBufType::Stage, sizeof(__sphere_indices),
                                       192);

            { // copy quad vertices
                uint8_t *mapped_ptr = temp_stage_buf.Map(Ren::eBufMap::Write);
                memcpy(mapped_ptr, fs_quad_positions, sizeof(fs_quad_positions));
                memcpy(mapped_ptr + sizeof(fs_quad_positions), fs_quad_norm_uvs, sizeof(fs_quad_norm_uvs));
                temp_stage_buf.FlushMappedRange(
                    0, temp_stage_buf.AlignMapOffset(sizeof(fs_quad_positions) + sizeof(fs_quad_norm_uvs)));
                temp_stage_buf.Unmap();
            }

            quad_vtx1_ = vtx_buf1->AllocSubRegion(mem_required, "quad", &temp_stage_buf, ctx.current_cmd_buf());
            quad_vtx2_ = vtx_buf2->AllocSubRegion(mem_required, "quad", nullptr);
            assert(quad_vtx1_.offset == quad_vtx2_.offset && "Offsets do not match!");
        }

        { // Allocate quad indices
            Ren::Buffer temp_stage_buf("Temp prim buf", ctx.api_ctx(), Ren::eBufType::Stage, 6 * sizeof(uint16_t), 192);

            { // copy quad indices
                uint8_t *mapped_ptr = temp_stage_buf.Map(Ren::eBufMap::Write);
                memcpy(mapped_ptr, fs_quad_indices, 6 * sizeof(uint16_t));
                temp_stage_buf.FlushMappedRange(0, temp_stage_buf.AlignMapOffset(6 * sizeof(uint16_t)));
                temp_stage_buf.Unmap();
            }

            quad_ndx_ = ndx_buf->AllocSubRegion(6 * sizeof(uint16_t), "quad", &temp_stage_buf, ctx.current_cmd_buf());
        }

        { // Allocate sphere positions
            Ren::Buffer temp_stage_buf("Temp prim buf", ctx.api_ctx(), Ren::eBufType::Stage, SphereVerticesSize, 192);

            { // copy sphere positions
                uint8_t *mapped_ptr = temp_stage_buf.Map(Ren::eBufMap::Write);
                memcpy(mapped_ptr, __sphere_positions, sizeof(__sphere_positions));
                temp_stage_buf.FlushMappedRange(0, temp_stage_buf.AlignMapOffset(SphereVerticesSize));
                temp_stage_buf.Unmap();
            }

            // Allocate sphere vertices
            sphere_vtx1_ =
                vtx_buf1->AllocSubRegion(SphereVerticesSize, "sphere", &temp_stage_buf, ctx.current_cmd_buf());
            sphere_vtx2_ = vtx_buf2->AllocSubRegion(SphereVerticesSize, "sphere", nullptr);
            assert(sphere_vtx1_.offset == sphere_vtx2_.offset && "Offsets do not match!");
        }

        { // Allocate sphere indices
            Ren::Buffer temp_stage_buf("Temp prim buf", ctx.api_ctx(), Ren::eBufType::Stage, sizeof(__sphere_indices),
                                       192);

            { // copy sphere indices
                uint8_t *mapped_ptr = temp_stage_buf.Map(Ren::eBufMap::Write);
                memcpy(mapped_ptr, __sphere_indices, sizeof(__sphere_indices));
                temp_stage_buf.FlushMappedRange(0, temp_stage_buf.AlignMapOffset(sizeof(__sphere_indices)));
                temp_stage_buf.Unmap();
            }
            sphere_ndx_ =
                ndx_buf->AllocSubRegion(sizeof(__sphere_indices), "sphere", &temp_stage_buf, ctx.current_cmd_buf());

            // Allocate temporary buffer
            temp_vtx1_ = vtx_buf1->AllocSubRegion(TempBufSize, "temp");
            temp_vtx2_ = vtx_buf2->AllocSubRegion(TempBufSize, "temp");
            assert(temp_vtx1_.offset == temp_vtx1_.offset && "Offsets do not match!");
            temp_ndx_ = ndx_buf->AllocSubRegion(TempBufSize, "temp");
        }

        { // Load skydome mesh
            Ren::eMeshLoadStatus status;
            skydome_mesh_ =
                ctx.LoadMesh("__skydome", __skydome_positions, __skydome_vertices_count, __skydome_indices,
                             __skydome_indices_count, ctx.default_stage_bufs(), vtx_buf1, vtx_buf2, ndx_buf, &status);
            assert(status == Ren::eMeshLoadStatus::CreatedFromData);
        }

        ctx_ = &ctx;
        initialized_ = true;
    }

    { // setup quad vertices
        const Ren::VtxAttribDesc attribs[] = {
            {vtx_buf1->handle(), VTX_POS_LOC, 2, Ren::eType::Float32, 0, quad_vtx1_.offset},
            {vtx_buf1->handle(), VTX_UV1_LOC, 2, Ren::eType::Float32, 0,
             uint32_t(quad_vtx1_.offset + 8 * sizeof(float))}};

        fs_quad_vtx_input_.Setup(attribs, ndx_buf);
    }

    { // setup sphere vertices
        const Ren::VtxAttribDesc attribs[] = {
            {vtx_buf1->handle(), VTX_POS_LOC, 3, Ren::eType::Float32, 0, sphere_vtx1_.offset}};
        sphere_vtx_input_.Setup(attribs, ndx_buf);
    }

    return true;
}

void Eng::PrimDraw::CleanUp() {
    using namespace PrimDrawInternal;

    if (quad_vtx1_.offset != 0xffffffff) {
        Ren::BufferRef vtx_buf1 = ctx_->default_vertex_buf1(), vtx_buf2 = ctx_->default_vertex_buf2(),
                       ndx_buf = ctx_->default_indices_buf();

        vtx_buf1->FreeSubRegion(quad_vtx1_);
        assert(quad_vtx2_.offset != 0xffffffff);
        vtx_buf2->FreeSubRegion(quad_vtx2_);
        assert(quad_ndx_.offset != 0xffffffff);
        ndx_buf->FreeSubRegion(quad_ndx_);
    }

    if (sphere_vtx1_.offset != 0xffffffff) {
        Ren::BufferRef vtx_buf1 = ctx_->default_vertex_buf1(), vtx_buf2 = ctx_->default_vertex_buf2(),
                       ndx_buf = ctx_->default_indices_buf();

        vtx_buf1->FreeSubRegion(sphere_vtx1_);
        assert(sphere_vtx2_.offset != 0xffffffff);
        vtx_buf2->FreeSubRegion(sphere_vtx2_);
        assert(sphere_ndx_.offset != 0xffffffff);
        ndx_buf->FreeSubRegion(sphere_ndx_);
    }

    if (temp_vtx1_.offset != 0xffffffff) {
        Ren::BufferRef vtx_buf1 = ctx_->default_vertex_buf1(), vtx_buf2 = ctx_->default_vertex_buf2(),
                       ndx_buf = ctx_->default_indices_buf();

        vtx_buf1->FreeSubRegion(temp_vtx1_);
        assert(temp_vtx2_.offset != 0xffffffff);
        vtx_buf2->FreeSubRegion(temp_vtx2_);
        assert(temp_ndx_.offset != 0xffffffff);
        ndx_buf->FreeSubRegion(temp_ndx_);
    }
}

const Ren::Framebuffer *Eng::PrimDraw::FindOrCreateFramebuffer(const Ren::RenderPass *rp,
                                                               Ren::Span<const Ren::RenderTarget> color_targets,
                                                               Ren::RenderTarget depth_target,
                                                               Ren::RenderTarget stencil_target) {
    int w = -1, h = -1;

    Ren::SmallVector<Ren::WeakTex2DRef, 4> color_refs;
    for (const auto &rt : color_targets) {
        color_refs.push_back(rt.ref);
        if (w == -1 && rt.ref) {
            w = rt.ref->params.w;
            h = rt.ref->params.h;
        }
    }

    Ren::WeakTex2DRef depth_ref = depth_target.ref, stencil_ref = stencil_target.ref;

    if (w == -1 && depth_ref) {
        w = depth_ref->params.w;
        h = depth_ref->params.h;
    }

    if (w == -1 && stencil_ref) {
        w = stencil_ref->params.w;
        h = stencil_ref->params.h;
    }

    for (size_t i = 0; i < framebuffers_.size(); ++i) {
        if (!framebuffers_[i].Changed(*rp, depth_ref, stencil_ref, color_refs)) {
            return &framebuffers_[i];
        }
    }

    Ren::ApiContext *api_ctx = ctx_->api_ctx();
    Ren::Framebuffer &new_framebuffer = framebuffers_.emplace_back();

    if (!new_framebuffer.Setup(api_ctx, *rp, w, h, depth_target, stencil_target, color_targets, ctx_->log())) {
        ctx_->log()->Error("Failed to create framebuffer!");
        framebuffers_.pop_back();
        return nullptr;
    }

    return &new_framebuffer;
}