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
class FontStorage;

namespace Gui {
class BaseElement;
class BitmapFont;
class Renderer;
} // namespace Gui

class DrawTest final : public BaseState {
    int view_pointer_ = 0, move_pointer_ = 0, sun_pointer_ = 0;

    Ren::Vec3d initial_view_pos_ = Ren::Vec3d{0, 1, 0}, initial_view_dir_ = Ren::Vec3d{0, 0, -1};

    Ren::Vec3d next_view_origin_ = Ren::Vec3d{0}, prev_view_origin_ = Ren::Vec3d{0};
    Ren::Vec3d view_origin_ = Ren::Vec3d{0}, view_dir_ = Ren::Vec3d{0, 0, -1}, view_up_ = Ren::Vec3d{0, 1, 0};
    Ren::Vec2f view_sensor_shift_ = Ren::Vec2f{};

    float fwd_press_speed_ = 0, side_press_speed_ = 0, fwd_touch_speed_ = 0, side_touch_speed_ = 0;

    float max_fwd_speed_ = 0.5f, view_fov_ = 60;
    float gamma_ = 1, min_exposure_ = -14, max_exposure_ = 8;

    uint64_t click_time_ = 0;

    uint64_t wind_update_time_ = 0;
    Ren::Vec3f wind_vector_goal_ = Ren::Vec3f{128, 0, 0};

    // test test
    uint32_t wolf_indices_[32] = {0xffffffff};
    uint32_t scooter_indices_[16] = {0xffffffff};
    uint32_t sophia_indices_[2] = {0xffffffff}, eric_indices_[2] = {0xffffffff};
    uint32_t zenith_index_ = 0xffffffff;
    uint32_t palm_index_ = 0xffffffff;
    uint32_t leaf_tree_index_ = 0xffffffff;
    uint32_t dynamic_light_index_ = 0xffffffff;
    float scooters_angle_ = 0, light_offset_ = 0;

    void OnPreloadScene(Sys::JsObjectP &js_scene) override;
    void OnPostloadScene(Sys::JsObjectP &js_scene) override;

    void SaveScene(Sys::JsObjectP &js_scene) override;

    void DrawUI(Gui::Renderer *r, Gui::BaseElement *root) override;

    void TestUpdateAnims(float delta_time_s);

  public:
    explicit DrawTest(Viewer *viewer);
    ~DrawTest() final = default;

    void Enter() override;
    void Exit() override;

    void Draw() override;

    void UpdateFixed(uint64_t dt_us) override;
    void UpdateAnim(uint64_t dt_us) override;

    bool HandleInput(const Eng::input_event_t &evt, const std::vector<bool> &keys_state) override;
};