#include "Renderer.h"

#include "Utils.h"

#include <cassert>

#include "../Ren/Context.h"
#include "../Ren/DebugMarker.h"
#include "../Ren/GL.h"
#include "../Ren/GLCtx.h"
#include "../Sys/Json.h"

namespace UIRendererConstants {
extern const int TexAtlasSlot;
} // namespace UIRendererConstants

Gui::Renderer::Renderer(Ren::Context &ctx) : ctx_(ctx) { instance_index_ = g_instance_count++; }

Gui::Renderer::~Renderer() {
    vertex_stage_buf_->Unmap();
    index_stage_buf_->Unmap();
}

void Gui::Renderer::Draw(const int w, const int h) {
    Ren::DebugMarker _(ctx_.api_ctx(), ctx_.current_cmd_buf(), name_);

#ifndef NDEBUG
    if (buf_range_fences_[ctx_.backend_frame()]) {
        const Ren::WaitResult res = buf_range_fences_[ctx_.backend_frame()].ClientWaitSync(0);
        if (res != Ren::WaitResult::Success) {
            ctx_.log()->Error("[Gui::Renderer::Draw]: Buffers are still in use!");
        }
        buf_range_fences_[ctx_.backend_frame()] = {};
    }
#endif

    //
    // Update buffers
    //
    const GLbitfield BufRangeMapFlags = GLbitfield(GL_MAP_COHERENT_BIT) | GLbitfield(GL_MAP_WRITE_BIT) |
                                        GLbitfield(GL_MAP_INVALIDATE_RANGE_BIT) | GLbitfield(GL_MAP_UNSYNCHRONIZED_BIT);

    if (vtx_count_[ctx_.backend_frame()]) {
        //
        // Update stage buffer
        //
        glBindBuffer(GL_COPY_READ_BUFFER, vertex_stage_buf_->id());

        const size_t vertex_buf_mem_offset = GLintptr(ctx_.backend_frame()) * MaxVerticesPerRange * sizeof(vertex_t);
        const size_t vertex_buf_mem_size = vtx_count_[ctx_.backend_frame()] * sizeof(vertex_t);
        if (!ctx_.capabilities.persistent_buf_mapping) {
            void *pinned_mem = glMapBufferRange(GL_COPY_READ_BUFFER, vertex_buf_mem_offset,
                                                MaxVerticesPerRange * sizeof(vertex_t), BufRangeMapFlags);
            if (pinned_mem) {
                memcpy(pinned_mem, vtx_stage_data_ + size_t(ctx_.backend_frame()) * MaxVerticesPerRange,
                       vertex_buf_mem_size);
                glUnmapBuffer(GL_COPY_READ_BUFFER);
            } else {
                ctx_.log()->Error("[Gui::Renderer::Draw]: Failed to map vertex buffer!");
            }
        }

        //
        // Copy stage buffer contents to actual vertex buffer
        //
        glBindBuffer(GL_COPY_WRITE_BUFFER, vertex_buf_->id());
        glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, vertex_buf_mem_offset /* read_offset */,
                            0 /* write_offset */, vertex_buf_mem_size);

        glBindBuffer(GL_COPY_READ_BUFFER, 0);
        glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
    }

    const size_t index_buf_mem_offset = size_t(ctx_.backend_frame()) * MaxIndicesPerRange * sizeof(uint16_t);

    if (ndx_count_[ctx_.backend_frame()]) {
        //
        // Update stage buffer
        //
        glBindBuffer(GL_COPY_READ_BUFFER, index_stage_buf_->id());

        const size_t index_buf_mem_size = ndx_count_[ctx_.backend_frame()] * sizeof(uint16_t);
        if (!ctx_.capabilities.persistent_buf_mapping) {
            void *pinned_mem = glMapBufferRange(GL_COPY_READ_BUFFER, index_buf_mem_offset,
                                                MaxIndicesPerRange * sizeof(uint16_t), BufRangeMapFlags);
            if (pinned_mem) {
                memcpy(pinned_mem, ndx_stage_data_ + size_t(ctx_.backend_frame()) * MaxIndicesPerRange,
                       index_buf_mem_size);
                glUnmapBuffer(GL_COPY_READ_BUFFER);
            } else {
                ctx_.log()->Error("[Gui::Renderer::Draw]: Failed to map index buffer!");
            }
        }

        //
        // Copy stage buffer contents to actual index buffer
        //
        glBindBuffer(GL_COPY_WRITE_BUFFER, index_buf_->id());
        glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, index_buf_mem_offset /* read_offset */,
                            0 /* write_offset */, index_buf_mem_size);

        glBindBuffer(GL_COPY_READ_BUFFER, 0);
        glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
    }

    //
    // Submit draw call
    //
    pipeline_.rast_state().viewport[2] = w;
    pipeline_.rast_state().viewport[3] = h;
    pipeline_.rast_state().Apply();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glBindVertexArray(pipeline_.vtx_input()->gl_vao());
    glUseProgram(pipeline_.prog()->id());

    glActiveTexture(GL_TEXTURE0 + UIRendererConstants::TexAtlasSlot);
    glBindTexture(GL_TEXTURE_2D_ARRAY, GLuint(ctx_.texture_atlas().id()));

    glDrawElements(GL_TRIANGLES, ndx_count_[ctx_.backend_frame()], GL_UNSIGNED_SHORT,
                   reinterpret_cast<const GLvoid *>(uintptr_t(0)));

    glBindVertexArray(0);
    glUseProgram(0);

    vtx_count_[ctx_.backend_frame()] = 0;
    ndx_count_[ctx_.backend_frame()] = 0;

#ifndef NDEBUG
    assert(!buf_range_fences_[ctx_.backend_frame()]);
    buf_range_fences_[ctx_.backend_frame()] = Ren::MakeFence();
#endif
}

#undef VTX_POS_LOC
#undef VTX_COL_LOC
#undef VTX_UVS_LOC

#undef TEX_ATLAS_SLOT
