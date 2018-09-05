#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <Gui/BitmapFont.h>

class FontStorage {
    std::vector<std::pair<std::string, std::shared_ptr<Gui::BitmapFont>>> fonts_;
public:

    std::shared_ptr<Gui::BitmapFont> FindFont(const std::string &name) const {
        for (auto &f : fonts_) {
            if (f.first == name) {
                return f.second;
            }
        }
        return nullptr;
    }

    std::shared_ptr<Gui::BitmapFont> LoadFont(const std::string &name, const std::string &file_name, Ren::Context *ctx) {
        auto font = FindFont(name);
        if (!font) {
            font = std::make_shared<Gui::BitmapFont>(file_name.c_str(), ctx);
            fonts_.push_back(std::make_pair(name, font));
        }
        return font;
    }

    void EraseFont(const std::string &name) {
        for (auto it = fonts_.begin(); it != fonts_.end(); ++it) {
            if (it->first == name) {
                fonts_.erase(it);
                return;
            }
        }
    }

    void Clear() {
        fonts_.clear();
    }
};
