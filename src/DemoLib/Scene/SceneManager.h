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

    TimingInfo timings() const;
    TimingInfo back_timings() const;

    void LoadScene(const JsObject &js_scene);
    void ClearScene();

    void SetupView(const Ren::Vec3f &origin, const Ren::Vec3f &target, const Ren::Vec3f &up);
    void Draw();

    void InitScene_PT(bool _override = false);
    void SetupView_PT(const Ren::Vec3f &origin, const Ren::Vec3f &target, const Ren::Vec3f &up);
    void Draw_PT();
    void Clear_PT();

    void ResetLightmaps_PT();
    bool PrepareLightmaps_PT();
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
    Environment env_;

    bool cur_lm_indir_ = false;
    size_t cur_lm_obj_ = 0;

    Ren::Storage<Transform> transforms_;
    Ren::Storage<LightSource> lights_;

    std::vector<SceneObject> objects_;
    std::vector<uint32_t> obj_indices_;

    std::vector<bvh_node_t> nodes_;
};