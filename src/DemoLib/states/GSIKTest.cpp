#include "GSIKTest.h"

#include <cmath>
#include <fstream>
#include <random>

#include <Eng/GameStateManager.h>
#include <Gui/Renderer.h>
#include <Ren/Context.h>
#include <Ren/GL.h>
#include <Ren/Utils.h>
#include <Sys/Log.h>
#include <Sys/Time_.h>

#include "../Gui/FontStorage.h"
#include "../Viewer.h"

namespace GSIKTestInternal {
const char *vs_shader = R"(
        /*
        ATTRIBUTES
            aVertexPosition : 0
        UNIFORMS
            uMVPMatrix : 0
        */

        attribute vec3 aVertexPosition;

        uniform mat4 uMVPMatrix;

        void main(void) {
            gl_Position = uMVPMatrix * vec4(aVertexPosition, 1.0);
        }
    )";

const char *fs_shader = R"(
        #ifdef GL_ES
        precision mediump float;
        #endif

        /*
        UNIFORMS
	        col : 0
        */

        uniform vec3 col;

        void main(void) {
	        gl_FragColor = vec4(col, 1.0);
        }
    )";

auto to_radians = [](float deg) {
    return deg * std::acos(-1.0f) / 180.0f;
};
}

GSIKTest::GSIKTest(GameBase *game) : game_(game), cam_(Ren::Vec3f{ 0.0f, -0.5f, 2.5f },
            Ren::Vec3f{ 0.0f, -0.5f, 0.0f },
            Ren::Vec3f{ 0.0f, 1.0f, 0.0f }) {
    state_manager_  = game->GetComponent<GameStateManager>(STATE_MANAGER_KEY);
    ctx_            = game->GetComponent<Ren::Context>(REN_CONTEXT_KEY);

    ui_renderer_    = game->GetComponent<Gui::Renderer>(UI_RENDERER_KEY);
    ui_root_        = game->GetComponent<Gui::BaseElement>(UI_ROOT_KEY);

    const auto fonts = game->GetComponent<FontStorage>(UI_FONTS_KEY);
    font_ = fonts->FindFont("main_font");

    cam_.Perspective(60.0f, float(game_->width)/game_->height, 0.1f, 1000.0f);
}

GSIKTest::~GSIKTest() {

}

void GSIKTest::Enter() {
    using namespace GSIKTestInternal;

    Ren::eProgLoadStatus status;
    line_prog_ = ctx_->LoadProgramGLSL("line_prog", vs_shader, fs_shader, &status);
    assert(status == Ren::ProgCreatedFromData);

    bones_.push_back(Bone{ Ren::Vec3f{ 0.0f, 1.0f, 0.0f }, Ren::Vec3f{ 0.0f, 1.0f, 0.0f }, 0.1f, 0.0f, -to_radians(180), to_radians(180), Ren::Vec3f{}, Ren::Vec3f{} });
    bones_.push_back(Bone{ Ren::Vec3f{ 0.0f, 1.0f, 0.0f }, Ren::Vec3f{ 0.0f, 0.0f, 1.0f }, 0.1f, 0.0f, -to_radians(45), to_radians(45), Ren::Vec3f{}, Ren::Vec3f{} });
    bones_.push_back(Bone{ Ren::Vec3f{ 0.0f, 1.0f, 0.0f }, Ren::Vec3f{ 0.0f, 0.0f, 1.0f }, 0.1f, 0.0f, -to_radians(45), to_radians(45), Ren::Vec3f{}, Ren::Vec3f{} });
    bones_.push_back(Bone{ Ren::Vec3f{ 0.0f, 1.0f, 0.0f }, Ren::Vec3f{ 0.0f, 0.0f, 1.0f }, 0.1f, 0.0f, -to_radians(45), to_radians(45), Ren::Vec3f{}, Ren::Vec3f{} });
    bones_.push_back(Bone{ Ren::Vec3f{ 0.0f, 1.0f, 0.0f }, Ren::Vec3f{ 0.0f, 0.0f, 1.0f }, 0.1f, 0.0f, -to_radians(45), to_radians(45), Ren::Vec3f{}, Ren::Vec3f{} });

    //bones_[0].angle = 45.0f;
    //bones_[1].angle = 45.0f;
}

void GSIKTest::Exit() {

}

