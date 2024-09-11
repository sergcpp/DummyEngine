#pragma once

#include <memory>
#include <vector>

#include <Gui/BaseElement.h>
#include <Ren/Fwd.h>

namespace Gui {
class BitmapFont;
}

namespace Sys {
template <typename T, typename FallBackAllocator> class MultiPoolAllocator;
}
template <typename Alloc> struct JsObjectT;
using JsObject = JsObjectT<std::allocator<char>>;
using JsObjectP = JsObjectT<Sys::MultiPoolAllocator<char, std::allocator<char>>>;

class PagedReader : public Gui::BaseElement {
    Ren::ILog *log_;
    std::shared_ptr<Gui::BitmapFont> main_font_, emph_font_, caption_font_;
    std::unique_ptr<Gui::Image9Patch> background_small_;

    struct PageData {
        int start_pos, end_pos;
        int sentence_beg, sentence_end;
    };

    struct SentenceData {
        int start_pos, end_pos;
        int rect_beg, rect_end;
    };

    struct rect_t {
        Gui::Vec2f dims[2];
    };

    struct ChapterData {
        std::string caption;
        std::string text_data;
        std::vector<PageData> pages;
        std::vector<SentenceData> sentences;
        std::vector<rect_t> sentence_rects;
    };

    std::string title_[2];
    std::vector<ChapterData> chapters_[2];

    int cur_chapter_, cur_page_;
    // std::string                         cur_text_data_;

    int sentence_to_translate_ = -1;
    Gui::Vec2f debug_point_;

    void UpdatePages();
    void DrawCurrentPage(Gui::Renderer *r) const;

  public:
    PagedReader(Ren::Context &ctx, const Gui::Vec2f &pos, const Gui::Vec2f &size, const BaseElement *parent,
                std::shared_ptr<Gui::BitmapFont> main_font, std::shared_ptr<Gui::BitmapFont> emph_font,
                std::shared_ptr<Gui::BitmapFont> caption_font);

    int cur_page() const { return cur_page_; }
    void set_cur_page(int page) { cur_page_ = page; }
    int selected_sentence() const { return sentence_to_translate_; }

    int page_count() const { return int(chapters_[0][cur_chapter_].pages.size()); }

    void Clear();
    bool LoadBook(const JsObject &js_book, const char *src_lang, const char *trg_lang);

    void Resize() override;

    void Draw(Gui::Renderer *r) override;
    void DrawHint(Gui::Renderer *r, const Gui::Vec2f &pos, const Gui::BaseElement *parent);

    //void Press(const Gui::Vec2f &p, bool push) override;
};
