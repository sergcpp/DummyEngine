#pragma once

#include <Eng/GameBase.h>

#if defined(__ANDROID__)
const char ASSETS_BASE_PATH[] = "assets";
#else
const char ASSETS_BASE_PATH[] = "assets_pc";
#endif

namespace Eng {
struct assets_context_t;
class Cmdline;
class SceneManager;
class PhysicsManager;
}

namespace Ray {
class RendererBase;
}

namespace Ren {
template <typename T, int AlignmentOfT> class SmallVectorImpl;
}

namespace Sys {
class ThreadWorker;
}

class DebugInfoUI;
class Dictionary;
class FontStorage;

class Viewer : public Eng::GameBase {
    std::unique_ptr<FontStorage> font_storage_;
    std::unique_ptr<DebugInfoUI> debug_ui_;
    std::unique_ptr<Ray::RendererBase> ray_renderer_;
    std::unique_ptr<Dictionary> dictionary_;

  public:
    Viewer(int w, int h, const char *local_dir, int validation_level, const char *device_name);
    ~Viewer();

    FontStorage *font_storage() { return font_storage_.get(); }
    DebugInfoUI *debug_ui() { return debug_ui_.get(); }
    Ray::RendererBase *ray_renderer() { return ray_renderer_.get(); }
    Dictionary *dictionary() { return dictionary_.get(); }

    void Frame() override;

    static void PrepareAssets(const char *platform = "all");
    static bool HConvTEIToDict(Eng::assets_context_t &ctx, const char *in_file, const char *out_file,
                               Ren::SmallVectorImpl<std::string, alignof(std::string)> &);
};
