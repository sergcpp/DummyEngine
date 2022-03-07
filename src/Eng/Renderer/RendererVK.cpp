#include "Renderer.h"

#include "../Utils/ShaderLoader.h"
#include <Ren/Camera.h>
#include <Ren/Context.h>
//#include <Ren/GL.h>

namespace PrimDrawInternal {
extern const float fs_quad_positions[];
extern const float fs_quad_norm_uvs[];
extern const uint16_t fs_quad_indices[];
} // namespace PrimDrawInternal

namespace RendererInternal {
const int U_MVP_MATR = 0;

const int U_GAMMA = 14;
const int U_EXPOSURE = 15;
const int U_FADE = 16;

const int U_RES = 15;

const int TEMP_BUF_SIZE = 256;
} // namespace RendererInternal

void Renderer::InitRendererInternal() {
    using namespace RendererInternal;
    using namespace Ren;

#if 0
    blit_prog_ = sh_.LoadProgram(ctx_, "blit", "internal/blit.vert.glsl",
                                 "internal/blit.frag.glsl");
    assert(blit_prog_->ready());
    blit_combine_prog_ = sh_.LoadProgram(ctx_, "blit_combine", "internal/blit.vert.glsl",
                                         "internal/blit_combine.frag.glsl");
    assert(blit_combine_prog_->ready());
    blit_ms_prog_ = sh_.LoadProgram(ctx_, "blit_ms", "internal/blit.vert.glsl",
                                    "internal/blit_ms.frag.glsl");
    assert(blit_ms_prog_->ready());
    blit_down_prog_ = sh_.LoadProgram(ctx_, "blit_down", "internal/blit.vert.glsl",
                                      "internal/blit_down.frag.glsl");
    assert(blit_down_prog_->ready());
    blit_gauss_prog_ = sh_.LoadProgram(ctx_, "blit_gauss", "internal/blit.vert.glsl",
                                       "internal/blit_gauss.frag.glsl");
    assert(blit_gauss_prog_->ready());
    blit_depth_prog_ = sh_.LoadProgram(ctx_, "blit_depth", "internal/blit.vert.glsl",
                                       "internal/blit_depth.frag.glsl");
    assert(blit_depth_prog_->ready());
    blit_rgbm_prog_ = sh_.LoadProgram(ctx_, "blit_rgbm", "internal/blit.vert.glsl",
                                      "internal/blit_rgbm.frag.glsl");
    assert(blit_rgbm_prog_->ready());
    blit_mipmap_prog_ = sh_.LoadProgram(ctx_, "blit_mipmap", "internal/blit.vert.glsl",
                                        "internal/blit_mipmap.frag.glsl");
    assert(blit_mipmap_prog_->ready());
    blit_prefilter_prog_ =
        sh_.LoadProgram(ctx_, "blit_prefilter", "internal/blit.vert.glsl",
                        "internal/blit_prefilter.frag.glsl");
    assert(blit_prefilter_prog_->ready());
    blit_project_sh_prog_ =
        sh_.LoadProgram(ctx_, "blit_project_sh_prog", "internal/blit.vert.glsl",
                        "internal/blit_project_sh.frag.glsl");
    assert(blit_project_sh_prog_->ready());

    Ren::CheckError("[InitRendererInternal]: UBO creation", ctx_.log());

    {
        GLuint probe_sample_pbo;
        glGenBuffers(1, &probe_sample_pbo);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, probe_sample_pbo);
        glBufferData(
            GL_PIXEL_PACK_BUFFER,
            GLsizeiptr(4 * probe_sample_buf_.w * probe_sample_buf_.h * sizeof(float)),
            nullptr, GL_DYNAMIC_READ);

        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        probe_sample_pbo_ = uint32_t(probe_sample_pbo);
    }

    Ren::CheckError("[InitRendererInternal]: probe sample PBO", ctx_.log());

    {
        GLuint temp_framebuf;
        glGenFramebuffers(1, &temp_framebuf);

        temp_framebuf_ = uint32_t(temp_framebuf);
    }

    Ren::CheckError("[InitRendererInternal]: temp framebuffer", ctx_.log());

    {                                               // Create timer queries
        for (int i = 0; i < FrameSyncWindow; i++) { // NOLINT
            glGenQueries(TimersCount, queries_[i]);

            for (int j = 0; j < TimersCount; j++) {
                glQueryCounter(queries_[i][j], GL_TIMESTAMP);
            }
        }
    }

    Ren::CheckError("[InitRendererInternal]: timer queries", ctx_.log());

    {
        Ren::BufferRef vtx_buf1 = ctx_.default_vertex_buf1(),
                       vtx_buf2 = ctx_.default_vertex_buf2(),
                       ndx_buf = ctx_.default_indices_buf();

        // Allocate temporary buffer
        temp_buf1_vtx_offset_ = vtx_buf1->AllocRegion(TEMP_BUF_SIZE, "temp buf");
        temp_buf2_vtx_offset_ = vtx_buf2->AllocRegion(TEMP_BUF_SIZE, "temp buf");
        assert(temp_buf1_vtx_offset_ == temp_buf2_vtx_offset_ && "Offsets do not match!");
        temp_buf_ndx_offset_ = ndx_buf->AllocRegion(TEMP_BUF_SIZE, "temp buf");
    }

    Ren::CheckError("[InitRendererInternal]: additional data allocation", ctx_.log());
