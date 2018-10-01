#pragma once

#include <memory>

#include <Ray/RendererBase.h>
#include <Ren/Camera.h>
#include <Sys/Json.h>

#include "SceneData.h"

class Renderer;

class SceneManager : public std::enable_shared_from_this<SceneManager> {
public:
    SceneManager(Ren::Context &ctx, Renderer &renderer, Ray::RendererBase &ray_renderer);
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
private:
    Ren::MaterialRef OnLoadMaterial(const char *name);
    Ren::ProgramRef OnLoadProgram(const char *name, const char *arg1, const char *arg2);
    Ren::Texture2DRef OnLoadTexture(const char *name);

    void RebuildBVH();

    Ren::Context &ctx_;
    Renderer &renderer_;
    Ray::RendererBase &ray_renderer_;
    Ray::RegionContext ray_reg_ctx_;
    std::shared_ptr<Ray::SceneBase> ray_scene_;

    Ren::Camera cam_;
    Environment env_;

    Ren::Storage<Transform> transforms_;

    std::vector<SceneObject> objects_;

    std::vector<bvh_node_t> nodes_;
};