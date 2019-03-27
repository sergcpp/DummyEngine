#pragma once

#include <memory>

#include <Ray/RendererBase.h>
#include <Ren/Camera.h>
#include <Sys/Json.h>

#include "SceneData.h"

class Renderer;

namespace Sys {
    class ThreadPool;
}

class SceneManager : public std::enable_shared_from_this<SceneManager> {
public:
    SceneManager(Ren::Context &ctx, Renderer &renderer, Ray::RendererBase &ray_renderer,
                 Sys::ThreadPool &threads);
    ~SceneManager();

    uint32_t render_flags() const;
    RenderInfo render_info() const;
    FrontendInfo frontend_info() const;
    BackendInfo backend_info() const;

    const Ren::Camera &main_cam() const { return cam_; }
    const SceneData &scene_data() const { return scene_data_; }

    void LoadScene(const JsObject &js_scene);
    void ClearScene();

    void SetupView(const Ren::Vec3f &origin, const Ren::Vec3f &target, const Ren::Vec3f &up);
    void Frame();

    void InitScene_PT(bool _override = false);
    void SetupView_PT(const Ren::Vec3f &origin, const Ren::Vec3f &target, const Ren::Vec3f &up);
    void Draw_PT();
    void Clear_PT();

    void ResetLightmaps_PT();
    bool PrepareLightmaps_PT();

    static bool PrepareAssets(const char *in_folder, const char *out_folder, const char *platform, Sys::ThreadPool *p_threads);
private:
    Ren::MaterialRef OnLoadMaterial(const char *name);
    Ren::ProgramRef OnLoadProgram(const char *name, const char *arg1, const char *arg2);
    Ren::Texture2DRef OnLoadTexture(const char *name);

    void RebuildBVH();

    std::string scene_name_;

    Ren::Context &ctx_;
    Renderer &renderer_;
    Ray::RendererBase &ray_renderer_;
    Sys::ThreadPool &threads_;
    std::vector<Ray::RegionContext> ray_reg_ctx_;
    std::shared_ptr<Ray::SceneBase> ray_scene_;

    Ren::Camera cam_;
    std::string env_map_pt_name_;

    SceneData scene_data_;

    bool cur_lm_indir_ = false;
    size_t cur_lm_obj_ = 0;

    // PT temp data
    std::vector<Ray::pixel_color_t> pt_lm_direct_, pt_lm_indir_,
                                    pt_lm_indir_sh_[4];
};