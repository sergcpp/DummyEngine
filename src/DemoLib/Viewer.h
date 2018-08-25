#pragma once

#include <Eng/GameBase.h>

const char UI_FONTS_KEY[]               = "ui_fonts";

const char RENDERER_KEY[]               = "renderer";

class Viewer : public GameBase {
public:
    Viewer(int w, int h, const char *local_dir);

    void Resize(int w, int h) override;
};

