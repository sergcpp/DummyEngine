#pragma once

#include <Ren/Camera.h>
#include <Ren/Mesh.h>
#include <Ren/MMat.h>
#include <Ren/Storage.h>
#include <Sys/Json.h>

struct Transform : public Ren::RefCounter {
    Ren::Mat4f mat;
    Ren::Vec3f bbox_min_ws, bbox_max_ws;
};

struct Drawable : public Ren::RefCounter {
    Ren::MeshRef mesh;
};

enum eObjectFlags { HasTransform = 1, HasDrawable = 2 };

struct SceneObject {
    uint32_t flags;
    Ren::StorageRef<Transform> tr;
    Ren::StorageRef<Drawable> dr;
};

class Renderer;

class SceneManager {
public:
    SceneManager(Ren::Context &ctx, Renderer &renderer);

    void LoadScene(const JsObject &js_scene);
    void ClearScene();

private:
    Ren::Context &ctx_;
    Renderer &renderer_;

    Ren::Camera cam_;

    Ren::Storage<Transform> transforms_;
    Ren::Storage<Drawable> drawables_;

    std::vector<SceneObject> objects_;
};