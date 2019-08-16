#pragma once

#include <condition_variable>
#include <mutex>
#include <thread>

#include <Eng/GameBase.h>
#include <Eng/GameState.h>
#include <Eng/Gui/BaseElement.h>
#include <Eng/Gui/BitmapFont.h>
#include <Eng/Renderer/FrameBuf.h>
#include <Ren/Camera.h>
#include <Ren/MVec.h>
#include <Ren/Mesh.h>
#include <Ren/Program.h>
#include <Ren/SW/SW.h>
#include <Ren/Texture.h>
#include <Sys/Optional.h>

#include "GSBaseState.h"

class Cmdline;
class DebugInfoUI;
class GameStateManager;
class FontStorage;
class SceneManager;
class PagedReader;

class GSUITest3 : public GSBaseState {
    uint64_t last_frame_time_ = 0;
    double cur_fps_ = 0.0;

    uint64_t click_time_ = 0;

    std::shared_ptr<Gui::BitmapFont> book_main_font_, book_emph_font_, book_caption_font_;
    float test_time_counter_s = 0.0f;

    std::unique_ptr<Gui::Image> test_image_;
    std::unique_ptr<Gui::Image9Patch> test_frame_;
    std::unique_ptr<PagedReader> paged_reader_;

    Ren::Vec3f view_origin_, view_dir_;
    float view_fov_ = 0.0f, view_offset_ = 0.0f;
    float max_exposure_ = 1000.0f;

    std::unique_ptr<Gui::Renderer> page_renderer_;
    FrameBuf page_buf_;
    Ren::Tex2DRef page_tex_;
    Ren::MaterialRef orig_page_mat_, page_mat_;

    Sys::Optional<Ren::Vec2f> hit_point_screen_, hit_point_ndc_;
    Sys::Optional<Ren::Vec2f> hint_pos_;

    enum class eBookState {
        BkClosed,
        BkOpening,
        BkOpened,
        BkTurningFwd,
        BkTurningBck
    } book_state_ = eBookState::BkClosed;

    uint32_t book_index_ = 0xffffffff;

    void OnPostloadScene(JsObjectP &js_scene) override;

    void UpdateAnim(uint64_t dt_us) override;

    void Draw() override;
    void DrawUI(Gui::Renderer *r, Gui::BaseElement *root) override;

    Sys::Optional<Ren::Vec2f> MapPointToPageFramebuf(const Ren::Vec2f &p);

    void InitBookMaterials();
    void RedrawPages(Gui::Renderer *r);

  public:
    explicit GSUITest3(GameBase *game);
    ~GSUITest3() final;

    void Enter() override;
    void Exit() override;

    bool HandleInput(const InputManager::Event &evt) override;
};