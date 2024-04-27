#pragma once

#include <Eng/ViewerBase.h>

#if defined(__ANDROID__)
const char ASSETS_BASE_PATH[] = "assets";
#else
const char ASSETS_BASE_PATH[] = "assets_pc";
#endif

class ILog;

namespace Eng {
struct assets_context_t;
class Cmdline;
class SceneManager;
class PhysicsManager;
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

class DebugInfoUI;
class Dictionary;
class FontStorage;

class Viewer : public Eng::ViewerBase {
    ILog *log_ = nullptr;
    std::unique_ptr<FontStorage> font_storage_;
    std::unique_ptr<DebugInfoUI> debug_ui_;
    std::unique_ptr<Dictionary> dictionary_;

  public:
    Viewer(int w, int h, const char *local_dir, int validation_level, bool nohwrt, ILog *log, const char *device_name);
    ~Viewer();

    FontStorage *font_storage() { return font_storage_.get(); }
    DebugInfoUI *debug_ui() { return debug_ui_.get(); }
    Dictionary *dictionary() { return dictionary_.get(); }
    Ray::ILog *ray_log();

    void Frame() override;

    static void PrepareAssets(const char *platform = "all");
    static bool HConvTEIToDict(Eng::assets_context_t &ctx, const char *in_file, const char *out_file,
                               Ren::SmallVectorImpl<std::string, alignof(std::string)> &);
};