#endif
}

void Renderer::DestroyRendererInternal() {
    Ren::ILog *log = ctx_.log();

    log->Info("DestroyRendererInternal");
}

uint64_t Renderer::GetGpuTimeBlockingUs() {
#if 0
    GLint64 time = 0;
    glGetInteger64v(GL_TIMESTAMP, &time);
    return (uint64_t)(time / 1000);
#endif
    return 0;
}

void Renderer::BlitPixels(const void *data, const int w, const int h, const Ren::eTexFormat format) {
    using namespace RendererInternal;
#if 0
    if (!temp_tex_ || temp_tex_->params().w != w || temp_tex_->params().h != h ||
        temp_tex_->params().format != format) {
        temp_tex_ = {};

        Ren::Tex2DParams params;
        params.w = w;
        params.h = h;
        params.format = format;
        params.sampling.repeat = Ren::eTexRepeat::ClampToEdge;

        Ren::eTexLoadStatus status;
        temp_tex_ = ctx_.LoadTexture2D("__TEMP_BLIT_TEXTURE__", params, &status);
        assert(status == Ren::eTexLoadStatus::CreatedDefault);
    }

    { // Update texture content
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, temp_tex_->id());

        if (format == Ren::eTexFormat::RawRGBA32F) {
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_FLOAT, data);
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, view_state_.scr_res[0], view_state_.scr_res[1]);

    glBindVertexArray(temp_vao_.id());
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    BlitTexture(-1.0f, 1.0f, 2.0f, -2.0f, temp_tex_);

    glBindVertexArray(0);
#endif
}

