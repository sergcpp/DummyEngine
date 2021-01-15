#include "GSVideoTest.h"

#ifdef ENABLE_ITT_API
#include <vtune/ittnotify.h>
extern __itt_domain *__g_itt_domain; // NOLINT
#endif

#include <Ren/Context.h>
#include <Ren/GL.h>
#include <Ren/Utils.h>

namespace GSVideoTestInternal {
#ifdef ENABLE_ITT_API
__itt_string_handle *itt_update_tex_str = __itt_string_handle_create("Update Textures");
#endif
const bool VerboseLogging = false;
} // namespace GSVideoTestInternal

//#define ORPHAN_TEXTURES

void GSVideoTest::InitVideoTextures() {
    for (int tx = 0; tx < 5; tx++) {
        if (!vp_[tx].initialized()) {
            continue;
        }
        const VideoPlayer &vp = vp_[tx];

        std::vector<uint8_t> temp_buf(vp.w() * vp.h(), 0);

        char name_buf[128];

        { // create Y-channel texture (full res)
            Ren::Tex2DParams params;
            params.w = vp.w();
            params.h = vp.h();
            params.format = Ren::eTexFormat::RawR8;
            params.filter = Ren::eTexFilter::BilinearNoMipmap;
            params.repeat = Ren::eTexRepeat::ClampToEdge;
#ifdef ORPHAN_TEXTURES
            params.flags = Ren::TexMutable;
#endif

            for (int j = 0; j < TextureSyncWindow; j++) {
                sprintf(name_buf, "__video_Y_texture_%i_%i__", tx, j);
                Ren::eTexLoadStatus load_status;
                y_tex_[tx][j] =
                    ren_ctx_->textures().Add(name_buf, &temp_buf[0], int(temp_buf.size()),
                                             params, &load_status, log_.get());
            }
        }

        { // register U-channel texture (half res)
            Ren::Tex2DParams params;
            params.w = vp.w() / 2;
            params.h = vp.h() / 2;
            params.format = Ren::eTexFormat::RawRG88;
            params.filter = Ren::eTexFilter::BilinearNoMipmap;
            params.repeat = Ren::eTexRepeat::ClampToEdge;
#ifdef ORPHAN_TEXTURES
            params.flags = Ren::TexMutable;
#endif

            for (int j = 0; j < TextureSyncWindow; j++) {
                sprintf(name_buf, "__video_UV_texture_%i_%i__", tx, j);
                Ren::eTexLoadStatus load_status;
                uv_tex_[tx][j] = ren_ctx_->textures().Add(name_buf, &temp_buf[0],
                                                         int(temp_buf.size() / 2), params,
                                                         &load_status, log_.get());
            }
        }

        { // register material
            Ren::eMatLoadStatus status;
            orig_vid_mat_[tx] = ren_ctx_->LoadMaterial("wall_picture_yuv.txt", nullptr,
                                                      &status, nullptr, nullptr);
            if (status != Ren::eMatLoadStatus::Found) {
                log_->Error("Failed to find material wall_picture");
                return;
            }

            Ren::ProgramRef programs[Ren::MaxMaterialProgramCount];
            for (int j = 0; j < Ren::MaxMaterialProgramCount; j++) {
                programs[j] = orig_vid_mat_[tx]->programs[j];
            }

            Ren::Tex2DRef textures[Ren::MaxMaterialTextureCount];
            for (int j = 0; j < Ren::MaxMaterialTextureCount; j++) {
                textures[j] = orig_vid_mat_[tx]->textures[j];
            }

            Ren::Vec4f params[Ren::MaxMaterialParamCount];
            for (int j = 0; j < Ren::MaxMaterialParamCount; j++) {
                params[j] = orig_vid_mat_[tx]->params[j];
            }

            sprintf(name_buf, "__video_texture_material_%i__", tx);
            vid_mat_[tx] = ren_ctx_->materials().Add(
                name_buf,
                orig_vid_mat_[tx]->flags() | uint32_t(Ren::eMaterialFlags::TaaResponsive),
                programs, textures, params, log_.get());
        }

#if !defined(__ANDROID__)
        { // init PBOs
            const GLbitfield BufferRangeMapFlags =
                GLbitfield(GL_MAP_WRITE_BIT) | GLbitfield(GL_MAP_PERSISTENT_BIT) |
                GLbitfield(GL_MAP_INVALIDATE_RANGE_BIT) |
                GLbitfield(GL_MAP_UNSYNCHRONIZED_BIT) |
                GLbitfield(GL_MAP_FLUSH_EXPLICIT_BIT);

            const size_t y_buf_size = TextureSyncWindow * vp.w() * vp.h(),
                         uv_buf_size =
                             TextureSyncWindow * 2 * (vp.w() / 2) * (vp.h() / 2);

            GLuint pbo_ids[2];
            glGenBuffers(2, pbo_ids);

            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo_ids[0]);
            glBufferStorage(GL_PIXEL_UNPACK_BUFFER, y_buf_size, nullptr,
                            GLbitfield(GL_MAP_WRITE_BIT) |
                                GLbitfield(GL_MAP_PERSISTENT_BIT));

            y_pbo_[tx] = uint32_t(pbo_ids[0]);
            if (ren_ctx_->capabilities.persistent_buf_mapping) {
                y_ptr_[tx] = (uint8_t *)glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0,
                                                        y_buf_size, BufferRangeMapFlags);
            }

            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo_ids[1]);
            glBufferStorage(GL_PIXEL_UNPACK_BUFFER, uv_buf_size, nullptr,
                            GLbitfield(GL_MAP_WRITE_BIT) |
                                GLbitfield(GL_MAP_PERSISTENT_BIT));

            uv_pbo_[tx] = uint32_t(pbo_ids[1]);
            if (ren_ctx_->capabilities.persistent_buf_mapping) {
                uv_ptr_[tx] = (uint8_t *)glMapBufferRange(
                    GL_PIXEL_UNPACK_BUFFER, 0, uv_buf_size, BufferRangeMapFlags);
            }

            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        }
