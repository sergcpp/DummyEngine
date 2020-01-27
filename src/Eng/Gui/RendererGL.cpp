#include "Renderer.h"

#include <cassert>

#include <Ren/GL.h>
#include <Sys/Json.h>

#define VTX_POS_LOC 0
#define VTX_COL_LOC 1
#define VTX_UVS_LOC 2

#define TEX_ATLAS_SLOT 0

namespace UIRendererConstants {
#define _AS_STR(x) #x
#define AS_STR(x) _AS_STR(x)

const char vs_source2[] =
R"(#version 310 es

layout(location = )" AS_STR(VTX_POS_LOC) R"() in vec3 aVertexPosition;
layout(location = )" AS_STR(VTX_COL_LOC) R"() in vec4 aVertexColor;
layout(location = )" AS_STR(VTX_UVS_LOC) R"() in vec4 aVertexUVs;

     out vec4 aVertexColor_;
     out vec3 aVertexUVs_;
flat out float aVertexMode_;

void main(void) {
    gl_Position = vec4(aVertexPosition, 1.0);
    aVertexColor_ = aVertexColor;
    aVertexUVs_ = aVertexUVs.xyz * vec3(1.0, 1.0, 16.0);
    aVertexMode_ = aVertexUVs.w;
}
)";

const char fs_source2[] =
R"(#version 310 es
#ifdef GL_ES
	precision mediump float;
#else
	#define lowp
	#define mediump
	#define highp
#endif

layout(binding = )" AS_STR(TEX_ATLAS_SLOT) R"() uniform mediump sampler2DArray s_texture;

     in vec4 aVertexColor_;
     in vec3 aVertexUVs_;
flat in float aVertexMode_;

out vec4 outColor;

float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

void main(void) {
    vec4 tex_color = texture(s_texture, aVertexUVs_);

    if (aVertexMode_ < 0.25) {
        // Simple texture drawing
	    outColor = aVertexColor_ * tex_color;
    } else if (aVertexMode_ < 0.75) {
        // SDF drawing
        float sig_dist = median(tex_color.r, tex_color.g, tex_color.b);
        
        float s = sig_dist - 0.5;
        float v = s / fwidth(s);
        
        vec4 base_color;
        base_color.rgb = vec3(1.0);
        base_color.a = clamp(v + 0.5, 0.0, 1.0);
        
        outColor = aVertexColor_ * base_color;
    } else {
        // SDF blitting
        float sig_dist = median(tex_color.r, tex_color.g, tex_color.b);
        outColor = aVertexColor_ * tex_color;
        outColor.a = step(0.1, sig_dist);
    }
}
)";

#undef _AS_STR
#undef AS_STR

inline void BindTexture(int slot, uint32_t tex) {
    glActiveTexture((GLenum)(GL_TEXTURE0 + slot));
    glBindTexture(GL_TEXTURE_2D, (GLuint)tex);
}

const int MaxVerticesPerBuf = 8 * 1024;
const int MaxIndicesPerBuf = 16 * 1024;
}