void Renderer::BlitPixelsTonemap(const void *data, const int w, const int h, const Ren::eTexFormat format) {
    using namespace RendererInternal;
#if 0
    if (!temp_tex_ || temp_tex_->params().w != w || temp_tex_->params().h != h ||
        temp_tex_->params().format != format) {
        temp_tex_ = {};

        Ren::Tex2DParams params;
        params.w = w;
        params.h = h;
        params.format = format;
        params.sampling.repeat = Ren::eTexRepeat::ClampToEdge;

        Ren::eTexLoadStatus status;
        temp_tex_ = ctx_.LoadTexture2D("__TEMP_BLIT_TEXTURE__", params, &status);
        assert(status == Ren::eTexLoadStatus::CreatedDefault);
    }

    { // Update texture content
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, temp_tex_->id());

        if (format == Ren::eTexFormat::RawRGBA32F) {
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_FLOAT, data);
        }
    }

    assert(format == Ren::eTexFormat::RawRGBA32F);

    Ren::Vec3f avarage_color;
    int sample_count = 0;
    const auto *_data = (const float *)data;

    for (int y = 0; y < h; y += 100) {
        for (int x = 0; x < w; x += 100) {
            int i = y * w + x;
            avarage_color += Ren::MakeVec3(&_data[i * 4 + 0]);
            sample_count++;
        }
    }

    avarage_color /= float(sample_count);

    const float lum = Dot(avarage_color, Ren::Vec3f{0.299f, 0.587f, 0.114f});

    const float alpha = 0.25f;
    reduced_average_ = alpha * lum + (1.0f - alpha) * reduced_average_;

    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    { // prepare downsampled buffer
        Ren::RastState rast_state;
        rast_state.cull_face.enabled = true;

        rast_state.viewport[2] = down_tex_4x_->params().w;
        rast_state.viewport[3] = down_tex_4x_->params().h;

        rast_state.Apply();

        ////

        Ren::Program *cur_program = blit_down_prog_.get();

        // TODO: REMOVE THIS!
        /*unif_shared_data_buf_[cur_buf_chunk_].write_count = 1;

        Graph::RpAllocBuf &unif_shared_data_buf =
            rp_builder_.GetReadBuffer(unif_shared_data_buf_[cur_buf_chunk_]);
        glBindBufferBase(GL_UNIFORM_BUFFER, REN_UB_SHARED_DATA_LOC,
                         (GLuint)unif_shared_data_buf.ref->id());*/

        const PrimDraw::Binding binding = {Ren::eBindTarget::Tex2D, REN_BASE0_TEX_SLOT,
                                           temp_tex_->handle()};

        const PrimDraw::Uniform uniforms[] = {{0, Ren::Vec4f{0.0f, 0.0f, 1.0f, 1.0f}}};

        const Ren::TexHandle down_tex_handle = down_tex_4x_->handle();
        if (!down_tex_4x_fb_.Setup(&down_tex_handle, 1, {}, {}, false)) {
            ctx_.log()->Error("Failed to init down_tex_4x_fb_");
        }

        prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, {down_tex_4x_fb_.id(), 0}, cur_program,
                            &binding, 1, uniforms, 1);
    }

    { // prepare blurred buffer
        Ren::Program *cur_program = blit_gauss_prog_.get();

        // TODO: FIX THIS!

        /*{
            const auto &p = blur_tex_[1]->params();

            Ren::RastState rast_state;
            rast_state.cull_face.enabled = true;

            rast_state.viewport[2] = p.w;
            rast_state.viewport[3] = p.h;

            rast_state.Apply();

            ////

            const PrimDraw::Binding binding = {
                Ren::eBindTarget::Tex2D, REN_BASE0_TEX_SLOT, down_tex_4x_->handle()};

            const PrimDraw::Uniform uniforms[] = {
                {0, Ren::Vec4f{0.0f, 0.0f, float(p.w), float(p.h)}}, {1, 0.0f}};

            Ren::TexHandle blur_tex_handle = blur_tex_[1]->handle();
            blur_tex_fb_[1].Setup(&blur_tex_handle, 1, {}, {}, false);

            prim_draw_.DrawPrim(PrimDraw::ePrim::Quad,
                                {blur_tex_fb_[1].id(), GL_COLOR_BUFFER_BIT}, cur_program,
                                &binding, 1, uniforms, 2);
        }

        {
            const auto &p = blur_tex_[0]->params();

            Ren::RastState rast_state;
            rast_state.cull_face.enabled = true;

            rast_state.viewport[2] = p.w;
            rast_state.viewport[3] = p.h;

            rast_state.Apply();

            ////

            const PrimDraw::Binding binding = {
                Ren::eBindTarget::Tex2D, REN_BASE0_TEX_SLOT, blur_tex_[1]->handle()};

            const PrimDraw::Uniform uniforms[] = {
                {0, Ren::Vec4f{0.0f, 0.0f, float(p.w), float(p.h)}}, {1, 1.0f}};

            prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, {blur_tex_fb_[0].id(), 0},
                                cur_program, &binding, 1, uniforms, 2);
        }*/
    }

    // TODO: FIX THIS!

    /*{ // combine buffers
        Ren::RastState rast_state;
        rast_state.cull_face.enabled = true;

        rast_state.viewport[2] = view_state_.scr_res[2];
        rast_state.viewport[3] = view_state_.scr_res[3];

        rast_state.Apply();

        ////

        Ren::Program *cur_program = blit_combine_prog_.get();

        float exposure = 1.0f / reduced_average_;
        exposure = std::min(exposure, 1000.0f);

        const PrimDraw::Binding bindings[] = {
            {Ren::eBindTarget::Tex2D, REN_BASE0_TEX_SLOT, temp_tex_->handle()},
            {Ren::eBindTarget::Tex2D, REN_BASE1_TEX_SLOT, blur_tex_[0]->handle()}};

        const PrimDraw::Uniform uniforms[] = {
            {0, Ren::Vec4f{0.0f, float(h), float(w), -float(h)}}, // vertically flipped
            {12, 1.0f},
            {13, Ren::Vec2f{float(w), float(h)}},
            {U_GAMMA, 2.2f},
            {U_EXPOSURE, exposure},
            {U_FADE, 0.0f}};

        prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, {0, 0}, cur_program, bindings, 2,
                            uniforms, 6);
    }*/

    glBindVertexArray(0);
