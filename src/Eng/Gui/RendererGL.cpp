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

const char vs_source[] =
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

const char fs_source[] =
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

const int MaxVerticesPerRange = 64 * 1024;
const int MaxIndicesPerRange = 128 * 1024;
} // namespace UIRendererConstants

Gui::Renderer::Renderer(Ren::Context &ctx, const JsObject &config) : ctx_(ctx) {
    using namespace UIRendererConstants;

    const JsString &js_gl_defines = config.at(GL_DEFINES_KEY).as_str();

    { // Load main shader
        using namespace Ren;

        eShaderLoadStatus sh_status;
        ShaderRef ui_vs_ref =
            ctx_.LoadShaderGLSL("__ui_vs__", vs_source, eShaderType::Vert, &sh_status);
        assert(sh_status == eShaderLoadStatus::CreatedFromData ||
               sh_status == eShaderLoadStatus::Found);
        ShaderRef ui_fs_ref =
            ctx_.LoadShaderGLSL("__ui_fs__", fs_source, eShaderType::Frag, &sh_status);
        assert(sh_status == eShaderLoadStatus::CreatedFromData ||
               sh_status == eShaderLoadStatus::Found);

        eProgLoadStatus status;
        ui_program_ =
            ctx_.LoadProgram("__ui_program__", ui_vs_ref, ui_fs_ref, {}, {}, &status);
        assert(status == eProgLoadStatus::CreatedFromData ||
               status == eProgLoadStatus::Found);
    }

    vtx_data_.reset(new vertex_t[MaxVerticesPerRange * FrameSyncWindow]);
    for (int i = 0; i < FrameSyncWindow; i++) {
        vertex_count_[i] = 0;
    }
    ndx_data_.reset(new uint16_t[MaxIndicesPerRange * FrameSyncWindow]);
    for (int i = 0; i < FrameSyncWindow; i++) {
        index_count_[i] = 0;
    }

    vertex_buf_ =
        ctx_.CreateBuffer("UI_VertexBuffer", Ren::eBufferType::VertexAttribs,
                          Ren::eBufferAccessType::Draw, Ren::eBufferAccessFreq::Dynamic,
                          FrameSyncWindow * MaxVerticesPerRange * sizeof(vertex_t));

    index_buf_ =
        ctx_.CreateBuffer("UI_IndexBuffer", Ren::eBufferType::VertexIndices,
                          Ren::eBufferAccessType::Draw, Ren::eBufferAccessFreq::Dynamic,
                          FrameSyncWindow * MaxIndicesPerRange * sizeof(uint16_t));

    const Ren::VtxAttribDesc attribs[] = {
        {vertex_buf_->handle(), VTX_POS_LOC, 3, Ren::eType::Float32,
         sizeof(vertex_t), uintptr_t(offsetof(vertex_t, pos))},
        {vertex_buf_->handle(), VTX_COL_LOC, 4, Ren::eType::Uint8UNorm,
         sizeof(vertex_t), uintptr_t(offsetof(vertex_t, col))},
        {vertex_buf_->handle(), VTX_UVS_LOC, 4, Ren::eType::Uint16UNorm,
         sizeof(vertex_t), uintptr_t(offsetof(vertex_t, uvs))}};

    vao_.Setup(attribs, 3, index_buf_->handle());

    draw_range_index_ = 0;
    fill_range_index_ = (draw_range_index_ + (FrameSyncWindow - 1)) % FrameSyncWindow;
}

Gui::Renderer::~Renderer() {
    for (int i = 0; i < FrameSyncWindow; i++) {
        if (buf_range_fences_[i]) {
            auto sync = reinterpret_cast<GLsync>(buf_range_fences_[i]);
            const GLenum res = glClientWaitSync(sync, 0, 1000000000);
            if (res != GL_ALREADY_SIGNALED && res != GL_CONDITION_SATISFIED) {
                ctx_.log()->Error("[Gui::Renderer::~Renderer]: Wait failed!");
            }
            glDeleteSync(sync);
            buf_range_fences_[i] = nullptr;
        }
    }
}