Gui::Renderer::Renderer(Ren::Context &ctx, const JsObject &config)
        : ctx_(ctx) {
    using namespace UIRendererConstants;

    const JsString &js_gl_defines = (const JsString &)config.at(GL_DEFINES_KEY);

    {   // Load main shader
        Ren::eProgLoadStatus status;
        ui_program2_ = ctx_.LoadProgramGLSL(UI_PROGRAM2_NAME, vs_source2, fs_source2, &status);
        assert(status == Ren::ProgCreatedFromData || status == Ren::ProgFound);
    }

    cur_range_index_ = 0;

    for (int i = 0; i < BuffersCount; i++) {
        vertex_count_[i] = 0;
        index_count_[i] = 0;

        GLuint vtx_buf_id;
        glGenBuffers(1, &vtx_buf_id);
        glBindBuffer(GL_ARRAY_BUFFER, vtx_buf_id);
        glBufferData(GL_ARRAY_BUFFER, FrameSyncWindow * MaxVerticesPerBuf * sizeof(vertex_t), nullptr, GL_DYNAMIC_DRAW);
        
        GLuint ndx_buf_id;
        glGenBuffers(1, &ndx_buf_id);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ndx_buf_id);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, FrameSyncWindow * MaxIndicesPerBuf * sizeof(uint16_t), nullptr, GL_DYNAMIC_DRAW);

        GLuint vao;
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);

        glBindBuffer(GL_ARRAY_BUFFER, vtx_buf_id);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ndx_buf_id);

        glEnableVertexAttribArray(VTX_POS_LOC);
        glVertexAttribPointer(VTX_POS_LOC, 3, GL_FLOAT, GL_FALSE, sizeof(vertex_t), (void *)uintptr_t(offsetof(vertex_t, pos)));

        glEnableVertexAttribArray(VTX_COL_LOC);
        glVertexAttribPointer(VTX_COL_LOC, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(vertex_t), (void *)uintptr_t(offsetof(vertex_t, col)));

        glEnableVertexAttribArray(VTX_UVS_LOC);
        glVertexAttribPointer(VTX_UVS_LOC, 4, GL_UNSIGNED_SHORT, GL_TRUE, sizeof(vertex_t), (void *)uintptr_t(offsetof(vertex_t, uvs)));

        glBindVertexArray(0);

        vao_[i] = (uint32_t)vao;
        vertex_buf_id_[i] = (uint32_t)vtx_buf_id;
        index_buf_id_[i] = (uint32_t)ndx_buf_id;
    }
}

Gui::Renderer::~Renderer() {
    for (int i = 0; i < BuffersCount; i++) {
        auto buf_id = (GLuint)vao_[i];
        glDeleteVertexArrays(1, &buf_id);

        buf_id = (GLuint)vertex_buf_id_[i];
        glDeleteBuffers(1, &buf_id);
        buf_id = (GLuint)index_buf_id_[i];
        glDeleteBuffers(1, &buf_id);
    }
}

void Gui::Renderer::BeginDraw() {
    using namespace UIRendererConstants;

#ifndef DISABLE_MARKERS
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "UI DRAW");
#endif
    glBindVertexArray(0);
    glUseProgram(ui_program2_->prog_id());

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
#if !defined(__ANDROID__)
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
#endif

    cur_range_index_ = (cur_range_index_ + 1) % FrameSyncWindow;
    if (buf_range_fences_[cur_range_index_]) {
        auto sync = reinterpret_cast<GLsync>(buf_range_fences_[cur_range_index_]);
        GLenum res = glClientWaitSync(sync, 0, 1000000000);
        if (res != GL_ALREADY_SIGNALED && res != GL_CONDITION_SATISFIED) {
            ctx_.log()->Error("[Gui::Renderer::BeginDraw2]: Wait failed!");
        }
        glDeleteSync(sync);
        buf_range_fences_[cur_range_index_] = nullptr;
    }
    cur_buffer_index_ = 0;

    cur_mapped_vtx_data_ = nullptr;
    cur_mapped_ndx_data_ = nullptr;

    cur_vertex_count_ = 0;
    cur_index_count_ = 0;
}

void Gui::Renderer::EndDraw() {
    if (cur_mapped_vtx_data_ && cur_mapped_ndx_data_) {
        SubmitVertexData(0, 0, true);
    }

#ifndef DISABLE_MARKERS
    glPopDebugGroup();
#endif

    assert(!buf_range_fences_[cur_range_index_]);
    buf_range_fences_[cur_range_index_] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
}