#endif
    }
}

void GSVideoTest::DestroyVideoTextures() {
    for (int i = 0; i < 5; i++) {
        if (y_pbo_[i]) {
            auto y_pbo = GLuint(y_pbo_[i]);
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, y_pbo);
            if (y_ptr_[i]) {
                glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
                y_ptr_[i] = nullptr;
            }
            glDeleteBuffers(1, &y_pbo);
            y_pbo_[i] = 0;
        }
        if (uv_pbo_[i]) {
            auto uv_pbo = GLuint(uv_pbo_[i]);
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, uv_pbo);
            if (uv_ptr_[i]) {
                glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
                uv_ptr_[i] = nullptr;
            }
            glDeleteBuffers(1, &uv_pbo);
            uv_pbo_[i] = 0;
        }
        for (int j = 0; j < TextureSyncWindow; j++) {
            WaitVideoTextureUpdated(i, j);
        }
    }
}

void GSVideoTest::UpdatePBOWithDecodedFrame(const int tex_index, const int frame_index) {
    assert(vp_[tex_index].initialized());

    const int tex_w = vp_[tex_index].w();
    const int tex_h = vp_[tex_index].h();

    const GLbitfield BufferRangeMapFlags =
        GLbitfield(GL_MAP_WRITE_BIT) | GLbitfield(GL_MAP_INVALIDATE_RANGE_BIT) |
        GLbitfield(GL_MAP_UNSYNCHRONIZED_BIT) | GLbitfield(GL_MAP_FLUSH_EXPLICIT_BIT);

    { // copy Y plane
        int w, h, stride;
        const uint8_t *y_img = vp_[tex_index].GetImagePtr(eYUVComp::Y, w, h, stride);
        if (y_img && w == tex_w && h == tex_h) {
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, GLuint(y_pbo_[tex_index]));
            if (y_ptr_[tex_index]) { // persistent mapping case
                const int range_offset = frame_index * w * h;
                for (int y = 0; y < h; y++) {
                    memcpy(&y_ptr_[tex_index][range_offset + y * w], &y_img[y * stride],
                           w);
                }
                glFlushMappedBufferRange(GL_PIXEL_UNPACK_BUFFER, range_offset,
                                         GLsizeiptr(w * h));
            } else { // non-persistent mapping case
                auto *pinned_mem = (uint8_t *)glMapBufferRange(
                    GL_PIXEL_UNPACK_BUFFER, GLintptr(frame_index * w * h),
                    GLsizeiptr(w * h), BufferRangeMapFlags);
                if (pinned_mem) {
                    for (int y = 0; y < h; y++) {
                        memcpy(&pinned_mem[y * w], &y_img[y * stride], w);
                    }
                    glFlushMappedBufferRange(GL_PIXEL_UNPACK_BUFFER, 0,
                                             GLsizeiptr(w * h));
                    glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
                }
            }
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        }
    }

    { // copy UV planes
        int u_w, u_h, u_stride;
        const uint8_t *u_img =
            vp_[tex_index].GetImagePtr(eYUVComp::U, u_w, u_h, u_stride);
        int v_w, v_h, v_stride;
        const uint8_t *v_img =
            vp_[tex_index].GetImagePtr(eYUVComp::V, v_w, v_h, v_stride);
        if (u_img && u_w == (tex_w / 2) && u_h == (tex_h / 2) && v_img &&
            v_w == (tex_w / 2) && v_h == (tex_h / 2)) {
            const int range_offset = 2 * frame_index * u_w * u_h;

            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, GLuint(uv_pbo_[tex_index]));
            if (uv_ptr_[tex_index]) { // persistent mapping case
                uint8_t *uv_dst = &uv_ptr_[tex_index][range_offset];

                Ren::InterleaveUVChannels_16px(u_img, v_img, u_stride, v_stride, u_w, u_h,
                                               uv_dst);

                glFlushMappedBufferRange(GL_PIXEL_UNPACK_BUFFER, range_offset,
                                         GLsizeiptr(u_w * u_h));
            } else { // non-persistent mapping case
                auto *pinned_mem = (uint8_t *)glMapBufferRange(
                    GL_PIXEL_UNPACK_BUFFER, GLintptr(range_offset),
                    GLsizeiptr(2 * u_w * u_h), BufferRangeMapFlags);
                if (pinned_mem) {
                    Ren::InterleaveUVChannels_16px(u_img, v_img, u_stride, v_stride, u_w,
                                                   u_h, pinned_mem);

                    glFlushMappedBufferRange(GL_PIXEL_UNPACK_BUFFER, 0,
                                             GLsizeiptr(2 * u_w * u_h));
                    glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
                }
            }
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        }
    }
}