#endif
}

void Renderer::BlitBuffer(const float px, const float py, const float sx, const float sy, const FrameBuf &buf,
                          const int first_att, const int att_count, const float multiplier) {
    using namespace RendererInternal;
#if 0
    Ren::BufferRef vtx_buf1 = ctx_.default_vertex_buf1(),
                   vtx_buf2 = ctx_.default_vertex_buf2(),
                   ndx_buf = ctx_.default_indices_buf();

    glBindVertexArray(temp_vao_.id());

    Ren::Program *cur_program = nullptr;

    if (buf.sample_count > 1) {
        cur_program = blit_ms_prog_.get();
    } else {
        cur_program = blit_prog_.get();
    }
    glUseProgram(cur_program->id());

    glUniform4f(0, 0.0f, 0.0f, 1.0f, 1.0f);

    for (int i = first_att; i < first_att + att_count; i++) {
        const float positions[] = {px + float(i - first_att) * sx,     py,
                                   px + float(i - first_att + 1) * sx, py,
                                   px + float(i - first_att + 1) * sx, py + sy,
                                   px + float(i - first_att) * sx,     py + sy};

        if (i == first_att) {
            const float uvs[] = {0.0f,         0.0f,         float(buf.w), 0.0f,
                                 float(buf.w), float(buf.h), 0.0f,         float(buf.h)};

            glBindBuffer(GL_ARRAY_BUFFER, vtx_buf1->id());
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ndx_buf->id());

            glBufferSubData(GL_ARRAY_BUFFER,
                            GLintptr(temp_buf1_vtx_offset_ + sizeof(positions)),
                            sizeof(uvs), uvs);
            glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, GLintptr(temp_buf_ndx_offset_),
                            6 * sizeof(uint16_t), PrimDrawInternal::fs_quad_indices);

            glEnableVertexAttribArray(REN_VTX_POS_LOC);
            glVertexAttribPointer(REN_VTX_POS_LOC, 2, GL_FLOAT, GL_FALSE, 0,
                                  (const GLvoid *)uintptr_t(temp_buf1_vtx_offset_));

            glEnableVertexAttribArray(REN_VTX_UV1_LOC);
            glVertexAttribPointer(
                REN_VTX_UV1_LOC, 2, GL_FLOAT, GL_FALSE, 0,
                (const GLvoid *)uintptr_t(temp_buf1_vtx_offset_ + sizeof(positions)));

            glUniform1f(4, multiplier);
        }

        glBufferSubData(GL_ARRAY_BUFFER, GLintptr(temp_buf1_vtx_offset_),
                        sizeof(positions), positions);

        if (buf.sample_count > 1) {
            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D_MULTISAMPLE, REN_BASE0_TEX_SLOT,
                                       buf.attachments[i].tex->id());
        } else {
            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_BASE0_TEX_SLOT,
                                       buf.attachments[i].tex->id());
        }

        glBindVertexArray(temp_vao_.id());

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT,
                       (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));
    }

    glDisableVertexAttribArray(REN_VTX_POS_LOC);
    glDisableVertexAttribArray(REN_VTX_UV1_LOC);