void Gui::Renderer::PushClipArea(const Vec2f dims[2]) {
    clip_area_stack_[clip_area_stack_size_][0] = dims[0];
    clip_area_stack_[clip_area_stack_size_][1] = dims[0] + dims[1];
    if (clip_area_stack_size_) {
        clip_area_stack_[clip_area_stack_size_][0] =
            Max(clip_area_stack_[clip_area_stack_size_ - 1][0],
                clip_area_stack_[clip_area_stack_size_][0]);
        clip_area_stack_[clip_area_stack_size_][1] =
            Min(clip_area_stack_[clip_area_stack_size_ - 1][1],
                clip_area_stack_[clip_area_stack_size_][1]);
    }
    ++clip_area_stack_size_;
}

void Gui::Renderer::PopClipArea() { --clip_area_stack_size_; }

const Gui::Vec2f *Gui::Renderer::GetClipArea() const {
    if (clip_area_stack_size_) {
        return clip_area_stack_[clip_area_stack_size_ - 1];
    }
    return nullptr;
}

int Gui::Renderer::AcquireVertexData(vertex_t **vertex_data, int *vertex_avail,
                                     uint16_t **index_data, int *index_avail) {
    using namespace UIRendererConstants;

    (*vertex_data) = vtx_data_.get() + fill_range_index_ * MaxVerticesPerRange +
                     vertex_count_[fill_range_index_];
    (*vertex_avail) = MaxVerticesPerRange - vertex_count_[fill_range_index_];

    (*index_data) = ndx_data_.get() + fill_range_index_ * MaxIndicesPerRange +
                    index_count_[fill_range_index_];
    (*index_avail) = MaxIndicesPerRange - index_count_[fill_range_index_];

    return vertex_count_[fill_range_index_];
}

void Gui::Renderer::SubmitVertexData(const int vertex_count, const int index_count) {
    using namespace UIRendererConstants;

    assert((vertex_count_[fill_range_index_] + vertex_count) <= MaxVerticesPerRange &&
           (index_count_[fill_range_index_] + index_count) <= MaxIndicesPerRange);

    vertex_count_[fill_range_index_] += vertex_count;
    index_count_[fill_range_index_] += index_count;
}

void Gui::Renderer::SwapBuffers() {
    draw_range_index_ = (draw_range_index_ + 1) % FrameSyncWindow;
    fill_range_index_ = (draw_range_index_ + (FrameSyncWindow - 1)) % FrameSyncWindow;
}

