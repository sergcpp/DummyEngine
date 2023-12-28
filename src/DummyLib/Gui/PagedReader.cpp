#include "PagedReader.h"

#include <Eng/gui/BitmapFont.h>
#include <Eng/gui/Image9Patch.h>
#include <Eng/gui/Utils.h>
#include <Ren/Context.h>
#include <Sys/Json.h>

namespace PagedReaderInternal {
const float PageMarginLeft = 0.05f, PageMarginRight = 0.0f, PageMarginTop = 0.1f, PageMarginBottom = 0.05f;

const char Frame01[] =
#if defined(__ANDROID__)
    "assets/"
#else
    "assets_pc/"
#endif
    "textures/ui/frame_01.uncompressed.png";
} // namespace PagedReaderInternal

PagedReader::PagedReader(Ren::Context &ctx, const Gui::Vec2f &pos, const Gui::Vec2f &size, const BaseElement *parent,
                         std::shared_ptr<Gui::BitmapFont> main_font, std::shared_ptr<Gui::BitmapFont> emph_font,
                         std::shared_ptr<Gui::BitmapFont> caption_font)
    : BaseElement(pos, size, parent), parent_(parent), log_(ctx.log()), main_font_(std::move(main_font)),
      emph_font_(std::move(emph_font)), caption_font_(std::move(caption_font)), cur_chapter_(0), cur_page_(0) {
    using namespace PagedReaderInternal;
    background_small_.reset(new Gui::Image9Patch{ctx, Frame01, Ren::Vec2f{3.0f, 3.0f}, 1.0f, Ren::Vec2f{0.0f, 0.0f},
                                                 Ren::Vec2f{1.0f, 1.0f}, parent});
}

void PagedReader::Clear() {
    title_[0].clear();
    title_[1].clear();

    chapters_[0].clear();
    chapters_[1].clear();

    cur_chapter_ = cur_page_ = 0;
    // cur_text_data_.clear();
}

bool PagedReader::LoadBook(const JsObject &js_book, const char *src_lang, const char *trg_lang) {
    Clear();

    if (js_book.Has("title")) {
        const JsObject &js_title = js_book.at("title").as_obj();
        if (!js_title.Has(src_lang)) {
            log_->Error("Language %s is missing in title!", src_lang);
            return false;
        }
        if (!js_title.Has(trg_lang)) {
            log_->Error("Language %s is missing in title!", trg_lang);
            return false;
        }

        const JsString &js_src_title = js_title.at(src_lang).as_str(), &js_trg_title = js_title.at(trg_lang).as_str();

        title_[0] = js_src_title.val;
        title_[1] = js_trg_title.val;
    }

    if (js_book.Has("chapters")) {
        const JsArray &js_chapters = js_book.at("chapters").as_arr();
        for (const JsElement &js_chapter_el : js_chapters.elements) {
            const auto &js_chapter = (const JsObject &)js_chapter_el.as_obj();

            chapters_[0].emplace_back();
            chapters_[1].emplace_back();
            ChapterData &chapter_src = chapters_[0].back(), &chapter_trg = chapters_[1].back();

            if (js_chapter.Has("caption")) {
                const JsObject &js_caption = js_chapter.at("caption").as_obj();
                if (!js_caption.Has(src_lang)) {
                    log_->Error("Language %s is missing in caption!", src_lang);
                    return false;
                }
                if (!js_caption.Has(trg_lang)) {
                    log_->Error("Language %s is missing in caption!", trg_lang);
                    return false;
                }

                const JsString &js_src_caption = js_caption.at(src_lang).as_str(),
                               &js_trg_caption = js_caption.at(trg_lang).as_str();

                chapter_src.caption = js_src_caption.val;
                chapter_trg.caption = js_trg_caption.val;
            }

            if (js_chapter.Has("text_data")) {
                const JsObject &js_data = js_chapter.at("text_data").as_obj();

                if (!js_data.Has(src_lang)) {
                    log_->Error("Language %s is missing in data!", src_lang);
                    return false;
                }
                if (!js_data.Has(trg_lang)) {
                    log_->Error("Language %s is missing in data!", trg_lang);
                    return false;
                }

                const JsString &js_src_data = js_data.at(src_lang).as_str(),
                               &js_trg_data = js_data.at(trg_lang).as_str();

                chapter_src.text_data = js_src_data.val;
                chapter_trg.text_data = js_trg_data.val;
            }
        }
    }

    UpdatePages();

    return true;
}

