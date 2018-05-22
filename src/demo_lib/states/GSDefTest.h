#pragma once

#include <engine/GameState.h>
#include <ren/Camera.h>
#include <ren/Mesh.h>
#include <ren/Program.h>
#include <ren/Texture.h>
#include <ren/SW/SW.h>

class GameBase;
class GameStateManager;
class FontStorage;

namespace ui {
class BaseElement;
class BitmapFont;
class Renderer;
}

class GSDefTest : public GameState {
    GameBase *game_;
    std::weak_ptr<GameStateManager> state_manager_;
    std::shared_ptr<ren::Context> ctx_;

    std::shared_ptr<ui::Renderer> ui_renderer_;
    std::shared_ptr<ui::BaseElement> ui_root_;
    std::shared_ptr<ui::BitmapFont> font_;

    bool view_grabbed_ = false;
    bool view_targeted_ = false;
    math::vec3 view_origin_ = { 0, 20, 3 },
               view_dir_ = { 1, 0, 0 },
               view_target_ = { 0, 0, 0 };

    float forward_speed_ = 0, side_speed_ = 0;

    ren::Camera cam_;

    ren::MeshRef test_mesh_;

    ren::ProgramRef prim_vars_prog_, screen_draw_prog_;

    struct light_t {
        math::vec3 pos, col;
    };

    std::vector<light_t> lights_;

    double anim_time_ = 0;

#if defined(USE_GL_RENDER)
    uint32_t framebuf_;
    uint32_t positions_tex_;
    uint32_t normals_tex_;
    uint32_t depth_rb_;

    uint32_t unit_sphere_buf_;
    uint32_t unit_sphere_tris_count_;
#endif

    void InitInternal(int w, int h);

    void DrawInternal();
    void DrawMesh();
public:
    explicit GSDefTest(GameBase *game);
    ~GSDefTest();

    void Enter() override;
    void Exit() override;

    void Draw(float dt_s) override;

    void Update(int dt_ms) override;

    void HandleInput(InputManager::Event) override;
};