#pragma once

#include <optional>
#include <string_view>

// #include <Eng/scene/SceneManager.h>
#include <Eng/ViewerBase.h>

#if defined(__ANDROID__)
const char ASSETS_BASE_PATH[] = "assets";
#else
const char ASSETS_BASE_PATH[] = "assets_pc";
#endif

class ILog;

namespace Eng {
struct assets_context_t;
struct asset_output_t;
class CmdlineUI;
class SceneManager;
class PhysicsManager;
class DebugFrameUI;
} // namespace Eng

namespace Ray {
class ILog;
};

namespace Ren {
template <typename T, int AlignmentOfT> class SmallVectorImpl;
}

namespace Sys {
class ThreadWorker;
}

class Dictionary;
class FontStorage;

enum class eGfxPreset { Medium, High, Ultra };

struct AppParams {
    std::string scene_name = "scenes/mat_test.json";
    std::string device_name;
    std::string ref_name;
    double psnr = 0.0;
    bool nohwrt = false;
    bool nosubgroup = false;
    bool pt = false;
    bool pt_denoise = true;
    bool pt_nohwrt = false;
    bool postprocess = true;
    bool freeze_sky = false;
    int pt_max_samples = 128;
    std::optional<float> exposure;
    eGfxPreset gfx_preset = eGfxPreset::High;
    float sun_dir[3] = {};
#ifndef NDEBUG
    int validation_level = 1;
#else
    int validation_level = 0;
#endif
};

class Viewer : public Eng::ViewerBase {
    ILog *log_ = nullptr;
    std::unique_ptr<FontStorage> font_storage_;
    std::unique_ptr<Eng::CmdlineUI> cmdline_ui_;
    std::unique_ptr<Eng::DebugFrameUI> debug_ui_;
    std::unique_ptr<Dictionary> dictionary_;

  public:
    AppParams app_params = {};

    Viewer(int w, int h, const AppParams &app_params, ILog *log);
    ~Viewer();

    FontStorage *font_storage() { return font_storage_.get(); }
    Eng::CmdlineUI *cmdline_ui() { return cmdline_ui_.get(); }
    Eng::DebugFrameUI *debug_ui() { return debug_ui_.get(); }
    Dictionary *dictionary() { return dictionary_.get(); }
    Ray::ILog *ray_log();

    void Frame() override;

    static void PrepareAssets(std::string_view platform = "all");
    static bool HConvTEIToDict(Eng::assets_context_t &ctx, const char *in_file, const char *out_file,
                               Ren::SmallVectorImpl<std::string, alignof(std::string)> &,
                               Ren::SmallVectorImpl<Eng::asset_output_t, 8> &);
};
