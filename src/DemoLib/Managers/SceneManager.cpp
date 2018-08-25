#include "SceneManager.h"

SceneManager::SceneManager(Ren::Context &ctx, Renderer &renderer) : ctx_(ctx),
                                                renderer_(renderer),
                                                cam_(Ren::Vec3f{ 0.0f, 0.0f, 1.0f },
                                                     Ren::Vec3f{ 0.0f, 0.0f, 0.0f },
                                                     Ren::Vec3f{ 0.0f, 1.0f, 0.0f }) {

}

void SceneManager::LoadScene(const JsObject &js_scene) {
    ClearScene();
}

void SceneManager::ClearScene() {
    objects_.clear();

    assert(transforms_.Size() == 0);
    assert(drawables_.Size() == 0);
}