void Gui::Renderer::Draw() {
    using namespace UIRendererConstants;

    //
    // Synchronize with previous draw
    //
    if (buf_range_fences_[draw_range_index_]) {
        auto sync = reinterpret_cast<GLsync>(buf_range_fences_[draw_range_index_]);
        const GLenum res = glClientWaitSync(sync, 0, 1000000000);
        if (res != GL_ALREADY_SIGNALED && res != GL_CONDITION_SATISFIED) {
            ctx_.log()->Error("[Gui::Renderer::BeginDraw2]: Wait failed!");
        }
        glDeleteSync(sync);
        buf_range_fences_[draw_range_index_] = nullptr;
    }

    //
    // Update buffers
    //
    const GLbitfield BufferRangeBindFlags =
        GLbitfield(GL_MAP_WRITE_BIT) | GLbitfield(GL_MAP_INVALIDATE_RANGE_BIT) |
        GLbitfield(GL_MAP_UNSYNCHRONIZED_BIT) | GLbitfield(GL_MAP_FLUSH_EXPLICIT_BIT);

    if (vertex_count_[draw_range_index_]) {
        glBindBuffer(GL_ARRAY_BUFFER, vertex_buf_->id());

        void *pinned_mem = glMapBufferRange(
            GL_ARRAY_BUFFER, draw_range_index_ * MaxVerticesPerRange * sizeof(vertex_t),
            MaxVerticesPerRange * sizeof(vertex_t), BufferRangeBindFlags);
        if (pinned_mem) {
            const size_t vertex_buf_mem_size =
                vertex_count_[draw_range_index_] * sizeof(vertex_t);
            memcpy(pinned_mem, vtx_data_.get() + draw_range_index_ * MaxVerticesPerRange,
                   vertex_buf_mem_size);
            glFlushMappedBufferRange(GL_ARRAY_BUFFER, 0, vertex_buf_mem_size);
            glUnmapBuffer(GL_ARRAY_BUFFER);
        } else {
            ctx_.log()->Error(
                "[Gui::Renderer::SwapBuffers]: Failed to map vertex buffer!");
        }
    }

    if (index_count_[draw_range_index_]) {
        glBindBuffer(GL_ARRAY_BUFFER, index_buf_->id());

        void *pinned_mem = glMapBufferRange(
            GL_ARRAY_BUFFER, draw_range_index_ * MaxIndicesPerRange * sizeof(uint16_t),
            MaxIndicesPerRange * sizeof(uint16_t), BufferRangeBindFlags);
        if (pinned_mem) {
            const size_t index_buf_mem_size =
                index_count_[draw_range_index_] * sizeof(uint16_t);
            memcpy(pinned_mem, ndx_data_.get() + draw_range_index_ * MaxIndicesPerRange,
                   index_buf_mem_size);
            glFlushMappedBufferRange(GL_ARRAY_BUFFER, 0, index_buf_mem_size);
            glUnmapBuffer(GL_ARRAY_BUFFER);
        } else {
            ctx_.log()->Error(
                "[Gui::Renderer::SwapBuffers]: Failed to map index buffer!");
        }
    }

    //
    // Submit draw call
    //
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
#if !defined(__ANDROID__)
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
#endif

    glBindVertexArray(vao_.id());
    glUseProgram(ui_program_->id());

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, (GLuint)ctx_.texture_atlas().tex_id());

    const size_t index_buf_mem_offset =
        draw_range_index_ * MaxIndicesPerRange * sizeof(uint16_t);

    glDrawElementsBaseVertex(
        GL_TRIANGLES, index_count_[draw_range_index_], GL_UNSIGNED_SHORT,
        reinterpret_cast<const GLvoid *>(uintptr_t(index_buf_mem_offset)),
        (draw_range_index_ * MaxVerticesPerRange));

    glBindVertexArray(0);
    glUseProgram(0);

    vertex_count_[draw_range_index_] = 0;
    index_count_[draw_range_index_] = 0;

    assert(!buf_range_fences_[draw_range_index_]);
    buf_range_fences_[draw_range_index_] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
}

