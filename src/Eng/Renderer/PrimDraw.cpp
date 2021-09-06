#include "PrimDraw.h"

#include <Ren/Context.h>

#include "Renderer_GL_Defines.inl"

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
#include "__skydome_mesh.inl"
#include "__sphere_mesh.inl"

extern const int SphereIndicesCount = __sphere_indices_count;
} // namespace PrimDrawInternal

bool PrimDraw::LazyInit(Ren::Context &ctx) {
    using namespace PrimDrawInternal;

    Ren::BufferRef vtx_buf1 = ctx.default_vertex_buf1(), vtx_buf2 = ctx.default_vertex_buf2(),
                   ndx_buf = ctx.default_indices_buf();

    if (!initialized_) {
        { // Allocate quad vertices
            uint32_t mem_required = sizeof(fs_quad_positions) + sizeof(fs_quad_norm_uvs);
            mem_required += (16 - mem_required % 16); // align to vertex stride

            Ren::Buffer temp_stage_buf("Temp prim buf", ctx.api_ctx(), Ren::eBufType::Stage, sizeof(__sphere_indices));

            { // copy quad vertices
                uint8_t *mapped_ptr = temp_stage_buf.Map(Ren::BufMapWrite);
                memcpy(mapped_ptr, fs_quad_positions, sizeof(fs_quad_positions));
                memcpy(mapped_ptr + sizeof(fs_quad_positions), fs_quad_norm_uvs, sizeof(fs_quad_norm_uvs));
                temp_stage_buf.FlushMappedRange(
                    0, temp_stage_buf.AlignMapOffset(sizeof(fs_quad_positions) + sizeof(fs_quad_norm_uvs)));
                temp_stage_buf.Unmap();
            }

            quad_vtx1_offset_ = vtx_buf1->AllocSubRegion(mem_required, "quad", &temp_stage_buf, ctx.current_cmd_buf());
            quad_vtx2_offset_ = vtx_buf2->AllocSubRegion(mem_required, "quad", nullptr);
            assert(quad_vtx1_offset_ == quad_vtx2_offset_ && "Offsets do not match!");
        }

        { // Allocate quad indices
            Ren::Buffer temp_stage_buf("Temp prim buf", ctx.api_ctx(), Ren::eBufType::Stage, 6 * sizeof(uint16_t));

            { // copy quad indices
                uint8_t *mapped_ptr = temp_stage_buf.Map(Ren::BufMapWrite);
                memcpy(mapped_ptr, fs_quad_indices, 6 * sizeof(uint16_t));
                temp_stage_buf.FlushMappedRange(0, temp_stage_buf.AlignMapOffset(6 * sizeof(uint16_t)));
                temp_stage_buf.Unmap();
            }

            quad_ndx_offset_ =
                ndx_buf->AllocSubRegion(6 * sizeof(uint16_t), "quad", &temp_stage_buf, ctx.current_cmd_buf());
        }

        { // Allocate sphere positions
            // aligned to vertex stride
            const size_t sphere_vertices_size = sizeof(__sphere_positions) + (16 - sizeof(__sphere_positions) % 16);

            Ren::Buffer temp_stage_buf("Temp prim buf", ctx.api_ctx(), Ren::eBufType::Stage, sphere_vertices_size);

            { // copy sphere positions
                uint8_t *mapped_ptr = temp_stage_buf.Map(Ren::BufMapWrite);
                memcpy(mapped_ptr, __sphere_positions, sphere_vertices_size);
                temp_stage_buf.FlushMappedRange(0, temp_stage_buf.AlignMapOffset(sphere_vertices_size));
                temp_stage_buf.Unmap();
            }

            // Allocate sphere vertices
            sphere_vtx1_offset_ =
                vtx_buf1->AllocSubRegion(sphere_vertices_size, "sphere", &temp_stage_buf, ctx.current_cmd_buf());
            sphere_vtx2_offset_ = vtx_buf2->AllocSubRegion(sphere_vertices_size, "sphere", nullptr);
            assert(sphere_vtx1_offset_ == sphere_vtx2_offset_ && "Offsets do not match!");
        }

        { // Allocate sphere indices
            Ren::Buffer temp_stage_buf("Temp prim buf", ctx.api_ctx(), Ren::eBufType::Stage, sizeof(__sphere_indices));

            { // copy sphere indices
                uint8_t *mapped_ptr = temp_stage_buf.Map(Ren::BufMapWrite);
                memcpy(mapped_ptr, __sphere_indices, sizeof(__sphere_indices));
                temp_stage_buf.FlushMappedRange(0, temp_stage_buf.AlignMapOffset(sizeof(__sphere_indices)));
                temp_stage_buf.Unmap();
            }
            sphere_ndx_offset_ =
                ndx_buf->AllocSubRegion(sizeof(__sphere_indices), "sphere", &temp_stage_buf, ctx.current_cmd_buf());

            // Allocate temporary buffer
            temp_buf1_vtx_offset_ = vtx_buf1->AllocSubRegion(TempBufSize, "temp");
            temp_buf2_vtx_offset_ = vtx_buf2->AllocSubRegion(TempBufSize, "temp");
            assert(temp_buf1_vtx_offset_ == temp_buf2_vtx_offset_ && "Offsets do not match!");
            temp_buf_ndx_offset_ = ndx_buf->AllocSubRegion(TempBufSize, "temp");
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
            {vtx_buf1->handle(), REN_VTX_POS_LOC, 2, Ren::eType::Float32, 0, quad_vtx1_offset_},
            {vtx_buf1->handle(), REN_VTX_UV1_LOC, 2, Ren::eType::Float32, 0,
             uint32_t(quad_vtx1_offset_ + 8 * sizeof(float))}};

        fs_quad_vtx_input_.Setup(attribs, 2, ndx_buf);
    }

    { // setup sphere vertices
        const Ren::VtxAttribDesc attribs[] = {
            {vtx_buf1->handle(), REN_VTX_POS_LOC, 3, Ren::eType::Float32, 0, sphere_vtx1_offset_}};
        sphere_vtx_input_.Setup(attribs, 1, ndx_buf);
    }

    return true;
}