int Gui::Renderer::AcquireVertexData(vertex_t **vertex_data, int *vertex_avail, uint16_t **index_data, int *index_avail) {
    using namespace UIRendererConstants;

    if (!cur_mapped_vtx_data_) {
        assert(!cur_mapped_ndx_data_);

        // Map next gl buffers
        const size_t
            vertex_buf_mem_offset = (cur_range_index_ * MaxVerticesPerBuf) * sizeof(vertex_t),
            vertex_buf_mem_size = MaxVerticesPerBuf * sizeof(vertex_t),
            index_buf_mem_offset = (cur_range_index_ * MaxIndicesPerBuf) * sizeof(uint16_t),
            index_buf_mem_size = MaxIndicesPerBuf * sizeof(uint16_t);

        const GLbitfield BufferRangeBindFlags =
            GLbitfield(GL_MAP_WRITE_BIT) | GLbitfield(GL_MAP_INVALIDATE_RANGE_BIT) |
            GLbitfield(GL_MAP_UNSYNCHRONIZED_BIT) | GLbitfield(GL_MAP_FLUSH_EXPLICIT_BIT);

        glBindBuffer(GL_ARRAY_BUFFER, vertex_buf_id_[cur_buffer_index_]);
        cur_mapped_vtx_data_ = (vertex_t *)glMapBufferRange(GL_ARRAY_BUFFER, vertex_buf_mem_offset, vertex_buf_mem_size, BufferRangeBindFlags);
        cur_vertex_count_ = 0;
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buf_id_[cur_buffer_index_]);
        cur_mapped_ndx_data_ = (uint16_t *)glMapBufferRange(GL_ELEMENT_ARRAY_BUFFER, index_buf_mem_offset, index_buf_mem_size, BufferRangeBindFlags);
        cur_index_count_ = 0;
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }

    assert(cur_mapped_vtx_data_ && cur_mapped_ndx_data_);

    (*vertex_data) = cur_mapped_vtx_data_ + cur_vertex_count_;
    (*vertex_avail) = MaxVerticesPerBuf - cur_vertex_count_;

    (*index_data) = cur_mapped_ndx_data_ + cur_index_count_;
    (*index_avail) = MaxIndicesPerBuf - cur_index_count_;

    return cur_vertex_count_;
}

void Gui::Renderer::SubmitVertexData(int vertex_count, int index_count, bool force_new_buffer) {
    using namespace UIRendererConstants;

    assert((cur_vertex_count_ + vertex_count) <= MaxVerticesPerBuf && (cur_index_count_ + index_count) <= MaxIndicesPerBuf);

    cur_vertex_count_ += vertex_count;
    cur_index_count_ += index_count;

    if (cur_vertex_count_ == MaxVerticesPerBuf || cur_index_count_ == MaxIndicesPerBuf || force_new_buffer) {
        assert(cur_mapped_vtx_data_ && cur_mapped_ndx_data_);

        {   // flush mapped buffers
            const size_t
                vertex_buf_mem_size = cur_vertex_count_ * sizeof(vertex_t),
                index_buf_mem_size = cur_index_count_ * sizeof(uint16_t);

            glBindBuffer(GL_ARRAY_BUFFER, vertex_buf_id_[cur_buffer_index_]);
            if (vertex_buf_mem_size) {
                glFlushMappedBufferRange(GL_ARRAY_BUFFER, 0, vertex_buf_mem_size);
            }
            glUnmapBuffer(GL_ARRAY_BUFFER);
            glBindBuffer(GL_ARRAY_BUFFER, 0);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buf_id_[cur_buffer_index_]);
            if (index_buf_mem_size) {
                glFlushMappedBufferRange(GL_ELEMENT_ARRAY_BUFFER, 0, index_buf_mem_size);
            }
            glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        }

        // make a draw call
        DrawCurrentBuffer();

        // start new buffer
        cur_buffer_index_++;
        assert(cur_buffer_index_ < BuffersCount);

        cur_mapped_vtx_data_ = nullptr;
        cur_mapped_ndx_data_ = nullptr;
    }
}

void Gui::Renderer::DrawCurrentBuffer() {
    using namespace UIRendererConstants;

    if (!cur_index_count_) return;

    glBindVertexArray(vao_[cur_buffer_index_]);

    glUseProgram(ui_program2_->prog_id());

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, (GLuint)ctx_.texture_atlas().tex_id());

    const size_t
        index_buf_mem_offset = (cur_range_index_ * MaxIndicesPerBuf) * sizeof(uint16_t);

    glDrawElementsBaseVertex(GL_TRIANGLES, cur_index_count_, GL_UNSIGNED_SHORT,
                             reinterpret_cast<const GLvoid *>(uintptr_t(index_buf_mem_offset)), (cur_range_index_ * MaxVerticesPerBuf));

    glBindVertexArray(0);
    glUseProgram(0);
}

