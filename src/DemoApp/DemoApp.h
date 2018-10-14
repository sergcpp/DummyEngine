#pragma once

#include <memory>
#include <string>
#include <vector>

#include <Eng/TimedInput.h>

#if !defined(__ANDROID__)
struct SDL_Renderer;
struct SDL_Texture;
struct SDL_Window;
#endif

class GameBase;

class DemoApp {
#if defined(USE_GL_RENDER)
    void            *gl_ctx_ = nullptr;
#elif defined(USE_SW_RENDER)
    SDL_Renderer    *renderer_ = nullptr;
    SDL_Texture     *texture_ = nullptr;
#endif
    bool quit_;

#if !defined(__ANDROID__)
    SDL_Window		*window_ = nullptr;

    bool ConvertToRawButton(int32_t key, InputManager::RawInputButton &button);
    void PollEvents();
#endif

    std::unique_ptr<GameBase> viewer_;
public:
    DemoApp();
    ~DemoApp();

    int Init(int w, int h);
    void Destroy();

    void Frame();
    void Resize(int w, int h);

    void AddEvent(int type, int key, float x, float y, float dx, float dy);

#if !defined(__ANDROID__)
    int Run(const std::vector<std::string> &args);
#endif

    bool terminated() const {
        return quit_;
    }
};