#endif
}

void Renderer::BlitTexture(const float px, const float py, const float sx, const float sy, const Ren::Tex2DRef &tex,
                           const float multiplier, const bool is_ms) {
    using namespace RendererInternal;
#if 0
    Ren::BufferRef vtx_buf1 = ctx_.default_vertex_buf1(),
                   vtx_buf2 = ctx_.default_vertex_buf2(),
                   ndx_buf = ctx_.default_indices_buf();

    Ren::Program *cur_program = nullptr;

    if (is_ms) {
        cur_program = blit_ms_prog_.get();
    } else {
        cur_program = blit_prog_.get();
    }
    glUseProgram(cur_program->id());

    glUniform4f(0, 0.0f, 0.0f, 1.0f, 1.0f);

    {
        const auto &p = tex->params();

        const float positions[] = {px, py, px + sx, py, px + sx, py + sy, px, py + sy};

        const float uvs[] = {0.0f,       0.0f,       float(p.w), 0.0f,
                             float(p.w), float(p.h), 0.0f,       float(p.h)};

        uint16_t indices[] = {0, 1, 2, 0, 2, 3};

        if (sy < 0.0f) {
            // keep counter-clockwise winding order
            std::swap(indices[0], indices[2]);
            std::swap(indices[3], indices[5]);
        }

        glBindBuffer(GL_ARRAY_BUFFER, vtx_buf1->id());
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ndx_buf->id());

        glBufferSubData(GL_ARRAY_BUFFER, GLintptr(temp_buf1_vtx_offset_),
                        sizeof(positions), positions);
        glBufferSubData(GL_ARRAY_BUFFER,
                        GLintptr(temp_buf1_vtx_offset_ + sizeof(positions)), sizeof(uvs),
                        uvs);
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, GLintptr(temp_buf_ndx_offset_),
                        sizeof(indices), indices);

        glEnableVertexAttribArray(REN_VTX_POS_LOC);
        glVertexAttribPointer(REN_VTX_POS_LOC, 2, GL_FLOAT, GL_FALSE, 0,
                              (const GLvoid *)uintptr_t(temp_buf1_vtx_offset_));

        glEnableVertexAttribArray(REN_VTX_UV1_LOC);
        glVertexAttribPointer(
            REN_VTX_UV1_LOC, 2, GL_FLOAT, GL_FALSE, 0,
            (const GLvoid *)uintptr_t(temp_buf1_vtx_offset_ + sizeof(positions)));

        glUniform1f(4, multiplier);

        if (is_ms) {
            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D_MULTISAMPLE, REN_BASE0_TEX_SLOT,
                                       tex->id());
        } else {
            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_BASE0_TEX_SLOT, tex->id());
        }

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT,
                       (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));
    }

    glDisableVertexAttribArray(REN_VTX_POS_LOC);
    glDisableVertexAttribArray(REN_VTX_UV1_LOC);
#endif
}

