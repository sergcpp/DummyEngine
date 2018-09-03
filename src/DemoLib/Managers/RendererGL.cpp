#include "Renderer.h"

void Renderer::DrawObjects(const Ren::Camera &cam, const std::vector<SceneObject> &objects) {
    const uint32_t has_dr_and_tr = HasDrawable | HasTransform;

    for (const auto &obj : objects) {
        if (obj.flags & has_dr_and_tr) {

        }
    }
}