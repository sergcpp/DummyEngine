#pragma once

#include <Eng/GameBase.h>

const char UI_FONTS_KEY[]               = "ui_fonts";

const char RENDERER_KEY[]               = "renderer";
const char RAY_RENDERER_KEY[]           = "ray_renderer";
const char SCENE_MANAGER_KEY[]          = "scene_manager";

class Viewer : public GameBase {
public:
    Viewer(int w, int h, const char *local_dir);

    void Resize(int w, int h) override;

    static void PrepareAssets();
};