void GSVideoTest::SetVideoTextureFence(const int tex_index, const int frame_index) {
    assert(!after_tex_update_fences_[tex_index][frame_index]);
    after_tex_update_fences_[tex_index][frame_index] =
        glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

    if (GSVideoTestInternal::VerboseLogging) { // NOLINT
        log_->Info("Setting VT fence %tx", frame_index);
    }
}

void GSVideoTest::InsertVideoTextureBarrier(const int tex_index, const int frame_index) {
    if (after_tex_update_fences_[tex_index][frame_index]) {
        auto sync =
            reinterpret_cast<GLsync>(after_tex_update_fences_[tex_index][frame_index]);
        glWaitSync(sync, 0, GL_TIMEOUT_IGNORED);
    }
}

void GSVideoTest::WaitVideoTextureUpdated(const int tex_index, const int frame_index) {
    if (after_tex_update_fences_[tex_index][frame_index]) {
        auto sync =
            reinterpret_cast<GLsync>(after_tex_update_fences_[tex_index][frame_index]);
        const GLenum res = glClientWaitSync(sync, 0, 1000000000);
        if (res != GL_ALREADY_SIGNALED && res != GL_CONDITION_SATISFIED) {
            log_->Error("[Renderer::DrawObjectsInternal]: Wait failed!");
        }
        glDeleteSync(sync);
        after_tex_update_fences_[tex_index][frame_index] = nullptr;

        if (GSVideoTestInternal::VerboseLogging) { // NOLINT
            log_->Info("Waiting PBO fence %tx", frame_index);
        }
    }
}