void PrimDraw::CleanUp() {
    Ren::BufferRef vtx_buf1 = ctx_->default_vertex_buf1(), vtx_buf2 = ctx_->default_vertex_buf2(),
                   ndx_buf = ctx_->default_indices_buf();

    if (quad_vtx1_offset_ != 0xffffffff) {
        vtx_buf1->FreeSubRegion(quad_vtx1_offset_);
        assert(quad_vtx2_offset_ != 0xffffffff);
        vtx_buf2->FreeSubRegion(quad_vtx2_offset_);
        assert(quad_ndx_offset_ != 0xffffffff);
        ndx_buf->FreeSubRegion(quad_ndx_offset_);
    }

    if (sphere_vtx1_offset_ != 0xffffffff) {
        vtx_buf1->FreeSubRegion(sphere_vtx1_offset_);
        assert(sphere_vtx2_offset_ != 0xffffffff);
        vtx_buf2->FreeSubRegion(sphere_vtx2_offset_);
        assert(sphere_ndx_offset_ != 0xffffffff);
        ndx_buf->FreeSubRegion(sphere_ndx_offset_);
    }

    if (temp_buf1_vtx_offset_ != 0xffffffff) {
        vtx_buf1->FreeSubRegion(temp_buf1_vtx_offset_);
        assert(temp_buf2_vtx_offset_ != 0xffffffff);
        vtx_buf2->FreeSubRegion(temp_buf2_vtx_offset_);
        assert(temp_buf_ndx_offset_ != 0xffffffff);
        ndx_buf->FreeSubRegion(temp_buf_ndx_offset_);
    }
}