void Gui::Renderer::PushImageQuad(const eDrawMode draw_mode, const int tex_layer,
                                  const Vec2f pos[2], const Vec2f uvs_px[2]) {
    const Vec2f uvs_scale =
        1.0f / Vec2f{(float)Ren::TextureAtlasWidth, (float)Ren::TextureAtlasHeight};
    Vec4f pos_uvs[2] = {Vec4f{pos[0][0], pos[0][1], uvs_px[0][0] * uvs_scale[0],
                              uvs_px[0][1] * uvs_scale[1]},
                        Vec4f{pos[1][0], pos[1][1], uvs_px[1][0] * uvs_scale[0],
                              uvs_px[1][1] * uvs_scale[1]}};

    vertex_t *vtx_data;
    int vtx_avail;
    uint16_t *ndx_data;
    int ndx_avail;
    int ndx_offset = AcquireVertexData(&vtx_data, &vtx_avail, &ndx_data, &ndx_avail);
    assert(vtx_avail >= 4 && ndx_avail >= 6);

    vertex_t *cur_vtx = vtx_data;
    uint16_t *cur_ndx = ndx_data;

    const uint16_t u16_tex_layer = f32_to_u16((1.0f / 16.0f) * float(tex_layer));

    static const uint16_t u16_draw_mode[] = {0, 32767, 65535};

    if (clip_area_stack_size_ &&
        !ClipQuadToArea(pos_uvs, clip_area_stack_[clip_area_stack_size_ - 1])) {
        return;
    }

    cur_vtx->pos[0] = pos_uvs[0][0];
    cur_vtx->pos[1] = pos_uvs[0][1];
    cur_vtx->pos[2] = 0.0f;
    cur_vtx->col[0] = cur_vtx->col[1] = cur_vtx->col[2] = cur_vtx->col[3] = 255;
    cur_vtx->uvs[0] = f32_to_u16(pos_uvs[0][2]);
    cur_vtx->uvs[1] = f32_to_u16(pos_uvs[0][3]);
    cur_vtx->uvs[2] = u16_tex_layer;
    cur_vtx->uvs[3] = u16_draw_mode[int(draw_mode)];
    ++cur_vtx;

    cur_vtx->pos[0] = pos_uvs[1][0];
    cur_vtx->pos[1] = pos_uvs[0][1];
    cur_vtx->pos[2] = 0.0f;
    cur_vtx->col[0] = cur_vtx->col[1] = cur_vtx->col[2] = cur_vtx->col[3] = 255;
    cur_vtx->uvs[0] = f32_to_u16(pos_uvs[1][2]);
    cur_vtx->uvs[1] = f32_to_u16(pos_uvs[0][3]);
    cur_vtx->uvs[2] = u16_tex_layer;
    cur_vtx->uvs[3] = u16_draw_mode[int(draw_mode)];
    ++cur_vtx;

    cur_vtx->pos[0] = pos_uvs[1][0];
    cur_vtx->pos[1] = pos_uvs[1][1];
    cur_vtx->pos[2] = 0.0f;
    cur_vtx->col[0] = cur_vtx->col[1] = cur_vtx->col[2] = cur_vtx->col[3] = 255;
    cur_vtx->uvs[0] = f32_to_u16(pos_uvs[1][2]);
    cur_vtx->uvs[1] = f32_to_u16(pos_uvs[1][3]);
    cur_vtx->uvs[2] = u16_tex_layer;
    cur_vtx->uvs[3] = u16_draw_mode[int(draw_mode)];
    ++cur_vtx;

    cur_vtx->pos[0] = pos_uvs[0][0];
    cur_vtx->pos[1] = pos_uvs[1][1];
    cur_vtx->pos[2] = 0.0f;
    cur_vtx->col[0] = cur_vtx->col[1] = cur_vtx->col[2] = cur_vtx->col[3] = 255;
    cur_vtx->uvs[0] = f32_to_u16(pos_uvs[0][2]);
    cur_vtx->uvs[1] = f32_to_u16(pos_uvs[1][3]);
    cur_vtx->uvs[2] = u16_tex_layer;
    cur_vtx->uvs[3] = u16_draw_mode[int(draw_mode)];
    ++cur_vtx;

    (*cur_ndx++) = ndx_offset + 0;
    (*cur_ndx++) = ndx_offset + 1;
    (*cur_ndx++) = ndx_offset + 2;

    (*cur_ndx++) = ndx_offset + 0;
    (*cur_ndx++) = ndx_offset + 2;
    (*cur_ndx++) = ndx_offset + 3;

    SubmitVertexData(int(cur_vtx - vtx_data), int(cur_ndx - ndx_data));
}

