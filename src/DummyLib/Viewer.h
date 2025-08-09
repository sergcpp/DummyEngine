#pragma once

#include <optional>
#include <string_view>

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
template <typename T, size_t Alignment> class aligned_allocator;
template <typename T, typename Allocator> class SmallVectorImpl;
} // namespace Ren

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
    bool postprocess = true;
    bool fog = true;
    bool freeze_sky = false;
    bool noshow = false;
    int pt_max_samples = 128;
    int pt_max_diff_depth = 4;
    int pt_max_spec_depth = 8;
    int pt_max_refr_depth = 8;
    int pt_max_transp_depth = 8;
    int pt_max_total_depth = 8;
    float pt_clamp_direct = 0.0f;
    float pt_clamp_indirect = 0.0f;
    std::optional<float> exposure;
    std::optional<int> cam_path;
    eGfxPreset gfx_preset = eGfxPreset::High;
    int tex_budget = 2048;
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
    static bool
    HConvTEIToDict(Eng::assets_context_t &ctx, const char *in_file, const char *out_file,
                   Ren::SmallVectorImpl<std::string, Ren::aligned_allocator<std::string, alignof(std::string)>> &,
                   Ren::SmallVectorImpl<Eng::asset_output_t, Ren::aligned_allocator<Eng::asset_output_t, 8>> &);
};
