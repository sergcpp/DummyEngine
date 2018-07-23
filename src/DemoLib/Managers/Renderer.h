#pragma once

#include <thread>

class Renderer {
public:
    Renderer();

private:
    std::thread background_thread_;

    void BackgroundProc();
};