void Renderer::BlitToTempProbeFace(const FrameBuf &src_buf, const Ren::ProbeStorage &dst_store, const int face) {
    using namespace RendererInternal;
#if 0
    Ren::BufferRef vtx_buf1 = ctx_.default_vertex_buf1(),
                   vtx_buf2 = ctx_.default_vertex_buf2(),
                   ndx_buf = ctx_.default_indices_buf();

    GLint framebuf_before;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuf_before);

    GLint viewport_before[4];
    glGetIntegerv(GL_VIEWPORT, viewport_before);

    auto framebuf = GLuint(temp_framebuf_);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuf);

    int temp_probe_index = dst_store.reserved_temp_layer();

    auto cube_array = GLuint(dst_store.handle().id);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, cube_array, 0,
                              GLint(temp_probe_index * 6 + face));
    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

    glViewport(0, 0, GLint(dst_store.res()), GLint(dst_store.res()));

    glDisable(GL_BLEND);

    glBindVertexArray(temp_vao_.id());

    glBindBuffer(GL_ARRAY_BUFFER, vtx_buf1->id());
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ndx_buf->id());

    glEnableVertexAttribArray(REN_VTX_POS_LOC);
    glVertexAttribPointer(REN_VTX_POS_LOC, 2, GL_FLOAT, GL_FALSE, 0,
                          (const GLvoid *)uintptr_t(temp_buf1_vtx_offset_));

    glEnableVertexAttribArray(REN_VTX_UV1_LOC);
    glVertexAttribPointer(
        REN_VTX_UV1_LOC, 2, GL_FLOAT, GL_FALSE, 0,
        (const GLvoid *)uintptr_t(temp_buf1_vtx_offset_ + 8 * sizeof(float)));

    glBufferSubData(GL_ARRAY_BUFFER, GLintptr(temp_buf1_vtx_offset_), 8 * sizeof(float),
                    PrimDrawInternal::fs_quad_positions);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, GLintptr(temp_buf_ndx_offset_),
                    6 * sizeof(uint16_t), PrimDrawInternal::fs_quad_indices);

    { // Update first mip level of a cubemap
        Ren::Program *prog = blit_rgbm_prog_.get();
        glUseProgram(prog->id());

        glUniform4f(0, 0.0f, 0.0f, 1.0f, 1.0f);

        const float uvs[] = {0.0f,
                             0.0f,
                             (float)src_buf.w,
                             0.0f,
                             (float)src_buf.w,
                             (float)src_buf.h,
                             0.0f,
                             (float)src_buf.h};

        glBufferSubData(GL_ARRAY_BUFFER,
                        GLintptr(temp_buf1_vtx_offset_ + 8 * sizeof(float)), sizeof(uvs),
                        uvs);

        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, 0, src_buf.attachments[0].tex->id());

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT,
                       (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));
    }

    { // Update rest of mipmaps
        Ren::Program *prog = blit_mipmap_prog_.get();
        glUseProgram(prog->id());

        glUniform4f(0, 0.0f, 0.0f, 1.0f, 1.0f);

        glUniform1f(1, float(temp_probe_index));
        glUniform1i(2, face);

        const float uvs[] = {-1.0f, 1.0f, -1.0, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f};

        glBufferSubData(GL_ARRAY_BUFFER,
                        GLintptr(temp_buf1_vtx_offset_ + 8 * sizeof(float)), sizeof(uvs),
                        uvs);

        ren_glBindTextureUnit_Comp(GL_TEXTURE_CUBE_MAP_ARRAY, 0, cube_array);

        int res = dst_store.res() / 2;
        int level = 1;

        while (level <= dst_store.max_level()) {
            glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, cube_array,
                                      level, GLint(temp_probe_index * 6 + face));
            assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

            glViewport(0, 0, res, res);

            glUniform1f(3, float(level - 1));

            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT,
                           (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));

            res /= 2;
            level++;
        }
    }

    glDisableVertexAttribArray(REN_VTX_POS_LOC);
    glDisableVertexAttribArray(REN_VTX_UV1_LOC);

    glBindVertexArray(0);

    glBindFramebuffer(GL_FRAMEBUFFER, framebuf_before);
    glViewport(viewport_before[0], viewport_before[1], viewport_before[2],
               viewport_before[3]);
#endif
}