void PagedReader::Resize(const BaseElement *parent) {
    BaseElement::Resize(parent);
    parent_ = parent;

    UpdatePages();
}

void PagedReader::Draw(Gui::Renderer *r) {
    using Gui::Vec2f;

    // const uint8_t
    //    color_white[4] = { 255, 255, 255, 255 };

    // font_->DrawText(r, "Hi!", Vec2f{ 0.0f, 0.0f }, color_white, parent_);
    DrawCurrentPage(r);
}

void PagedReader::DrawHint(Gui::Renderer *r, const Ren::Vec2f &pos, const Gui::BaseElement *parent) {
    char portion_buf[1024];
    int portion_buf_size = 0;

    const uint8_t color_white[4] = {255, 255, 255, 255};

    if (sentence_to_translate_ != -1) {
        const ChapterData &chapter = chapters_[0][cur_chapter_];
        const SentenceData &sent = chapter.sentences[sentence_to_translate_];

        const char *input_src_data = chapter.text_data.c_str();

        int char_pos = sent.start_pos;
        while (char_pos < sent.end_pos) {
            int char_start = char_pos;

            uint32_t unicode;
            char_pos += Gui::ConvChar_UTF8_to_Unicode(&input_src_data[char_pos], unicode);

            // parse tag
            if (unicode == Gui::g_unicode_less_than) {
                char tag_str[32];
                int tag_str_len = 0;

                while (unicode != Gui::g_unicode_greater_than) {
                    char_pos += Gui::ConvChar_UTF8_to_Unicode(&input_src_data[char_pos], unicode);
                    tag_str[tag_str_len++] = (char)unicode;
                }
                tag_str[tag_str_len - 1] = '\0';

                char_start = char_pos;
            }

            for (int j = char_start; j < char_pos; j++) {
                portion_buf[portion_buf_size++] = input_src_data[j];
            }
        }
        portion_buf[portion_buf_size] = '\0';

        const float width = main_font_->GetWidth(portion_buf, -1, parent), height = main_font_->height(parent);

        background_small_->Resize(pos - Ren::Vec2f{0.025f, 0.025f}, Ren::Vec2f{width + 0.05f, height + 0.05f}, parent);
        background_small_->Draw(r);

        main_font_->DrawText(r, portion_buf, pos, color_white, parent);
    }
}

