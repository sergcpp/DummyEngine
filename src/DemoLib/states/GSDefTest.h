#pragma once

#include <Eng/GameState.h>
#include <Ren/Camera.h>
#include <Ren/Mesh.h>
#include <Ren/MVec.h>
#include <Ren/Program.h>
#include <Ren/Texture.h>
#include <Ren/SW/SW.h>

class GameBase;
class GameStateManager;
class FontStorage;

namespace Gui {
class BaseElement;
class BitmapFont;
class Renderer;
}

class GSDefTest : public GameState {
    GameBase *game_;
    std::weak_ptr<GameStateManager> state_manager_;
    std::shared_ptr<Ren::Context> ctx_;

    std::shared_ptr<Gui::Renderer> ui_renderer_;
    std::shared_ptr<Gui::BaseElement> ui_root_;
    std::shared_ptr<Gui::BitmapFont> font_;

    bool view_grabbed_ = false;
    bool view_targeted_ = false;
    Ren::Vec3f view_origin_ = { 0, 20, 3 },
               view_dir_ = { 0, 0, 1 },
               view_target_ = { 0, 0, 0 };

    float forward_speed_ = 0, side_speed_ = 0;

    Ren::Camera cam_;

    Ren::MeshRef test_mesh_;

    Ren::ProgramRef prim_vars_prog_, screen_draw_prog_;

    struct light_t {
        Ren::Vec3f pos, col;
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