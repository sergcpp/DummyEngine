#include "SceneManager.h"

#ifdef ENABLE_ITT_API
#include <vtune/ittnotify.h>
extern __itt_domain *__g_itt_domain;
#endif

namespace SceneManagerConstants {
#ifdef ENABLE_ITT_API
__itt_string_handle *itt_read_file_str = __itt_string_handle_create("ReadFile");
#endif
} // namespace SceneManagerConstants

#define NEXT_REQ_NDX(x) ((x + 1) % MaxSimultaneousRequests)
#define PREV_REQ_NDX(x) ((MaxSimultaneousRequests + x - 1) % MaxSimultaneousRequests)

void SceneManager::TextureLoaderProc() {
    using namespace SceneManagerConstants;

    for (;;) {
        Ren::Tex2DRef ref;

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

            ref = std::move(requested_textures_.front());
            requested_textures_.pop_front();
        }

#ifdef ENABLE_ITT_API
        __itt_task_begin(__g_itt_domain, __itt_null, __itt_null, itt_read_file_str);
#endif

        TextureRequest &req = pending_textures_[pending_textures_head_];
        assert(!req.ref);
        bool result = texture_reader_.ReadFile(ref->name().c_str(), req.buf_size,
                                               req.buf.get(), req.data_size);
        if (!result && req.data_size) {
            while (req.buf_size < req.data_size) {
                req.buf_size *= 2;
            }
            req.buf.reset(new uint8_t[req.buf_size]);

            result = texture_reader_.ReadFile(ref->name().c_str(), req.buf_size,
                                              req.buf.get(), req.data_size);
        }

#ifdef ENABLE_ITT_API
        __itt_task_end(__g_itt_domain);
#endif

        std::lock_guard<std::mutex> _(texture_requests_lock_);
        req.ref = std::move(ref);
        if (!result) {
            req.data_size = 0;
        }
        pending_textures_head_ = NEXT_REQ_NDX(pending_textures_head_);
    }
}

void SceneManager::ProcessPendingTextures(const int portion_size) {
    for (int i = 0; i < portion_size; i++) {
        TextureRequest *req = nullptr;
        int textures_pending = 0;

        {
            std::lock_guard<std::mutex> _(texture_requests_lock_);
            if (pending_textures_head_ != pending_textures_tail_) {
                req = &pending_textures_[pending_textures_tail_];
                if (pending_textures_head_ > pending_textures_tail_) {
                    textures_pending = pending_textures_head_ - pending_textures_tail_;
                } else {
                    textures_pending = pending_textures_head_ + MaxSimultaneousRequests -
                                       pending_textures_tail_;
                }
            }
        }

        if (req) {
            const char *tex_name = req->ref->name().c_str();

            if (req->data_size) {
                Ren::Texture2DParams p = req->ref->params();
                if (strstr(tex_name, ".tga_rgbe")) {
                    p.filter = Ren::eTexFilter::BilinearNoMipmap;
                    p.repeat = Ren::eTexRepeat::ClampToEdge;
                } else {
                    p.filter = Ren::eTexFilter::Trilinear;
                    if (p.flags & Ren::TexNoRepeat) {
                        p.repeat = Ren::eTexRepeat::ClampToEdge;
                    } else {
                        p.repeat = Ren::eTexRepeat::Repeat;
                    }
                }

                req->ref->Init(req->buf.get(), int(req->data_size), p, nullptr,
                               ren_ctx_.log());

                const int count = textures_pending - 1 + int(requested_textures_.size());
                ren_ctx_.log()->Info("Texture %s loaded (%i left)", tex_name, count);
            } else {
                ren_ctx_.log()->Error("Error loading %s", tex_name);
            }

            {
                std::lock_guard<std::mutex> _(texture_requests_lock_);
                req->ref = {};
                pending_textures_tail_ = NEXT_REQ_NDX(pending_textures_tail_);
                texture_loader_cnd_.notify_one();
            }
        }
    }
}

#undef NEXT_REQ_NDX
#undef PREV_REQ_NDX