void Gui::Renderer::DrawImageQuad(eDrawMode draw_mode, int tex_layer, const Vec2f pos[2], const Vec2f uvs_px[2]) {
    const Vec2f uvs_scale = 1.0f / Vec2f{ (float)Ren::TextureAtlasWidth, (float)Ren::TextureAtlasHeight };
    const Vec2f uvs[2] = {
        uvs_px[0] * uvs_scale,
        uvs_px[1] * uvs_scale
    };

    vertex_t *vtx_data; int vtx_avail;
    uint16_t *ndx_data; int ndx_avail;
    int ndx_offset = AcquireVertexData(&vtx_data, &vtx_avail, &ndx_data, &ndx_avail);
    if (vtx_avail < 4 || ndx_avail < 6) {
        SubmitVertexData(0, 0, true);
        ndx_offset = AcquireVertexData(&vtx_data, &vtx_avail, &ndx_data, &ndx_avail);
    }

    vertex_t *cur_vtx = vtx_data;
    uint16_t *cur_ndx = ndx_data;

    uint16_t u16_tex_layer = f32_to_u16((1.0f / 16.0f) * float(tex_layer));

    static const uint16_t u16_draw_mode[] = {
        0, 32767, 65535
    };

    cur_vtx->pos[0] = pos[0][0];
    cur_vtx->pos[1] = pos[0][1];
    cur_vtx->pos[2] = 0.0f;
    cur_vtx->col[0] = cur_vtx->col[1] = cur_vtx->col[2] = cur_vtx->col[3] = 255;
    cur_vtx->uvs[0] = f32_to_u16(uvs[0][0]);
    cur_vtx->uvs[1] = f32_to_u16(uvs[1][1]);
    cur_vtx->uvs[2] = u16_tex_layer;
    cur_vtx->uvs[3] = u16_draw_mode[draw_mode];
    ++cur_vtx;

    cur_vtx->pos[0] = pos[1][0];
    cur_vtx->pos[1] = pos[0][1];
    cur_vtx->pos[2] = 0.0f;
    cur_vtx->col[0] = cur_vtx->col[1] = cur_vtx->col[2] = cur_vtx->col[3] = 255;
    cur_vtx->uvs[0] = f32_to_u16(uvs[1][0]);
    cur_vtx->uvs[1] = f32_to_u16(uvs[1][1]);
    cur_vtx->uvs[2] = u16_tex_layer;
    cur_vtx->uvs[3] = u16_draw_mode[draw_mode];
    ++cur_vtx;

    cur_vtx->pos[0] = pos[1][0];
    cur_vtx->pos[1] = pos[1][1];
    cur_vtx->pos[2] = 0.0f;
    cur_vtx->col[0] = cur_vtx->col[1] = cur_vtx->col[2] = cur_vtx->col[3] = 255;
    cur_vtx->uvs[0] = f32_to_u16(uvs[1][0]);
    cur_vtx->uvs[1] = f32_to_u16(uvs[0][1]);
    cur_vtx->uvs[2] = u16_tex_layer;
    cur_vtx->uvs[3] = u16_draw_mode[draw_mode];
    ++cur_vtx;

    cur_vtx->pos[0] = pos[0][0];
    cur_vtx->pos[1] = pos[1][1];
    cur_vtx->pos[2] = 0.0f;
    cur_vtx->col[0] = cur_vtx->col[1] = cur_vtx->col[2] = cur_vtx->col[3] = 255;
    cur_vtx->uvs[0] = f32_to_u16(uvs[0][0]);
    cur_vtx->uvs[1] = f32_to_u16(uvs[0][1]);
    cur_vtx->uvs[2] = u16_tex_layer;
    cur_vtx->uvs[3] = u16_draw_mode[draw_mode];
    ++cur_vtx;

    (*cur_ndx++) = ndx_offset + 0;
    (*cur_ndx++) = ndx_offset + 1;
    (*cur_ndx++) = ndx_offset + 2;

    (*cur_ndx++) = ndx_offset + 0;
    (*cur_ndx++) = ndx_offset + 2;
    (*cur_ndx++) = ndx_offset + 3;

    SubmitVertexData(int(cur_vtx - vtx_data), int(cur_ndx - ndx_data), false);
}

#undef VTX_POS_LOC
#undef VTX_COL_LOC
#undef VTX_UVS_LOC

#undef TEX_ATLAS_SLOT
