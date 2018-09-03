#pragma once

#include <thread>

#include "SceneData.h"

class Renderer {
public:
    Renderer();

    void DrawObjects(const Ren::Camera &cam, const std::vector<SceneObject> &objects);
private:
    std::thread background_thread_;

    void BackgroundProc();
};