void GSIKTest::Draw(float dt_s) {
    using namespace GSIKTestInternal;

    {
        glClearColor(0, 0.2f, 0.2f, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDisable(GL_DEPTH_TEST);

        Ren::Mat4f view_from_world = cam_.view_matrix(),
                   proj_from_view = cam_.projection_matrix();

        Ren::Mat4f world_from_object = Ren::Mat4f{ 1.0f };

        world_from_object = Ren::Rotate(world_from_object, to_radians(view_angle_), Ren::Vec3f{ 0.0f, 1.0f, 0.0f });

        Ren::Mat4f view_from_object = view_from_world * world_from_object,
                   proj_from_object = proj_from_view * view_from_object;

        const auto *p = line_prog_.get();

        const GLuint pos_attrib = p->attribute("aVertexPosition").loc;

        const GLuint mvp_unif = p->uniform("uMVPMatrix").loc;
        const GLuint col_unif = p->uniform("col").loc;

        glUseProgram(p->prog_id());

        glUniformMatrix4fv(mvp_unif, 1, GL_FALSE, ValuePtr(proj_from_object));

        glUniform3f(col_unif, 0.0f, 1.0f, 1.0f);

        glEnableVertexAttribArray(pos_attrib);
        glVertexAttribPointer(pos_attrib, 3, GL_FLOAT, GL_FALSE, sizeof(Bone), &bones_[0].cur_pos[0]);

        glDrawArrays(GL_LINE_STRIP, 0, (GLsizei)bones_.size());

        glUniform3f(col_unif, 1.0f, 1.0f, 1.0f);

        const float cross[] = { cur_goal_[0] - 0.025f, cur_goal_[1], cur_goal_[2],
                                cur_goal_[0] + 0.025f, cur_goal_[1], cur_goal_[2],
                                cur_goal_[0], cur_goal_[1] - 0.025f, cur_goal_[2],
                                cur_goal_[0], cur_goal_[1] + 0.025f, cur_goal_[2],
                                cur_goal_[0], cur_goal_[1], cur_goal_[2] - 0.025f,
                                cur_goal_[0], cur_goal_[1], cur_goal_[2] + 0.025f
                              };

        glVertexAttribPointer(pos_attrib, 3, GL_FLOAT, GL_FALSE, 0, &cross[0]);
        glDrawArrays(GL_LINES, 0, 6);

        const float lines[] = { -0.5f, -1.0f, -0.5f,    0.5f, -1.0f, -0.5f,
                                0.5f, -1.0f, -0.5f,     0.5f, -1.0f, 0.5f,
                                0.5f, -1.0f, 0.5f,      -0.5f, -1.0f, 0.5f,
                                -0.5f, -1.0f, 0.5f,     -0.5f, -1.0f, -0.5f,

                                -0.5f, -1.0f, -0.5f,    -0.5f, 0.0f, -0.5f,
                                0.5f, -1.0f, -0.5f,     0.5f, 0.0f, -0.5f,
                                0.5f, -1.0f, 0.5f,      0.5f, 0.0f, 0.5f,
                                -0.5f, -1.0f, 0.5f,     -0.5f, 0.0f, 0.5f,

                                -0.5f, 0.0f, -0.5f,    0.5f, 0.0f, -0.5f,
                                0.5f, 0.0f, -0.5f,     0.5f, 0.0f, 0.5f,
                                0.5f, 0.0f, 0.5f,      -0.5f, 0.0f, 0.5f,
                                -0.5f, 0.0f, 0.5f,     -0.5f, 0.0f, -0.5f
                              };

        glVertexAttribPointer(pos_attrib, 3, GL_FLOAT, GL_FALSE, 0, &lines[0]);
        glDrawArrays(GL_LINES, 0, (GLsizei)24);
    }

    {
        // ui draw
        ui_renderer_->BeginDraw();

        std::string str = "Iterations: " + std::to_string(iterations_) + " Error: " + std::to_string(error_);
        font_->DrawText(ui_renderer_.get(), str.c_str(), { -0.95f, 0.95f - 1 * font_->height(ui_root_.get()) }, ui_root_.get());
        //font_->DrawText(ui_renderer_.get(), s2.c_str(), { -1, 1.0f - 2 * font_->height(ui_root_.get()) }, ui_root_.get());
        //font_->DrawText(ui_renderer_.get(), s3.c_str(), { -1, 1.0f - 3 * font_->height(ui_root_.get()) }, ui_root_.get());
        //font_->DrawText(ui_renderer_.get(), s4.c_str(), { -1, 1.0f - 4 * font_->height(ui_root_.get()) }, ui_root_.get());

        ui_renderer_->EndDraw();
    }

    ctx_->ProcessTasks();
}

void GSIKTest::UpdateBones() {
    if (bones_.empty()) return;

    Ren::Vec3f cur_pos = { 0.0f, -1.0f, 0.0f };

    Ren::Mat4f cur_rot;

    for (auto &b : bones_) {
        b.cur_pos = cur_pos;

        Ren::Vec4f dir = Ren::Vec4f{ 0.0f };
        dir[0] = b.rot_axis[0];
        dir[1] = b.rot_axis[1];
        dir[2] = b.rot_axis[2];

        dir = dir * cur_rot;

        b.cur_rot_axis = Ren::Vec3f{ dir };

        cur_rot = Ren::Rotate(cur_rot, b.angle, b.cur_rot_axis);

        dir[0] = b.dir[0];
        dir[1] = b.dir[1];
        dir[2] = b.dir[2];

        dir = dir * cur_rot;

        cur_pos += Ren::Vec3f{ dir } * b.length;
    }
}

void GSIKTest::Update(int dt_ms) {
    /*for (auto &b : bones_) {
        b.angle += 0.01f;
    }*/

    /*Ren::Vec3f cur_pos = { 0.0f, 0.0f, 0.0f };
    Ren::Vec4f cur_dir = { 0.0f, 1.0f, 0.0f, 0.0f };
    for (auto &b : bones_) {
        b.pos = cur_pos;

        Ren::Vec4f dir = { 0.0f, 1.0f, 0.0f, 0.0f };

        Ren::Mat4f rot;
        rot = Ren::Rotate(rot, b.angle, Ren::Vec3f{ 0.0f, 0.0f, 1.0f });

        cur_dir = cur_dir * rot;

        cur_pos += Ren::Vec3f{ cur_dir[0], cur_dir[1], cur_dir[2] } *b.length;
    }*/

    auto clamp = [](float x, float min, float max) {
        return std::min(std::max(x, min), max);
    };

    //bones_[0].angle += 1.0f;

    UpdateBones();

    //return;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(-1.0f, 1.0f);

#if 1
    goal_change_timer_ += dt_ms;
    if (goal_change_timer_ > 1000) {
        prev_goal_ = next_goal_;
        next_goal_[0] = 0.5f * dis(gen);
        next_goal_[1] = -1.0f + 0.5f * dis(gen) + 0.5f;
        next_goal_[2] = 0.5f * dis(gen);

        goal_change_timer_ -= 1000;
    }

    cur_goal_ = prev_goal_ + (next_goal_ - prev_goal_) * 0.001f * float(goal_change_timer_);
#else
    cur_goal_ = Ren::Vec3f { 0.0f, 0.0f, 0.25f };
#endif

    Ren::Vec3f end_pos = bones_.back().cur_pos;
    Ren::Vec3f end_dir = Ren::Normalize(bones_.back().cur_pos - bones_[bones_.size() - 2].cur_pos);
    //Ren::Vec3f diff = cur_goal_ - end_pos;
    Ren::Vec3f x = bones_[bones_.size() - 2].cur_pos + Ren::Dot(cur_goal_ - bones_[bones_.size() - 2].cur_pos, end_dir) * end_dir;
    if (Ren::Dot(x - bones_[bones_.size() - 2].cur_pos, end_dir) < 0) {
        x = bones_[bones_.size() - 2].cur_pos;
    }
    Ren::Vec3f diff = cur_goal_ - x;
    float error = Ren::Length(diff);

    int iterations = 0;

    while (error > 0.001f && iterations < 1000) {
        const float h = 0.025f;

        bool locked = true;

        for (size_t i = 0; i < bones_.size() - 1; i++) {
            Ren::Vec3f j_row = Ren::Cross(bones_[i].cur_rot_axis, Ren::Normalize(bones_[i].cur_pos - x));

            float delta = Ren::Dot(j_row, diff) * h;

            if (std::abs(delta) > 0.00001f) {
                locked = false;
            }

            bones_[i].angle = clamp(bones_[i].angle + delta, bones_[i].min_angle, bones_[i].max_angle);
        }

        if (locked) {
            for (size_t i = 0; i < bones_.size() - 1; i++) {
                bones_[i].angle += 0.001f * h * dis(gen);
            }
        }

        UpdateBones();

        end_pos = bones_.back().cur_pos;
        end_dir = Ren::Normalize(bones_.back().cur_pos - bones_[bones_.size() - 2].cur_pos);
        //diff = cur_goal_ - end_pos;
        x = bones_[bones_.size() - 2].cur_pos + Ren::Dot(cur_goal_ - bones_[bones_.size() - 2].cur_pos, end_dir) * end_dir;
        if (Ren::Dot(x - bones_[bones_.size() - 2].cur_pos, end_dir) < 0) {
            x = bones_[bones_.size() - 2].cur_pos;
        }
        diff = cur_goal_ - x;
        error = Ren::Length(diff);
        iterations++;
    }

    //if (iterations) {
    //LOGI("Iterations: %i\tError: %f", iterations_, Ren::Length(diff));
    iterations_ = iterations;
    error_ = error;
    //}
}

void GSIKTest::HandleInput(InputManager::Event evt) {
    using namespace GSIKTestInternal;

    switch (evt.type) {
    case InputManager::RAW_INPUT_P1_DOWN:
        view_grabbed_ = true;
        cur_goal_ = { 2 * evt.point.x / ctx_->w() - 1.0f, 2 * (ctx_->h() - evt.point.y) / ctx_->h() - 1.0f, 0.0f };
        break;
    case InputManager::RAW_INPUT_P1_UP: {
        view_grabbed_ = false;
    }
    break;
    case InputManager::RAW_INPUT_P1_MOVE:
        if (view_grabbed_) {
            cur_goal_ = { 2 * evt.point.x / ctx_->w() - 1.0f, 2 * (ctx_->h() - evt.point.y) / ctx_->h() - 1.0f, 0.0f };

            view_angle_ += 0.5f * evt.move.dx;
        }
        break;
    case InputManager::RAW_INPUT_KEY_DOWN:

        break;
    case InputManager::RAW_INPUT_KEY_UP:
        break;
    case InputManager::RAW_INPUT_RESIZE:

        break;
    default:
        break;
    }
}