void GSVideoTest::FlushGPUCommands() { glFlush(); }

void GSVideoTest::UpdateVideoTextureData(const int tex_index, const int frame_index) {
    using namespace GSVideoTestInternal;

    assert(vp_[tex_index].initialized());

    if (GSVideoTestInternal::VerboseLogging) { // NOLINT
        log_->Info("Updating texture %tx", frame_index);
    }

#ifdef ENABLE_ITT_API
    __itt_task_begin(__g_itt_domain, __itt_null, __itt_null, itt_update_tex_str);
#endif

    const int tex_w = vp_[tex_index].w();
    const int tex_h = vp_[tex_index].h();

    const size_t y_buf_chunk_size = tex_w * tex_h,
                 uv_buf_chunk_size = 2 * (tex_w / 2) * (tex_h / 2);

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, GLuint(y_pbo_[tex_index]));
    glFlushMappedBufferRange(GL_PIXEL_UNPACK_BUFFER, frame_index * y_buf_chunk_size,
                             GLsizeiptr(y_buf_chunk_size));

    glBindTexture(GL_TEXTURE_2D, GLuint(y_tex_[tex_index][frame_index]->id()));
#ifdef ORPHAN_TEXTURES
    glTexImage2D(
        GL_TEXTURE_2D, 0, Ren::GLInternalFormatFromTexFormat(Ren::eTexFormat::RawR8),
        tex_w, tex_h, 0, Ren::GLFormatFromTexFormat(Ren::eTexFormat::RawR8),
        GL_UNSIGNED_BYTE,
        reinterpret_cast<const void *>(uintptr_t(frame_index * y_buf_chunk_size)));
#else
    glTexSubImage2D(
        GL_TEXTURE_2D, 0, 0, 0, tex_w, tex_h,
        Ren::GLFormatFromTexFormat(Ren::eTexFormat::RawR8), GL_UNSIGNED_BYTE,
        reinterpret_cast<const void *>(uintptr_t(frame_index * y_buf_chunk_size)));
#endif

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, GLuint(uv_pbo_[tex_index]));
    glFlushMappedBufferRange(GL_PIXEL_UNPACK_BUFFER, frame_index * uv_buf_chunk_size,
                             GLsizeiptr(uv_buf_chunk_size));

    glBindTexture(GL_TEXTURE_2D, GLuint(uv_tex_[tex_index][frame_index]->id()));
#ifdef ORPHAN_TEXTURES
    glTexImage2D(
        GL_TEXTURE_2D, 0, Ren::GLInternalFormatFromTexFormat(Ren::eTexFormat::RawRG88),
        tex_w / 2, tex_h / 2, 0, Ren::GLFormatFromTexFormat(Ren::eTexFormat::RawRG88),
        GL_UNSIGNED_BYTE,
        reinterpret_cast<const void *>(uintptr_t(frame_index * uv_buf_chunk_size)));
#else
    glTexSubImage2D(
        GL_TEXTURE_2D, 0, 0, 0, tex_w / 2, tex_h / 2,
        Ren::GLFormatFromTexFormat(Ren::eTexFormat::RawRG88), GL_UNSIGNED_BYTE,
        reinterpret_cast<const void *>(uintptr_t(frame_index * uv_buf_chunk_size)));
#endif

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

#ifdef ENABLE_ITT_API
    __itt_task_end(__g_itt_domain);
#endif
}