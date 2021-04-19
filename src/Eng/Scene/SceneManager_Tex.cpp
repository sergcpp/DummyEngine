#include "SceneManager.h"

#include <Ren/Context.h>
#include <Ren/Utils.h>

#ifdef ENABLE_ITT_API
#include <vtune/ittnotify.h>
extern __itt_domain *__g_itt_domain;
#endif

namespace SceneManagerConstants {
#if defined(__ANDROID__)
extern const char *TEXTURES_PATH;
#else
extern const char *TEXTURES_PATH;
#endif

#ifdef ENABLE_ITT_API
__itt_string_handle *itt_read_file_str = __itt_string_handle_create("ReadFile");
#endif
} // namespace SceneManagerConstants

namespace SceneManagerInternal {
void ParseDDSHeader(const Ren::DDSHeader &hdr, Ren::Tex2DParams &params, Ren::ILog *log) {
    params.w = uint16_t(hdr.dwWidth);
    params.h = uint16_t(hdr.dwHeight);
    params.mip_count = uint8_t(hdr.dwMipMapCount);

    const int px_format = int(hdr.sPixelFormat.dwFourCC >> 24u) - '0';
    switch (px_format) {
    case 1:
        params.format = Ren::eTexFormat::Compressed_DXT1;
        params.block = Ren::eTexBlock::_4x4;
        break;
    case 3:
        params.format = Ren::eTexFormat::Compressed_DXT3;
        params.block = Ren::eTexBlock::_4x4;
        break;
    case 5:
        params.format = Ren::eTexFormat::Compressed_DXT5;
        params.block = Ren::eTexBlock::_4x4;
        break;
    default:
        log->Error("Unknown DDS pixel format %i", px_format);
        return;
    }
}
} // namespace SceneManagerInternal

#define NEXT_REQ_NDX(x) ((x + 1) % MaxSimultaneousRequests)
#define PREV_REQ_NDX(x) ((MaxSimultaneousRequests + x - 1) % MaxSimultaneousRequests)

void SceneManager::TextureLoaderProc() {
    using namespace SceneManagerConstants;
    using namespace SceneManagerInternal;

#ifdef ENABLE_ITT_API
    __itt_thread_set_name("Texture loader");
#endif

    for (;;) {
        TextureRequestPending &req = pending_textures_[pending_textures_head_];

        {
            std::unique_lock<std::mutex> lock(texture_requests_lock_);
            texture_loader_cnd_.wait(lock, [this] {
                return texture_loader_stop_ ||
                       !requested_textures_.empty() &&
                           NEXT_REQ_NDX(pending_textures_head_) != pending_textures_tail_;
            });
            if (texture_loader_stop_) {
                break;
            }

            static_cast<TextureRequest &>(req) = std::move(requested_textures_.front());
            requested_textures_.pop_front();
        }

#ifdef ENABLE_ITT_API
        __itt_task_begin(__g_itt_domain, __itt_null, __itt_null, itt_read_file_str);
#endif
        req.buf.set_data_len(0);
        req.mip_offset_to_init = -1;
        bool read_success;

        size_t read_offset = 0, read_size = 0;

        char path_buf[4096];
        strcpy(path_buf, SceneManagerConstants::TEXTURES_PATH);
        strcat(path_buf, req.ref->name().c_str());

        if (req.ref->name().EndsWith(".dds") || req.ref->name().EndsWith(".DDS")) {
            if (req.orig_params.format == Ren::eTexFormat::Undefined) {
                Ren::DDSHeader header;

                size_t data_size = sizeof(Ren::DDSHeader);
                read_success = texture_reader_.ReadFileBlocking(
                    path_buf, 0 /* file_offset */, sizeof(Ren::DDSHeader), &header,
                    data_size);
                read_success &= (data_size == sizeof(Ren::DDSHeader));

                if (read_success) {
                    ParseDDSHeader(header, req.orig_params, ren_ctx_.log());
                    req.mip_count = int(header.dwMipMapCount);
                }
            }

            const int max_load_w = std::max(req.ref->params().w * 2, 64);
            const int max_load_h = std::max(req.ref->params().h * 2, 64);

            read_offset += sizeof(Ren::DDSHeader);
            int w = int(req.orig_params.w), h = int(req.orig_params.h);
            for (int i = 0; i < req.mip_count; i++) {
                const int data_len = Ren::GetMipDataLenBytes(w, h, req.orig_params.format,
                                                             req.orig_params.block);

                if ((w <= max_load_w && h <= max_load_h) || i == req.mip_count - 1) {
                    if (req.mip_offset_to_init == -1) {
                        req.mip_offset_to_init = i;
                        req.mip_count_to_init = 0;
                    }
                    if (w > req.ref->params().w || h > req.ref->params().h || !req.ref->ready()) {
                        req.mip_count_to_init++;
                        read_size += data_len;
                    }
                } else {
                    read_offset += data_len;
                }

                w = std::max(w / 2, 1);
                h = std::max(h / 2, 1);
            }
        } else if (req.ref->name().EndsWith(".ktx") || req.ref->name().EndsWith(".KTX")) {
            assert(false && "Not implemented!");
            read_offset += sizeof(Ren::KTXHeader);
        }

        { // load next mip levels
            assert(req.ref->params().w == req.orig_params.w ||
                   req.ref->params().h != req.orig_params.h);
        }

        if (read_size) {
            read_success = texture_reader_.ReadFileNonBlocking(
                path_buf, read_offset, read_size, req.buf, req.ev);
            assert(req.buf.data_len() == read_size);
        }

#ifdef ENABLE_ITT_API
        __itt_task_end(__g_itt_domain);
#endif

        std::lock_guard<std::mutex> _(texture_requests_lock_);
        pending_textures_head_ = NEXT_REQ_NDX(pending_textures_head_);
    }
}

