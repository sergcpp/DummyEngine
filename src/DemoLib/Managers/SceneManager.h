#pragma once

#include <Ren/Camera.h>
#include <Sys/Json.h>

#include "SceneData.h"

class Renderer;

class SceneManager {
public:
    SceneManager(Ren::Context &ctx, Renderer &renderer);
    ~SceneManager();

    TimingInfo timings() const;
    TimingInfo back_timings() const;

    void SetupView(const Ren::Vec3f &origin, const Ren::Vec3f &target, const Ren::Vec3f &up);

    void LoadScene(const JsObject &js_scene);
    void ClearScene();

    void Draw();
private:
    Ren::MaterialRef OnLoadMaterial(const char *name);
    Ren::ProgramRef OnLoadProgram(const char *name, const char *arg1, const char *arg2);
    Ren::Texture2DRef OnLoadTexture(const char *name);

    Ren::Context &ctx_;
    Renderer &renderer_;

    Ren::Camera cam_;

    Ren::Storage<Transform> transforms_;
    Ren::Storage<Drawable> drawables_;

    std::vector<SceneObject> objects_;
};