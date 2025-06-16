#pragma once

#include <condition_variable>
#include <mutex>
#include <thread>

#include <Eng/ViewerBase.h>
#include <Eng/ViewerState.h>
#include <Ren/Camera.h>
#include <Ren/MVec.h>
#include <Ren/Mesh.h>
#include <Ren/Program.h>
#include <Ren/SW/SW.h>
#include <Ren/Texture.h>

#include "BaseState.h"

class DebugInfoUI;
class ViewerStateManager;
class FontStorage;
class SceneManager;

namespace Gui {
class BaseElement;
class BitmapFont;
class Renderer;
} // namespace Gui

class PhyTest final : public BaseState {
    int view_pointer_ = 0, move_pointer_ = 0;

    Ren::Vec3d initial_view_pos_ = Ren::Vec3d{0, 1, 0};
    Ren::Vec3f initial_view_dir_ = Ren::Vec3f{0, 0, -1};

    Ren::Vec3d view_origin_;
    Ren::Vec3f view_dir_;

    float fwd_press_speed_ = 0, side_press_speed_ = 0, fwd_touch_speed_ = 0, side_touch_speed_ = 0;

    float max_fwd_speed_ = 0.5f, view_fov_ = 60;
    float min_exposure_ = -1000, max_exposure_ = 1000;

    uint64_t click_time_ = 0;

    void OnPreloadScene(Sys::JsObjectP &js_scene) override;
    void OnPostloadScene(Sys::JsObjectP &js_scene) override;

    void SaveScene(Sys::JsObjectP &js_scene) override;

    void DrawUI(Gui::Renderer *r, Gui::BaseElement *root) override;

  public:
    explicit PhyTest(Viewer *viewer);
    ~PhyTest() final = default;

    void Enter() override;
    void Exit() override;

    void Draw() override;

    void UpdateFixed(uint64_t dt_us) override;
    void UpdateAnim(uint64_t dt_us) override;

    bool HandleInput(const Eng::input_event_t &evt, const std::vector<bool> &keys_state) override;
};