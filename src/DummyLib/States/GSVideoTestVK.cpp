#include "GSVideoTest.h"

#include <vtune/ittnotify.h>
extern __itt_domain *__g_itt_domain; // NOLINT

#include <Ren/Context.h>
//#include <Ren/GL.h>
#include <Ren/Utils.h>

namespace GSVideoTestInternal {
const bool VerboseLogging = false;
} // namespace GSVideoTestInternal

void GSVideoTest::InitVideoTextures() {
    for (int tx = 0; tx < 5; tx++) {
        if (!vp_[tx].initialized()) {
            continue;
        }
        const Eng::VideoPlayer &vp = vp_[tx];

        char name_buf[128];

        { // create Y-channel texture (full res)
            Ren::Tex2DParams params;
            params.w = vp.w();
            params.h = vp.h();
            params.format = Ren::eTexFormat::RawR8;
            params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

            for (int j = 0; j < TextureSyncWindow; j++) {
                sprintf(name_buf, "__video_Y_texture_%i_%i__", tx, j);
                y_tex_[tx][j] = ren_ctx_->textures().Add(name_buf, ren_ctx_->api_ctx(), params,
                                                         ren_ctx_->default_mem_allocs(), log_);
            }
        }

        { // register U-channel texture (half res)
            Ren::Tex2DParams params;
            params.w = vp.w() / 2;
            params.h = vp.h() / 2;
            params.format = Ren::eTexFormat::RawRG88;
            params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

            for (int j = 0; j < TextureSyncWindow; j++) {
                sprintf(name_buf, "__video_UV_texture_%i_%i__", tx, j);
                uv_tex_[tx][j] = ren_ctx_->textures().Add(name_buf, ren_ctx_->api_ctx(), params,
                                                          ren_ctx_->default_mem_allocs(), log_);
            }
        }

        { // register material
            orig_vid_mat_[tx] = scene_manager_->scene_data().materials.FindByName("wall_picture_yuv.txt");
            if (!orig_vid_mat_[tx]) {
                log_->Error("Failed to find material wall_picture");
                return;
            }

            auto pipelines = orig_vid_mat_[tx]->pipelines;
            auto textures = orig_vid_mat_[tx]->textures;
            auto samplers = orig_vid_mat_[tx]->samplers;
            auto params = orig_vid_mat_[tx]->params;

            sprintf(name_buf, "__video_texture_material_%i__", tx);
            assert(false && "Temrorarily broken!");
            /*vid_mat_[tx] = scene_manager_->scene_data().materials.Add(
                name_buf, orig_vid_mat_[tx]->flags() | uint32_t(Ren::eMatFlags::TaaResponsive), programs.data(),
                int(programs.size()), textures.data(), samplers.data(), int(textures.size()), params.data(),
                int(params.size()), log_.get());*/
        }

#if !defined(__ANDROID__)
        { // init PBOs
            const uint32_t y_buf_size = TextureSyncWindow * vp.w() * vp.h(),
                           uv_buf_size = TextureSyncWindow * 2 * (vp.w() / 2) * (vp.h() / 2);

            // y_sbuf_[tx].Resize(y_buf_size, ren_ctx_->capabilities.persistent_buf_mapping);
            // uv_sbuf_[tx].Resize(uv_buf_size, ren_ctx_->capabilities.persistent_buf_mapping);
        }
#endif
    }
}

void GSVideoTest::DestroyVideoTextures() {
    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < TextureSyncWindow; j++) {
            WaitVideoTextureUpdated(i, j);
        }
        y_sbuf_[i].Free();
        uv_sbuf_[i].Free();
    }
}

void GSVideoTest::SetVideoTextureFence(const int tex_index, const int frame_index) {
    assert(!after_tex_update_fences_[tex_index][frame_index]);
    after_tex_update_fences_[tex_index][frame_index] = Ren::MakeFence();

    if (GSVideoTestInternal::VerboseLogging) { // NOLINT
        log_->Info("Setting VT fence %tx", frame_index);
    }
}

void GSVideoTest::InsertVideoTextureBarrier(const int tex_index, const int frame_index) {
    if (after_tex_update_fences_[tex_index][frame_index]) {
        after_tex_update_fences_[tex_index][frame_index].WaitSync();
    }
}

void GSVideoTest::WaitVideoTextureUpdated(const int tex_index, const int frame_index) {
    if (after_tex_update_fences_[tex_index][frame_index]) {
        if (after_tex_update_fences_[tex_index][frame_index].ClientWaitSync() == Ren::WaitResult::Fail) {
            log_->Error("[Renderer::DrawObjectsInternal]: Wait failed!");
        }
        assert(!after_tex_update_fences_[tex_index][frame_index]);

        if (GSVideoTestInternal::VerboseLogging) { // NOLINT
            log_->Info("Waiting PBO fence %tx", frame_index);
        }
    }
}

void GSVideoTest::FlushGPUCommands() {
#if 0
    glFlush();
#endif
}
