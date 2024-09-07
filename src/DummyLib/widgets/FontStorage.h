#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <Gui/BitmapFont.h>

class FontStorage {
    std::vector<std::pair<std::string, std::unique_ptr<Gui::BitmapFont>>> fonts_;

  public:
    const Gui::BitmapFont *FindFont(std::string_view name) const {
        for (auto &f : fonts_) {
            if (f.first == name) {
                return f.second.get();
            }
        }
        return nullptr;
    }

    const Gui::BitmapFont *LoadFont(std::string_view name, std::string_view file_name, Ren::Context &ctx) {
        const Gui::BitmapFont *font = FindFont(name);
        if (!font) {
            auto new_font = std::make_unique<Gui::BitmapFont>(file_name, ctx);
            font = new_font.get();
            fonts_.push_back(std::make_pair(std::string(name), std::move(new_font)));
        }
        return font;
    }

    void EraseFont(std::string_view name) {
        for (auto it = fonts_.begin(); it != fonts_.end(); ++it) {
            if (it->first == name) {
                fonts_.erase(it);
                return;
            }
        }
    }

    void Clear() { fonts_.clear(); }
};