void Renderer::BlitPrefilterFromTemp(const Ren::ProbeStorage &dst_store, const int probe_index) {
    using namespace RendererInternal;
#if 0
    Ren::BufferRef vtx_buf1 = ctx_.default_vertex_buf1(),
                   vtx_buf2 = ctx_.default_vertex_buf2(),
                   ndx_buf = ctx_.default_indices_buf();

    GLint framebuf_before;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuf_before);

    GLint viewport_before[4];
    glGetIntegerv(GL_VIEWPORT, viewport_before);

    auto framebuf = GLuint(temp_framebuf_);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuf);

    int temp_probe_index = dst_store.reserved_temp_layer();

    auto cube_array = GLuint(dst_store.handle().id);

    glDisable(GL_BLEND);

    glBindVertexArray(temp_vao_.id());

    glBindBuffer(GL_ARRAY_BUFFER, vtx_buf1->id());
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ndx_buf->id());

    glEnableVertexAttribArray(REN_VTX_POS_LOC);
    glVertexAttribPointer(REN_VTX_POS_LOC, 2, GL_FLOAT, GL_FALSE, 0,
                          (const GLvoid *)uintptr_t(temp_buf1_vtx_offset_));

    glEnableVertexAttribArray(REN_VTX_UV1_LOC);
    glVertexAttribPointer(
        REN_VTX_UV1_LOC, 2, GL_FLOAT, GL_FALSE, 0,
        (const GLvoid *)uintptr_t(temp_buf1_vtx_offset_ + 8 * sizeof(float)));

    glBufferSubData(GL_ARRAY_BUFFER, GLintptr(temp_buf1_vtx_offset_), 8 * sizeof(float),
                    PrimDrawInternal::fs_quad_positions);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, GLintptr(temp_buf_ndx_offset_),
                    6 * sizeof(uint16_t), PrimDrawInternal::fs_quad_indices);

    const float uvs[] = {-1.0f, 1.0f, -1.0, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f};

    glBufferSubData(GL_ARRAY_BUFFER, GLintptr(temp_buf1_vtx_offset_ + 8 * sizeof(float)),
                    sizeof(uvs), uvs);

    Ren::Program *prog = blit_prefilter_prog_.get();
    glUseProgram(prog->id());

    glUniform4f(0, 0.0f, 0.0f, 1.0f, 1.0f);
    glUniform1f(1, float(temp_probe_index));

    ren_glBindTextureUnit_Comp(GL_TEXTURE_CUBE_MAP_ARRAY, 0, cube_array);

    int res = dst_store.res();
    int level = 0;

    while (level <= dst_store.max_level()) {
        glViewport(0, 0, res, res);

        const float roughness = (1.0f / 6.0f) * float(level);
        glUniform1f(3, roughness);

        for (int face = 0; face < 6; face++) {
            glUniform1i(2, face);

            glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, cube_array,
                                      level, GLint(probe_index * 6 + face));
            assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT,
                           (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));
        }

        res /= 2;
        level++;
    }

    glDisableVertexAttribArray(REN_VTX_POS_LOC);
    glDisableVertexAttribArray(REN_VTX_UV1_LOC);

    glBindVertexArray(0);

    glBindFramebuffer(GL_FRAMEBUFFER, framebuf_before);
    glViewport(viewport_before[0], viewport_before[1], viewport_before[2],
               viewport_before[3]);
#endif
}

