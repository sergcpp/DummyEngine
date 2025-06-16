#pragma once

#include <condition_variable>
#include <mutex>
#include <optional>
#include <thread>

#include <Eng/ViewerBase.h>
#include <Eng/ViewerState.h>
#include <Gui/BaseElement.h>
#include <Gui/BitmapFont.h>
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
class PagedReader;

class UITest3 : public BaseState {
    uint64_t last_frame_time_ = 0;
    double cur_fps_ = 0.0;

    uint64_t click_time_ = 0;

    const Gui::BitmapFont *book_main_font_ = {}, *book_emph_font_ = {}, *book_caption_font_ = {};
    float test_time_counter_s = 0;

    std::unique_ptr<Gui::Image> test_image_;
    std::unique_ptr<Gui::Image9Patch> test_frame_;
    std::unique_ptr<PagedReader> paged_reader_;

    Ren::Vec3d view_origin_;
    Ren::Vec3f view_dir_;
    float view_fov_ = 0, view_offset_ = 0;
    float min_exposure_ = -1000, max_exposure_ = 1000;

    std::unique_ptr<Gui::Renderer> page_renderer_;
    // FrameBuf page_buf_;
    Ren::TexRef page_tex_;
    Ren::MaterialRef orig_page_mat_, page_mat_;

    std::optional<Gui::Vec2f> hit_point_screen_, hit_point_ndc_;
    std::optional<Gui::Vec2f> hint_pos_;

    enum class eBookState {
        BkClosed,
        BkOpening,
        BkOpened,
        BkTurningFwd,
        BkTurningBck
    } book_state_ = eBookState::BkClosed;

    uint32_t book_index_ = 0xffffffff;

    void OnPostloadScene(Sys::JsObjectP &js_scene) override;

    void UpdateAnim(uint64_t dt_us) override;

    void Draw() override;
    void DrawUI(Gui::Renderer *r, Gui::BaseElement *root) override;

    std::optional<Gui::Vec2f> MapPointToPageFramebuf(const Gui::Vec2f &p);

    void InitBookMaterials();
    void RedrawPages(Gui::Renderer *r);

  public:
    explicit UITest3(Viewer *viewer);
    ~UITest3() final;

    void Enter() override;
    void Exit() override;

    bool HandleInput(const Eng::input_event_t &evt, const std::vector<bool> &keys_state) override;
};