#include "GSUITest3.h"

#include <Ren/GL.h>

#include "../widgets/PagedReader.h"

namespace GSUITest3Internal {
extern const Ren::Vec2f page_corners_uvs[];
extern const int page_order_indices[][4];
} // namespace GSUITest3Internal

void GSUITest3::InitBookMaterials() {
    /*assert(page_buf_.w != -1 && page_buf_.h != -1 && "Page framebuffer is not initialized!");

    { // register framebuffer texture
        Ren::Tex2DParams params;
        params.w = page_buf_.w;
        params.h = page_buf_.h;
        params.format = page_buf_.attachments[0].desc.format;
        params.sampling.filter = page_buf_.attachments[0].desc.filter;
        params.sampling.wrap = page_buf_.attachments[0].desc.wrap;

        // prevent texture deletion
        params.flags = Ren::eTexFlagBits::NoOwnership;

        page_tex_ =
            ren_ctx_->textures().Add("__book_page_texture__", ren_ctx_->api_ctx(), page_buf_.attachments[0].tex->id(),
                                     ren_ctx_->default_mem_allocs(), params, log_.get());
    }

    { // replace texture
        orig_page_mat_ = scene_manager_->scene_data().materials.FindByName("book/book_page0.mat");
        if (!orig_page_mat_) {
            log_->Error("Failed to find material book/book_page0");
            return;
        }

        orig_page_mat_->textures[2] = page_tex_;
    }*/
}

void GSUITest3::RedrawPages(Gui::Renderer *r) {
    using namespace GSUITest3Internal;
    using Ren::Vec2f;

    /*for (int i = 0; i < 4; i++) {
        Vec2f uvs_px = page_corners[i] * Vec2f{ float(page_buf_.w), float(page_buf_.h) };
        uvs_px[0] = std::floor(uvs_px[0]) + 0.5f;
        uvs_px[1] = std::floor(uvs_px[1]) + 0.5f;
        page_corners[i] = uvs_px / Vec2f{ float(page_buf_.w), float(page_buf_.h) };
    }*/

#ifndef DISABLE_MARKERS
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "PAGE DRAW");
#endif
#if 0
    auto page_root = Gui::RootElement{Ren::Vec2i{page_buf_.w, page_buf_.h}};

    glBindFramebuffer(GL_FRAMEBUFFER, page_buf_.fb);
    glViewport(0, 0, page_buf_.w, page_buf_.h);

    glClear(GL_COLOR_BUFFER_BIT);

    book_main_font_->set_scale(/*std::max(float(ctx_->w()) / 4096, 1)*/ 1);
    assert(book_main_font_->draw_mode() == Gui::eDrawMode::DistanceField &&
           book_emph_font_->draw_mode() == Gui::eDrawMode::DistanceField &&
           book_caption_font_->draw_mode() == Gui::eDrawMode::DistanceField);

    // just draw SDF 'as-is'
    book_main_font_->set_draw_mode(Gui::eDrawMode::BlitDistanceField);
    book_emph_font_->set_draw_mode(Gui::eDrawMode::BlitDistanceField);
    book_caption_font_->set_draw_mode(Gui::eDrawMode::BlitDistanceField);

    const int page_base = paged_reader_->cur_page();
    for (int i = 0; i < (book_state_ == eBookState::BkOpened ? 2 : 4); i++) {
        paged_reader_->set_cur_page(page_base + page_order_indices[size_t(book_state_)][i]);

        paged_reader_->Resize(2 * page_corners_uvs[i * 2] - Vec2f{1},
                              2 * (page_corners_uvs[i * 2 + 1] - page_corners_uvs[i * 2]), &page_root);
        paged_reader_->Draw(r);
    }

    paged_reader_->set_cur_page(page_base);

    book_main_font_->set_draw_mode(Gui::eDrawMode::DistanceField);
    book_emph_font_->set_draw_mode(Gui::eDrawMode::DistanceField);
    book_caption_font_->set_draw_mode(Gui::eDrawMode::DistanceField);

    r->Draw(page_buf_.w, page_buf_.h);
#endif

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

#ifndef DISABLE_MARKERS
    glPopDebugGroup();
#endif
}