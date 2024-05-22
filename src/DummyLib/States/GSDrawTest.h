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

#include "GSBaseState.h"

class DebugInfoUI;
class FontStorage;

namespace Gui {
class BaseElement;
class BitmapFont;
class Renderer;
} // namespace Gui

class GSDrawTest final : public GSBaseState {
    int view_pointer_ = 0, move_pointer_ = 0, sun_pointer_ = 0;

    Ren::Vec3f initial_view_pos_ = Ren::Vec3f{0, 1, 0}, initial_view_dir_ = Ren::Vec3f{0, 0, -1};

    Ren::Vec3f view_origin_ = Ren::Vec3f{0.0f}, view_dir_ = Ren::Vec3f{0.0f, 0.0f, -1.0f};

    float fwd_press_speed_ = 0, side_press_speed_ = 0, fwd_touch_speed_ = 0, side_touch_speed_ = 0;

    float max_fwd_speed_ = 0.5f, view_fov_ = 60.0f;
    float gamma_ = 1.0f, min_exposure_ = -1000.0f, max_exposure_ = 1000.0f;

    uint64_t click_time_ = 0;

    uint64_t wind_update_time_ = 0;
    Ren::Vec3f wind_vector_goal_ = Ren::Vec3f{128.0f, 0.0f, 0.0f};

    // test test
    uint32_t wolf_indices_[32] = {0xffffffff};
    uint32_t scooter_indices_[16] = {0xffffffff};
    uint32_t sophia_indices_[2] = {0xffffffff}, eric_indices_[2] = {0xffffffff};
    uint32_t zenith_index_ = 0xffffffff;
    uint32_t palm_index_ = 0xffffffff;
    uint32_t leaf_tree_index_ = 0xffffffff;
    float scooters_angle_ = 0.0f;

    std::vector<Ren::Vec3f> cam_follow_path_;
    int cam_follow_point_ = -1;
    float cam_follow_param_ = 0.0f;

    void OnPreloadScene(JsObjectP &js_scene) override;
    void OnPostloadScene(JsObjectP &js_scene) override;

    void SaveScene(JsObjectP &js_scene) override;

    void DrawUI(Gui::Renderer *r, Gui::BaseElement *root) override;

    void TestUpdateAnims(float delta_time_s);

  public:
    explicit GSDrawTest(Viewer *viewer);
    ~GSDrawTest() final = default;

    void Enter() override;
    void Exit() override;

    void Draw() override;

    void UpdateFixed(uint64_t dt_us) override;
    void UpdateAnim(uint64_t dt_us) override;

    bool HandleInput(const Eng::InputManager::Event &evt) override;
};