bool Renderer::BlitProjectSH(const Ren::ProbeStorage &store, const int probe_index, const int iteration,
                             LightProbe &probe) {
    using namespace RendererInternal;
#if 0
    Ren::BufferRef vtx_buf1 = ctx_.default_vertex_buf1(),
                   vtx_buf2 = ctx_.default_vertex_buf2(),
                   ndx_buf = ctx_.default_indices_buf();

    GLint framebuf_before;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuf_before);

    GLint viewport_before[4];
    glGetIntegerv(GL_VIEWPORT, viewport_before);

    glBindFramebuffer(GL_FRAMEBUFFER, probe_sample_buf_.fb);

    glViewport(0, 0, probe_sample_buf_.w, probe_sample_buf_.h);

    glDisable(GL_BLEND);

    glBindVertexArray(temp_vao_.id());

    glBindBuffer(GL_ARRAY_BUFFER, vtx_buf1->id());
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ndx_buf->id());

    glEnableVertexAttribArray(REN_VTX_POS_LOC);
    glVertexAttribPointer(REN_VTX_POS_LOC, 2, GL_FLOAT, GL_FALSE, 0,
                          (const GLvoid *)uintptr_t(temp_buf1_vtx_offset_));

    glEnableVertexAttribArray(REN_VTX_UV1_LOC);
    glVertexAttribPointer(
        REN_VTX_UV1_LOC, 2, GL_FLOAT, GL_FALSE, 0,
        (const GLvoid *)uintptr_t(temp_buf1_vtx_offset_ + 8 * sizeof(float)));

    const float uvs[] = {0.0f,
                         0.0f,
                         float(probe_sample_buf_.w),
                         0.0f,
                         float(probe_sample_buf_.w),
                         float(probe_sample_buf_.h),
                         0.0f,
                         float(probe_sample_buf_.h)};

    glBufferSubData(GL_ARRAY_BUFFER, GLintptr(temp_buf1_vtx_offset_), 8 * sizeof(float),
                    PrimDrawInternal::fs_quad_positions);
    glBufferSubData(GL_ARRAY_BUFFER, GLintptr(temp_buf1_vtx_offset_ + 8 * sizeof(float)),
                    sizeof(uvs), uvs);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, GLintptr(temp_buf_ndx_offset_),
                    6 * sizeof(uint16_t), PrimDrawInternal::fs_quad_indices);

    if (iteration != 0) {
        // Retrieve result of previous read
        glBindBuffer(GL_PIXEL_PACK_BUFFER, GLuint(probe_sample_pbo_));

        Ren::Vec3f sh_coeffs[4];

        auto *pixels = (float *)glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0,
                                                 GLsizeiptr(4) * probe_sample_buf_.w *
                                                     probe_sample_buf_.h * sizeof(float),
                                                 GL_MAP_READ_BIT);
        if (pixels) {
            for (int y = 0; y < probe_sample_buf_.h; y++) {
                for (int x = 0; x < probe_sample_buf_.w; x++) {
                    const int i = (x >= 8) ? ((x >= 16) ? 2 : 1) : 0;

                    sh_coeffs[0][i] += pixels[4 * (y * probe_sample_buf_.w + x) + 0];
                    sh_coeffs[1][i] += pixels[4 * (y * probe_sample_buf_.w + x) + 1];
                    sh_coeffs[2][i] += pixels[4 * (y * probe_sample_buf_.w + x) + 2];
                    sh_coeffs[3][i] += pixels[4 * (y * probe_sample_buf_.w + x) + 3];
                }
            }
            glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
        }
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

        const float inv_weight = 1.0f / float(probe_sample_buf_.h * probe_sample_buf_.h);
        for (Ren::Vec3f &sh_coeff : sh_coeffs) {
            sh_coeff *= inv_weight;
        }

        const float k = 1.0f / float(iteration);
        for (int i = 0; i < 4; i++) {
            const Ren::Vec3f diff = sh_coeffs[i] - probe.sh_coeffs[i];
            probe.sh_coeffs[i] += diff * k;
        }
    }

    if (iteration < 64) {
        { // Sample cubemap and project on sh basis
            Ren::Program *prog = blit_project_sh_prog_.get();
            glUseProgram(prog->id());

            glUniform4f(0, 0.0f, 0.0f, 1.0f, 1.0f);

            ren_glBindTextureUnit_Comp(GL_TEXTURE_CUBE_MAP_ARRAY, 1, store.handle().id);

            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, 1, rand2d_8x8_->id());

            glUniform1f(1, float(probe_index));
            glUniform1i(2, iteration);

            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT,
                           (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));
        }

        { // Start readback from buffer (result will be retrieved at the start of next
          // iteration)
            glReadBuffer(GL_COLOR_ATTACHMENT0);
            glBindBuffer(GL_PIXEL_PACK_BUFFER, GLuint(probe_sample_pbo_));

            glReadPixels(0, 0, probe_sample_buf_.w, probe_sample_buf_.h, GL_RGBA,
                         GL_FLOAT, nullptr);

            glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        }
    }

    glDisableVertexAttribArray(REN_VTX_POS_LOC);
    glDisableVertexAttribArray(REN_VTX_UV1_LOC);

    glBindVertexArray(0);

    glBindFramebuffer(GL_FRAMEBUFFER, framebuf_before);
    glViewport(viewport_before[0], viewport_before[1], viewport_before[2],
               viewport_before[3]);

    return iteration == 64;
#endif
    return false;
}

#undef _AS_STR
#undef AS_STR