void Gui::Renderer::PushLine(eDrawMode draw_mode, int tex_layer, const uint8_t color[4],
                             const Vec4f &p0, const Vec4f &p1, const Vec2f &d0,
                             const Vec2f &d1, const Vec4f &thickness) {
    const Vec2f uvs_scale =
        1.0f / Vec2f{(float)Ren::TextureAtlasWidth, (float)Ren::TextureAtlasHeight};

    const uint16_t u16_tex_layer = f32_to_u16((1.0f / 16.0f) * float(tex_layer));

    static const uint16_t u16_draw_mode[] = {0, 32767, 65535};

    const Vec4f perp[2] = {Vec4f{thickness} * Vec4f{-d0[1], d0[0], 1.0f, 0.0f},
                           Vec4f{thickness} * Vec4f{-d1[1], d1[0], 1.0f, 0.0f}};

    Vec4f pos_uvs[8] = {p0 - perp[0], p1 - perp[1], p1 + perp[1], p0 + perp[0]};
    int vertex_count = 4;

    for (int i = 0; i < vertex_count; i++) {
        pos_uvs[i][2] *= uvs_scale[0];
        pos_uvs[i][3] *= uvs_scale[1];
    }

    if (clip_area_stack_size_ &&
        !(vertex_count = ClipPolyToArea(pos_uvs, vertex_count,
                                        clip_area_stack_[clip_area_stack_size_ - 1]))) {
        return;
    }
    assert(vertex_count < 8);

    vertex_t *vtx_data;
    int vtx_avail;
    uint16_t *ndx_data;
    int ndx_avail;
    int ndx_offset = AcquireVertexData(&vtx_data, &vtx_avail, &ndx_data, &ndx_avail);
    assert(vtx_avail >= vertex_count && ndx_avail >= 3 * (vertex_count - 2));

    vertex_t *cur_vtx = vtx_data;
    uint16_t *cur_ndx = ndx_data;

    for (int i = 0; i < vertex_count; i++) {
        cur_vtx->pos[0] = pos_uvs[i][0];
        cur_vtx->pos[1] = pos_uvs[i][1];
        cur_vtx->pos[2] = 0.0f;
        memcpy(cur_vtx->col, color, 4);
        cur_vtx->uvs[0] = f32_to_u16(pos_uvs[i][2]);
        cur_vtx->uvs[1] = f32_to_u16(pos_uvs[i][3]);
        cur_vtx->uvs[2] = u16_tex_layer;
        cur_vtx->uvs[3] = u16_draw_mode[int(draw_mode)];
        ++cur_vtx;
    }

    for (int i = 0; i < vertex_count - 2; i++) {
        (*cur_ndx++) = ndx_offset + 0;
        (*cur_ndx++) = ndx_offset + i + 1;
        (*cur_ndx++) = ndx_offset + i + 2;
    }

    SubmitVertexData(int(cur_vtx - vtx_data), int(cur_ndx - ndx_data));
}

void Gui::Renderer::PushCurve(eDrawMode draw_mode, int tex_layer, const uint8_t color[4],
                              const Vec4f &p0, const Vec4f &p1, const Vec4f &p2,
                              const Vec4f &p3, const Vec4f &thickness) {
    const float tolerance = 0.000001f;

    const Vec4f p01 = 0.5f * (p0 + p1), p12 = 0.5f * (p1 + p2), p23 = 0.5f * (p2 + p3),
                p012 = 0.5f * (p01 + p12), p123 = 0.5f * (p12 + p23),
                p0123 = 0.5f * (p012 + p123);

    const Vec2f d = Vec2f{p3} - Vec2f{p0};
    const float d2 = std::abs((p1[0] - p3[0]) * d[1] - (p1[1] - p3[1]) * d[0]),
                d3 = std::abs((p2[0] - p3[0]) * d[1] - (p2[1] - p3[1]) * d[0]);

    if ((d2 + d3) * (d2 + d3) < tolerance * (d[0] * d[0] + d[1] * d[1])) {
        PushLine(draw_mode, tex_layer, color, p0, p3, Normalize(Vec2f{p1 - p0}),
                 Normalize(Vec2f{p3 - p2}), thickness);
    } else {
        PushCurve(draw_mode, tex_layer, color, p0, p01, p012, p0123, thickness);
        PushCurve(draw_mode, tex_layer, color, p0123, p123, p23, p3, thickness);
    }
}

#undef VTX_POS_LOC
#undef VTX_COL_LOC
#undef VTX_UVS_LOC

#undef TEX_ATLAS_SLOT