void SceneManager::ProcessPendingTextures(const int portion_size) {
    using namespace SceneManagerConstants;

    for (int i = 0; i < portion_size; i++) {
        TextureRequestPending *req = nullptr;
        {
            std::lock_guard<std::mutex> _(texture_requests_lock_);
            if (pending_textures_head_ != pending_textures_tail_) {
                req = &pending_textures_[pending_textures_tail_];
            }
        }

        if (req) {
            const Ren::String &tex_name = req->ref->name();

            size_t bytes_read;
            const Sys::eFileReadResult res =
                req->ev.GetResult(false /* block */, &bytes_read);

            if (res == Sys::eFileReadResult::Successful) {
                const uint64_t t1_us = Sys::GetTimeUs();

                int w = std::max(req->orig_params.w >> req->mip_offset_to_init, 1);
                int h = std::max(req->orig_params.h >> req->mip_offset_to_init, 1);

                uint16_t initialized_mips = req->ref->initialized_mips();
                int last_initialized_mip = 0;
                while (initialized_mips >>= 1) {
                    ++last_initialized_mip;
                }

                const Ren::Tex2DParams &p = req->ref->params();
                req->ref->Realloc(w, h, last_initialized_mip + 1 + req->mip_count_to_init,
                                  1 /* samples */, req->orig_params.format,
                                  (p.flags & Ren::TexSRGB) != 0, ren_ctx_.log());

                const uint8_t *p_buf = req->buf.data();
                for (int i = req->mip_offset_to_init;
                     i < req->mip_offset_to_init + req->mip_count_to_init; i++) {
                    const int data_len = Ren::GetMipDataLenBytes(
                        w, h, req->orig_params.format, req->orig_params.block);

                    req->ref->SetSubImage(i - req->mip_offset_to_init, 0, 0, w, h,
                                          req->orig_params.format, p_buf, data_len);

                    w = std::max(w / 2, 1);
                    h = std::max(h / 2, 1);
                    p_buf += data_len;
                }

                const uint64_t t2_us = Sys::GetTimeUs();

                ren_ctx_.log()->Info("Texture %s loaded (%.3f ms)", tex_name.c_str(),
                                     double(t2_us - t1_us) * 0.001);
            } else if (res == Sys::eFileReadResult::Failed) {
                ren_ctx_.log()->Error("Error loading %s", tex_name.c_str());
            }

            if (res != Sys::eFileReadResult::Pending) {
                std::lock_guard<std::mutex> _(texture_requests_lock_);
                if (res == Sys::eFileReadResult::Successful &&
                    (req->ref->params().w != req->orig_params.w ||
                     req->ref->params().h != req->orig_params.h)) {
                    // send texture to be processed further (for next mip levels)
                    requested_textures_.emplace_back(
                        static_cast<TextureRequest &&>(*req));
                } else {
                    static_cast<TextureRequest &>(*req) = {};
                }
                pending_textures_tail_ = NEXT_REQ_NDX(pending_textures_tail_);
                texture_loader_cnd_.notify_one();
            } else {
                break;
            }
        }
    }
}

#undef NEXT_REQ_NDX
#undef PREV_REQ_NDX