void PagedReader::DrawCurrentPage(Gui::Renderer *r) const {
    using namespace PagedReaderInternal;

    static const uint8_t color_white[4] = {255, 255, 255, 255};

    const float main_font_height = main_font_->height(parent_), emph_font_height = emph_font_->height(parent_);
    const float x_start = dims_[0][0] + ((cur_page_ % 2) ? PageMarginRight : PageMarginLeft),
                x_limit = dims_[0][0] + dims_[1][0] - ((cur_page_ % 2) ? PageMarginLeft : PageMarginRight),
                y_start = dims_[0][1] + dims_[1][1] - PageMarginTop /*,
                 y_limit = dims_[0][1] + PageMarginBottom*/
        ;

    const float paragraph_height = main_font_height * 2.0f;

    float x_offset = x_start, y_offset = y_start;

    const char *input_src_data = chapters_[1][cur_chapter_].text_data.c_str();
    char portion_buf[4096];
    int portion_buf_size = 0;

    const ChapterData &chapter = chapters_[1][cur_chapter_];
    if (cur_page_ < 0 || cur_page_ >= (int)chapter.pages.size())
        return;

    const PageData &page = chapter.pages[cur_page_];

    if (cur_page_ == 0) {
        caption_font_->DrawText(r, chapter.caption.c_str(), Ren::Vec2f{x_offset, y_offset}, color_white, parent_);
        y_offset -= paragraph_height;
    }

    bool emphasize = false;

    for (int sentence = page.sentence_beg; sentence < page.sentence_end; sentence++) {
        const SentenceData &sent = chapter.sentences[sentence];
        for (int char_pos = std::max(sent.start_pos, page.start_pos);
             char_pos < std::min(sent.end_pos, page.end_pos);) {
            int char_start = char_pos;

            uint32_t unicode;
            char_pos += Gui::ConvChar_UTF8_to_Unicode(&input_src_data[char_pos], unicode);

            bool draw = false, new_line = false, paragraph = false, set_emphasize = false /*, pop_color = false*/;

            const uint8_t *cur_color = color_white;

            // parse tag
            if (unicode == Gui::g_unicode_less_than) {
                char tag_str[32];
                int tag_str_len = 0;

                while (unicode != Gui::g_unicode_greater_than) {
                    char_pos += Gui::ConvChar_UTF8_to_Unicode(&input_src_data[char_pos], unicode);
                    tag_str[tag_str_len++] = (char)unicode;
                }
                tag_str[tag_str_len - 1] = '\0';

                if (strcmp(tag_str, "br") == 0 || strcmp(tag_str, "/p") == 0) {
                    draw = true;
                    new_line = true;
                    paragraph = true;
                } else if (strcmp(tag_str, "em") == 0) {
                    set_emphasize = true;
                } else if (strcmp(tag_str, "/em") == 0) {
                    draw = true;
                    emphasize = true;
                }

                /*const uint8_t *push_color = nullptr;

                if (strcmp(tag_str, "red") == 0) {
                    push_color = color_red;
                } else if (strcmp(tag_str, "cyan") == 0) {
                    push_color = color_cyan;
                } else if (strcmp(tag_str, "violet") == 0) {
                    push_color = color_violet;
                } else if (strcmp(tag_str, "white") == 0) {
                    push_color = color_white;
                } else if (strcmp(tag_str, "yellow") == 0) {
                    push_color = color_yellow;
                } else if (strcmp(tag_str, "/red") == 0 || strcmp(tag_str, "/cyan") == 0
                || strcmp(tag_str, "/violet") == 0 || strcmp(tag_str, "/white") == 0 ||
                strcmp(tag_str, "/yellow") == 0) { pop_color = true; draw = true; } else
                if (strcmp(tag_str, "option") == 0) { const int option_index =
                (int)options_rects_.size(); const OptionData &opt =
                text_options_[data_pos_][option_index];

                    if (opt.is_expanded) {
                        push_color = color_cyan;
                    } if (opt.is_pressed) {
                        push_color = color_green;
                    } else if (opt.is_hover) {
                        push_color = color_red;
                    } else {
                        push_color = color_cyan;
                    }

                    options_rects_.emplace_back();
                    rect_t &rect = options_rects_.back();
                    rect.dims[0] = Ren::Vec2f{ x_offset, y_offset };
                } else if (strcmp(tag_str, "/option") == 0) {
                    pop_color = true;
                    draw = true;

                    const int option_index = (int)options_rects_.size() - 1;
                    const OptionData &opt = text_options_[data_pos_][option_index];

                    if (opt.is_expanded) {
                        expanded_option_ = option_index;
                        expanded_x = x_offset;
                        expanded_y = y_offset;
                    }

                    const float width = font_->GetWidth(portion_string.c_str(), parent_);

                    rect_t &rect = options_rects_.back();
                    rect.dims[1] = Ren::Vec2f{ width, font_height };
                } else if (strcmp(tag_str, "hint") == 0) {
                    const int hint_index = (int)hint_rects_.size();
                    const HintData &hint = text_hints_[data_pos_][hint_index];

                    if (hint.is_hover) {
                        push_color = color_green;
                    } else {
                        push_color = color_yellow;
                    }

                    hint_rects_.emplace_back();
                    rect_t &rect = hint_rects_.back();
                    rect.dims[0] = Ren::Vec2f{ x_offset, y_offset };
                } else if (strcmp(tag_str, "/hint") == 0) {
                    pop_color = true;
                    draw = true;

                    const int hint_index = (int)hint_rects_.size() - 1;
                    const HintData &hint = text_hints_[data_pos_][hint_index];

                    if (hint.is_hover) {
                        expanded_hint = hint_index;
                        expanded_hint_x = x_offset;
                        expanded_hint_y = y_offset;
                    }

                    const float width = font_->GetWidth(portion_string.c_str(), parent_);

                    rect_t &rect = hint_rects_.back();
                    rect.dims[1] = Gui::Vec2f{ width, font_height };
                } else {
                    assert(false && "Unknown tag!");
                }*/

                /*if (push_color) {
                    memcpy(text_color_stack[text_color_stack_size++], push_color, 4);
                    draw = true;
                }*/

                // skip tag symbols
                char_start = char_pos;
            }

            // parse special character
            if (unicode == Gui::g_unicode_ampersand) {
                char char_str[32];
                int char_str_len = 0;

                while (unicode != Gui::g_unicode_semicolon) {
                    char_pos += Gui::ConvChar_UTF8_to_Unicode(&input_src_data[char_pos], unicode);
                    char_str[char_str_len++] = (char)unicode;
                }
                char_str[char_str_len - 1] = '\0';

                if (strcmp(char_str, "quot") == 0) {
                    portion_buf[portion_buf_size++] = '\"';
                } else if (strcmp(char_str, "apos") == 0) {
                    portion_buf[portion_buf_size++] = '\'';
                }

                // skip
                char_start = char_pos;
            }

            for (int j = char_start; j < char_pos; j++) {
                portion_buf[portion_buf_size++] = input_src_data[j];
            }

            if (unicode == Gui::g_unicode_spacebar) {
                const int len_before = portion_buf_size;

                int next_pos = char_pos;
                while (input_src_data[next_pos]) {
                    const int next_start = next_pos;

                    uint32_t _unicode;
                    next_pos += Gui::ConvChar_UTF8_to_Unicode(&input_src_data[next_pos], _unicode);

                    // skip tag
                    if (_unicode == Gui::g_unicode_less_than) {
                        while (_unicode != Gui::g_unicode_greater_than) {
                            next_pos += Gui::ConvChar_UTF8_to_Unicode(&input_src_data[next_pos], _unicode);
                        }
                        continue;
                    }

                    for (int j = next_start; j < next_pos; j++) {
                        portion_buf[portion_buf_size++] = input_src_data[j];
                    }

                    if (_unicode == Gui::g_unicode_spacebar)
                        break;
                }

                // null terminate
                portion_buf[portion_buf_size] = '\0';

                const float x_end = x_offset + main_font_->GetWidth(portion_buf, -1, parent_);
                if (x_end > x_limit) {
                    new_line = true;
                }

                portion_buf_size = len_before;
                draw = true;
            }

            if (draw) {
                // null terminate
                portion_buf[portion_buf_size] = '\0';

                const Gui::BitmapFont *font = emphasize ? emph_font_.get() : main_font_.get();
                // const Gui::BitmapFont *font = (sentence % 2) ? emph_font_.get() :
                // main_font_.get();
                x_offset += font->DrawText(r, portion_buf, Gui::Vec2f{x_offset, y_offset}, cur_color, parent_);

                portion_buf_size = 0;
                if (new_line) {
                    if (paragraph) {
                        y_offset -= paragraph_height;
                    } else {
                        y_offset -= emphasize ? emph_font_height : main_font_height;
                    }
                    x_offset = x_start;
                }
                /*if (pop_color) {
                    text_color_stack_size--;
                }*/
            }

            if (set_emphasize) {
                emphasize = true;
            }
        }
    }

    // draw the last line
    if (portion_buf_size) {
        // null terminate
        portion_buf[portion_buf_size] = '\0';

        x_offset += main_font_->DrawText(r, portion_buf, Gui::Vec2f{x_offset, y_offset},
                                         /*text_color_stack[text_color_stack_size - 1]*/ color_white, parent_);
    }

    (void)x_offset;
}

