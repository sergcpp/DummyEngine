#include "GSUITest3.h"

#include <Ren/GL.h>

#include "../Gui/PagedReader.h"

namespace GSUITest3Internal {
extern const Ren::Vec2f page_corners_uvs[];
extern const int page_order_indices[][4];
} // namespace GSUITest3Internal

void GSUITest3::InitBookMaterials() {
    assert(page_buf_.w != -1 && page_buf_.h != -1 &&
           "Page framebuffer is not initialized!");

    { // register framebuffer texture
        Ren::Tex2DParams params;
        params.w = page_buf_.w;
        params.h = page_buf_.h;
        params.format = page_buf_.attachments[0].desc.format;
        params.sampling.filter = page_buf_.attachments[0].desc.filter;
        params.sampling.repeat = page_buf_.attachments[0].desc.repeat;

        // prevent texture deletion
        params.flags = Ren::TexNoOwnership;

        page_tex_ = ren_ctx_->textures().Add(
            "__book_page_texture__", page_buf_.attachments[0].tex->id(), params, log_.get());
    }

    { // register material
        Ren::eMatLoadStatus status;
        orig_page_mat_ = ren_ctx_->LoadMaterial("book/book_page0.txt", nullptr, &status,
                                                nullptr, nullptr);
        if (status != Ren::eMatLoadStatus::Found) {
            log_->Error("Failed to find material book/book_page0");
            return;
        }

        Ren::ProgramRef programs[Ren::MaxMaterialProgramCount];
        for (int i = 0; i < Ren::MaxMaterialProgramCount; i++) {
            programs[i] = orig_page_mat_->programs[i];
        }

        Ren::Tex2DRef textures[Ren::MaxMaterialTextureCount];
        for (int i = 0; i < Ren::MaxMaterialTextureCount; i++) {
            textures[i] = orig_page_mat_->textures[i];
        }

        // replace texture
        textures[2] = page_tex_;

        Ren::Vec4f params[Ren::MaxMaterialParamCount];
        for (int i = 0; i < Ren::MaxMaterialParamCount; i++) {
            params[i] = orig_page_mat_->params[i];
        }

        page_mat_ =
            ren_ctx_->materials().Add("__book_page_material__", orig_page_mat_->flags(),
                                      programs, textures, params, log_.get());
    }
}

void GSUITest3::RedrawPages(Gui::Renderer *r) {
    using namespace GSUITest3Internal;
    using Ren::Vec2f;

    /*for (int i = 0; i < 4; i++) {
        Vec2f uvs_px = page_corners[i] * Vec2f{ (float)page_buf_.w, (float)page_buf_.h };
        uvs_px[0] = std::floor(uvs_px[0]) + 0.5f;
        uvs_px[1] = std::floor(uvs_px[1]) + 0.5f;
        page_corners[i] = uvs_px / Vec2f{ (float)page_buf_.w, (float)page_buf_.h };
    }*/

#ifndef DISABLE_MARKERS
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "PAGE DRAW");
#endif
    auto page_root = Gui::RootElement{Ren::Vec2i{page_buf_.w, page_buf_.h}};

    glBindFramebuffer(GL_FRAMEBUFFER, page_buf_.fb);
    glViewport(0, 0, page_buf_.w, page_buf_.h);

    glClear(GL_COLOR_BUFFER_BIT);

    r->SwapBuffers();

    // just blit sdf into a buffer ignoring alpha
    // glDisable(GL_BLEND);
    // glBlendFunc(GL_ONE, GL_ONE);

    book_main_font_->set_scale(/*std::max((float)ctx_->w() / 4096.0f, 1.0f)*/ 1.0f);
    assert(book_main_font_->draw_mode() == Gui::eDrawMode::DrDistanceField &&
           book_emph_font_->draw_mode() == Gui::eDrawMode::DrDistanceField &&
           book_caption_font_->draw_mode() == Gui::eDrawMode::DrDistanceField);

    // just draw SDF 'as-is'
    book_main_font_->set_draw_mode(Gui::eDrawMode::DrBlitDistanceField);
    book_emph_font_->set_draw_mode(Gui::eDrawMode::DrBlitDistanceField);
    book_caption_font_->set_draw_mode(Gui::eDrawMode::DrBlitDistanceField);

    const int page_base = paged_reader_->cur_page();
    for (int i = 0; i < (book_state_ == eBookState::BkOpened ? 2 : 4); i++) {
        paged_reader_->set_cur_page(page_base +
                                    page_order_indices[(size_t)book_state_][i]);

        paged_reader_->Resize(
            2.0f * page_corners_uvs[i * 2] - Vec2f{1.0f},
            2.0f * (page_corners_uvs[i * 2 + 1] - page_corners_uvs[i * 2]), &page_root);
        paged_reader_->Draw(r);
    }

    paged_reader_->set_cur_page(page_base);

    book_main_font_->set_draw_mode(Gui::eDrawMode::DrDistanceField);
    book_emph_font_->set_draw_mode(Gui::eDrawMode::DrDistanceField);
    book_caption_font_->set_draw_mode(Gui::eDrawMode::DrDistanceField);

    r->Draw();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

#ifndef DISABLE_MARKERS
    glPopDebugGroup();
#endif
}