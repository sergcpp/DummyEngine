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

#include "../Viewer.h"
#include "../ui/FontStorage.h"

namespace GSIKTestInternal {
    const char *vs_shader = R"(
        /*
        ATTRIBUTES
            aVertexPosition : 0
        UNIFORMS
            uMVPMatrix : 0
        */

        attribute vec3 aVertexPosition;

        void main(void) {
            gl_Position = vec4(aVertexPosition, 1.0);
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
}

GSIKTest::GSIKTest(GameBase *game) : game_(game) {
    state_manager_  = game->GetComponent<GameStateManager>(STATE_MANAGER_KEY);
    ctx_            = game->GetComponent<Ren::Context>(REN_CONTEXT_KEY);

    ui_renderer_    = game->GetComponent<Gui::Renderer>(UI_RENDERER_KEY);
    ui_root_        = game->GetComponent<Gui::BaseElement>(UI_ROOT_KEY);

    const auto fonts = game->GetComponent<FontStorage>(UI_FONTS_KEY);
    font_ = fonts->FindFont("main_font");
}

GSIKTest::~GSIKTest() {
    
}

void GSIKTest::Enter() {
    using namespace GSIKTestInternal;

    Ren::eProgLoadStatus status;
    line_prog_ = ctx_->LoadProgramGLSL("line_prog", vs_shader, fs_shader, &status);
    assert(status == Ren::ProgCreatedFromData);

    auto to_radians = [](float deg) { return deg * std::acos(-1.0f) / 180.0f; };

    bones_.push_back(Bone{ Ren::Vec3f{ 0.0f }, 0.1f, 0.0f, -to_radians(45), to_radians(45) });
    bones_.push_back(Bone{ Ren::Vec3f{ 0.0f }, 0.1f, 0.0f, -to_radians(45), to_radians(45) });
    bones_.push_back(Bone{ Ren::Vec3f{ 0.0f }, 0.1f, 0.0f, -to_radians(45), to_radians(45) });
    bones_.push_back(Bone{ Ren::Vec3f{ 0.0f }, 0.1f, 0.0f, -to_radians(45), to_radians(45) });
    bones_.push_back(Bone{ Ren::Vec3f{ 0.0f }, 0.1f, 0.0f, -to_radians(45), to_radians(45) });
    bones_.push_back(Bone{ Ren::Vec3f{ 0.0f }, 0.1f, 0.0f, -to_radians(45), to_radians(45) });
    bones_.push_back(Bone{ Ren::Vec3f{ 0.0f }, 0.0f, 0.0f, -to_radians(45), to_radians(45) });
}

void GSIKTest::Exit() {

}

void GSIKTest::Draw(float dt_s) {
    using namespace GSIKTestInternal;

    {
        glClearColor(0, 0.2f, 0.2f, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDisable(GL_DEPTH_TEST);

        const auto *p = line_prog_.get();

        const GLuint pos_attrib = p->attribute("aVertexPosition").loc;

        const GLuint col_unif = p->uniform("col").loc;

        glUseProgram(p->prog_id());

        glUniform3f(col_unif, 0.0f, 1.0f, 1.0f);

        glEnableVertexAttribArray(pos_attrib);
        glVertexAttribPointer(pos_attrib, 3, GL_FLOAT, GL_FALSE, sizeof(Bone), &bones_[0].pos[0]);

        glDrawArrays(GL_LINE_STRIP, 0, (GLsizei)bones_.size());

        glUniform3f(col_unif, 1.0f, 1.0f, 1.0f);

        const float cross[] = { cur_goal_[0] - 0.025f, cur_goal_[1], cur_goal_[2],
                                cur_goal_[0] + 0.025f, cur_goal_[1], cur_goal_[2],
                                cur_goal_[0], cur_goal_[1] - 0.025f, cur_goal_[2],
                                cur_goal_[0], cur_goal_[1] + 0.025f, cur_goal_[2] };

        glVertexAttribPointer(pos_attrib, 3, GL_FLOAT, GL_FALSE, 0, &cross[0]);

        glDrawArrays(GL_LINES, 0, 4);
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
    Ren::Vec3f cur_pos = { 0.0f, -0.65f, 0.0f };
    Ren::Vec4f cur_dir = { 0.0f, 1.0f, 0.0f, 0.0f };
    for (auto &b : bones_) {
        b.pos = cur_pos;

        Ren::Vec4f dir = { 0.0f, 1.0f, 0.0f, 0.0f };

        Ren::Mat4f rot;
        rot = Ren::Rotate(rot, b.angle, Ren::Vec3f{ 0.0f, 0.0f, 1.0f });

        cur_dir = cur_dir * rot;

        cur_pos += Ren::Vec3f{ cur_dir[0], cur_dir[1], cur_dir[2] } *b.length;
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

    UpdateBones();

    /*goal_change_timer_ += dt_ms;
    if (goal_change_timer_ > 1000) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dis(-1.0f, 1.0f);

        prev_goal_ = next_goal_;
        next_goal_[0] = dis(gen);
        next_goal_[1] = dis(gen);
        next_goal_[2] = dis(gen);

        goal_change_timer_ -= 1000;
    }

    cur_goal_ = prev_goal_ + (next_goal_ - prev_goal_) * 0.001f * goal_change_timer_;*/


    Ren::Vec3f end_pos = bones_.back().pos;
    Ren::Vec3f end_dir = Ren::Normalize(bones_.back().pos - bones_[bones_.size() - 2].pos);
    //Ren::Vec3f diff = cur_goal_ - end_pos;
    Ren::Vec3f x = bones_[bones_.size() - 2].pos + Ren::Dot(cur_goal_ - bones_[bones_.size() - 2].pos, end_dir) * end_dir;
    if (Ren::Dot(x - bones_[bones_.size() - 2].pos, end_dir) < 0) {
        x = bones_[bones_.size() - 2].pos;
    }
    Ren::Vec3f diff = cur_goal_ - x;
    float error = Ren::Length(diff);

    int iterations = 0;

    while (error > 0.001f && iterations < 1000) {
        Ren::Vec3f j_0 = -Ren::Cross(Ren::Vec3f{ 0.0f, 0.0f, 1.0f }, x - bones_[0].pos);
        Ren::Vec3f j_1 = -Ren::Cross(Ren::Vec3f{ 0.0f, 0.0f, 1.0f }, x - bones_[1].pos);
        Ren::Vec3f j_2 = -Ren::Cross(Ren::Vec3f{ 0.0f, 0.0f, 1.0f }, x - bones_[2].pos);
        Ren::Vec3f j_3 = -Ren::Cross(Ren::Vec3f{ 0.0f, 0.0f, 1.0f }, x - bones_[3].pos);
        Ren::Vec3f j_4 = -Ren::Cross(Ren::Vec3f{ 0.0f, 0.0f, 1.0f }, x - bones_[4].pos);
        Ren::Vec3f j_5 = -Ren::Cross(Ren::Vec3f{ 0.0f, 0.0f, 1.0f }, x - bones_[5].pos);

        /*Ren::Mat3f j_mat;
        j_mat[0] = j_0;
        j_mat[1] = j_1;
        j_mat[2] = j_2;

        //j_mat = Ren::Transpose(j_mat);

        Ren::Vec3f dO = j_mat * diff;*/

        const float h = 0.025f;

        bones_[0].angle = clamp(bones_[0].angle + Ren::Dot(j_0, diff) * h, bones_[0].min_angle, bones_[0].max_angle);
        bones_[1].angle = clamp(bones_[1].angle + Ren::Dot(j_1, diff) * h, bones_[1].min_angle, bones_[1].max_angle);
        bones_[2].angle = clamp(bones_[2].angle + Ren::Dot(j_2, diff) * h, bones_[2].min_angle, bones_[2].max_angle);
        bones_[3].angle = clamp(bones_[3].angle + Ren::Dot(j_2, diff) * h, bones_[3].min_angle, bones_[3].max_angle);
        bones_[4].angle = clamp(bones_[4].angle + Ren::Dot(j_2, diff) * h, bones_[4].min_angle, bones_[4].max_angle);
        bones_[5].angle = clamp(bones_[5].angle + Ren::Dot(j_2, diff) * h, bones_[5].min_angle, bones_[5].max_angle);

        UpdateBones();

        end_pos = bones_.back().pos;
        end_dir = Ren::Normalize(bones_.back().pos - bones_[bones_.size() - 2].pos);
        //diff = cur_goal_ - end_pos;
        x = bones_[bones_.size() - 2].pos + Ren::Dot(cur_goal_ - bones_[bones_.size() - 2].pos, end_dir) * end_dir;
        if (Ren::Dot(x - bones_[bones_.size() - 2].pos, end_dir) < 0) {
            x = bones_[bones_.size() - 2].pos;
        }
        diff = cur_goal_ - x;
        error = Ren::Length(diff);
        iterations++;
    }

    if (iterations) {
        //LOGI("Iterations: %i\tError: %f", iterations_, Ren::Length(diff));
        iterations_ = iterations;
        error_ = error;
    }
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
    } break;
    case InputManager::RAW_INPUT_P1_MOVE:
        if (view_grabbed_) {
            cur_goal_ = { 2 * evt.point.x / ctx_->w() - 1.0f, 2 * (ctx_->h() - evt.point.y) / ctx_->h() - 1.0f, 0.0f };
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
