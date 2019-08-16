#pragma once

#include <Eng/GameBase.h>

const char UI_FONTS_KEY[] = "ui_fonts";
const char UI_DEBUG_KEY[] = "ui_debug";

const char RENDERER_KEY[] = "renderer";
const char RAY_RENDERER_KEY[] = "ray_renderer";
const char SCENE_MANAGER_KEY[] = "scene_manager";
const char PHYSICS_MANAGER_KEY[] = "physics_manager";
const char CMDLINE_KEY[] = "cmdline";
const char DICT_KEY[] = "dictionary";

const char SWAP_TIMER_KEY[] = "swap_timer";

const char AUX_GFX_THREAD[] = "aux_gfx_thread";

#if defined(__ANDROID__)
const char ASSETS_BASE_PATH[] = "assets";
#else
const char ASSETS_BASE_PATH[] = "assets_pc";
#endif

struct assets_context_t;

namespace Sys {
class ThreadWorker;
}

class Viewer : public GameBase {
  public:
    Viewer(int w, int h, const char *local_dir, const char *device_name,
           std::shared_ptr<Sys::ThreadWorker> aux_gfx_thread);

    void Resize(int w, int h) override;

    void Frame() override;

    static void PrepareAssets(const char *platform = "all");
    static bool HConvTEIToDict(assets_context_t &ctx, const char *in_file, const char *out_file);
};