void PagedReader::UpdatePages() {
    using namespace PagedReaderInternal;

    if (chapters_[0].empty())
        return;

    const float main_font_height = main_font_->height(parent_);
    const float x_start = dims_[0][0] + ((cur_page_ % 2) ? PageMarginRight : PageMarginLeft),
                x_limit = dims_[0][0] + dims_[1][0] - ((cur_page_ % 2) ? PageMarginLeft : PageMarginRight),
                y_start = dims_[0][1] + dims_[1][1] - PageMarginTop, y_limit = dims_[0][1] + PageMarginBottom;

    const float paragraph_height = main_font_height * 2.0f;

    char portion_buf[4096];
    int portion_buf_size = 0;

    // TODO: refactor to avoid duplication of this in draw function
    for (int lang = 0; lang < 2; lang++) {
        const char *input_src_data = chapters_[lang][cur_chapter_].text_data.c_str();

        for (ChapterData &chapter : chapters_[lang]) {
            chapter.pages.clear();
            chapter.sentences.clear();
            chapter.sentence_rects.clear();

            chapter.sentences.emplace_back();

            int char_pos = 0;
            while (input_src_data[char_pos]) {
                chapter.pages.emplace_back();
                PageData &page = chapter.pages.back();

                float x_offset = x_start, y_offset = y_start;
                portion_buf_size = 0;

                if (chapter.pages.size() == 1u /* is first page? */) {
                    y_offset -= paragraph_height;
                }

                page.start_pos = char_pos;
                page.sentence_beg = (int)chapter.sentences.size() - 1;

                while (input_src_data[char_pos]) {
                    int char_start = char_pos;

                    uint32_t unicode;
                    char_pos += Gui::ConvChar_UTF8_to_Unicode(&input_src_data[char_pos], unicode);

                    bool draw = false, new_line = false, paragraph = false;

                    // parse tag
                    if (unicode == Gui::g_unicode_less_than) {
                        char tag_str[32];
                        int tag_str_len = 0;

                        while (unicode != Gui::g_unicode_greater_than) {
                            char_pos += Gui::ConvChar_UTF8_to_Unicode(&input_src_data[char_pos], unicode);
                            tag_str[tag_str_len++] = (char)unicode;
                        }
                        tag_str[tag_str_len - 1] = '\0';

                        if (strcmp(tag_str, "br") == 0 || strcmp(tag_str, "/p") == 0) {
                            draw = true;
                            new_line = true;
                            paragraph = true;
                        }

                        // skip
                        char_start = char_pos;
                    }

                    // parse special character
                    if (unicode == Gui::g_unicode_ampersand) {
                        char char_str[32];
                        int char_str_len = 0;

                        while (unicode != Gui::g_unicode_semicolon) {
                            char_pos += Gui::ConvChar_UTF8_to_Unicode(&input_src_data[char_pos], unicode);
                            char_str[char_str_len++] = (char)unicode;
                        }
                        char_str[char_str_len - 1] = '\0';

                        if (strcmp(char_str, "quot") == 0) {
                            portion_buf[portion_buf_size++] = '\"';
                        } else if (strcmp(char_str, "apos") == 0) {
                            portion_buf[portion_buf_size++] = '\'';
                        }

                        // skip
                        char_start = char_pos;
                    }

                    // parse end of sentence
                    if (unicode == '.') {
                        { // finish current sentence
                            SentenceData &sentence = chapter.sentences.back();
                            sentence.end_pos = char_pos;
                            sentence.rect_end = (int)chapter.sentence_rects.size();
                        }

                        chapter.sentences.emplace_back();
                        SentenceData &next_sentence = chapter.sentences.back();
                        next_sentence.start_pos = char_pos;
                        next_sentence.rect_beg = (int)chapter.sentence_rects.size();
                    }

                    for (int j = char_start; j < char_pos; j++) {
                        portion_buf[portion_buf_size++] = input_src_data[j];
                    }

                    if (unicode == Gui::g_unicode_spacebar) {
                        int len_before = portion_buf_size;

                        int next_pos = char_pos;
                        while (input_src_data[next_pos]) {
                            const int next_start = next_pos;

                            uint32_t _unicode;
                            next_pos += Gui::ConvChar_UTF8_to_Unicode(&input_src_data[next_pos], _unicode);

                            // skip tag
                            if (_unicode == Gui::g_unicode_less_than) {
                                while (_unicode != Gui::g_unicode_greater_than) {
                                    next_pos += Gui::ConvChar_UTF8_to_Unicode(&input_src_data[next_pos], _unicode);
                                }
                                continue;
                            }

                            for (int j = next_start; j < next_pos; j++) {
                                portion_buf[portion_buf_size++] = input_src_data[j];
                            }

                            if (_unicode == Gui::g_unicode_spacebar)
                                break;
                        }

                        // null terminate
                        portion_buf[portion_buf_size] = '\0';

                        const float x_end = x_offset + main_font_->GetWidth(portion_buf, -1, parent_);
                        if (x_end > x_limit) {
                            new_line = true;
                        }

                        portion_buf_size = len_before;
                        draw = true;
                    }

                    if (draw) {
                        // null terminate
                        portion_buf[portion_buf_size] = '\0';

                        const float text_width = main_font_->GetWidth(portion_buf, -1, parent_);

                        const rect_t text_rect = {Ren::Vec2f{x_offset, y_offset},
                                                  Ren::Vec2f{text_width, main_font_height}};

                        chapter.sentence_rects.push_back(text_rect);

                        x_offset += text_width;

                        portion_buf_size = 0;
                        if (new_line) {
                            if (paragraph) {
                                y_offset -= paragraph_height;
                            } else {
                                y_offset -= main_font_height;
                            }
                            x_offset = x_start;

                            if (y_offset < y_limit) {
                                char_pos = char_start;
                                break;
                            }
                        }
                    }
                }

                page.end_pos = char_pos;
                page.sentence_end = (int)chapter.sentences.size() + 1;
            }

            chapter.sentences.back().end_pos = char_pos;
        }
    }
}

void PagedReader::Press(const Gui::Vec2f &p, bool push) {
    if (chapters_[1].empty())
        return;

    sentence_to_translate_ = -1;

    const ChapterData &chapter = chapters_[1][cur_chapter_];
    const PageData &page = chapter.pages[cur_page_];
    for (int sentence = page.sentence_beg; sentence < page.sentence_end; sentence++) {
        const SentenceData &sent = chapter.sentences[sentence];
        for (int rect_index = sent.rect_beg; rect_index < sent.rect_end; rect_index++) {
            const rect_t &rect = chapter.sentence_rects[rect_index];

            if (p[0] > rect.dims[0][0] && p[1] > rect.dims[0][1] && p[0] < rect.dims[0][0] + rect.dims[1][0] &&
                p[1] < rect.dims[0][1] + rect.dims[1][1]) {
                sentence_to_translate_ = sentence;
                break;
            }
        }
    }

    debug_point_ = p;
    log_->Info("Selected sentence %i (%i)", sentence_to_translate_, cur_page_);
}