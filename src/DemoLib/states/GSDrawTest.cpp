#include "GSDrawTest.h"

#include <fstream>
#include <memory>

#include <Eng/GameStateManager.h>
#include <Gui/Renderer.h>
#include <Ren/Context.h>
#include <Ren/GL.h>
#include <Ren/Utils.h>
#include <Sys/AssetFile.h>
#include <Sys/Json.h>
#include <Sys/Log.h>
#include <Sys/MemBuf.h>
#include <Sys/Time_.h>

#include "../Gui/FontStorage.h"
#include "../Viewer.h"
#include "../Renderer/Renderer.h"
#include "../Scene/SceneManager.h"

namespace GSDrawTestInternal {
const float FORWARD_SPEED = 0.5f;

const int MAX_CMD_LINES = 8;

#if defined(__ANDROID__)
const char SCENE_NAME[] = "assets/scenes/"
#else
const char SCENE_NAME[] = "assets_pc/scenes/"
#endif
    "jap_house.json";
}

GSDrawTest::GSDrawTest(GameBase *game) : game_(game) {
    state_manager_  = game->GetComponent<GameStateManager>(STATE_MANAGER_KEY);
    ctx_            = game->GetComponent<Ren::Context>(REN_CONTEXT_KEY);

    renderer_       = game->GetComponent<Renderer>(RENDERER_KEY);
    scene_manager_  = game->GetComponent<SceneManager>(SCENE_MANAGER_KEY);

    ui_renderer_    = game->GetComponent<Gui::Renderer>(UI_RENDERER_KEY);
    ui_root_        = game->GetComponent<Gui::BaseElement>(UI_ROOT_KEY);

    const auto fonts = game->GetComponent<FontStorage>(UI_FONTS_KEY);
    font_ = fonts->FindFont("main_font");
}

GSDrawTest::~GSDrawTest() {

}

void GSDrawTest::Enter() {
    using namespace GSDrawTestInternal;

    LOGI("GSDrawTest: Loading scene!");
    LoadScene(SCENE_NAME);

    cmdline_history_.resize(MAX_CMD_LINES, "~");

    auto state_manager = state_manager_.lock();

    std::weak_ptr<GSDrawTest> weak_this = std::dynamic_pointer_cast<GSDrawTest>(state_manager->Peek());

    game_->RegisterCommand("wireframe", [weak_this](const std::vector<std::string> &args) -> bool {
        auto shrd_this = weak_this.lock();
        if (shrd_this) {
            uint32_t flags = shrd_this->renderer_->render_flags();
            flags ^= EnableWireframe;
            shrd_this->renderer_->set_render_flags(flags);
        }
        return true;
    });

    game_->RegisterCommand("culling", [weak_this](const std::vector<std::string> &args) -> bool {
        auto shrd_this = weak_this.lock();
        if (shrd_this) {
            uint32_t flags = shrd_this->renderer_->render_flags();
            flags ^= EnableCulling;
            shrd_this->renderer_->set_render_flags(flags);
        }
        return true;
    });

    game_->RegisterCommand("pt", [weak_this](const std::vector<std::string> &args) ->bool {
        auto shrd_this = weak_this.lock();
        if (shrd_this) {
            shrd_this->use_pt_ = !shrd_this->use_pt_;
            if (shrd_this->use_pt_) {
                shrd_this->scene_manager_->InitScene_PT();
                shrd_this->invalidate_view_ = true;
            }
        }
        return true;
    });

    game_->RegisterCommand("lm", [weak_this](const std::vector<std::string> &args) ->bool {
        auto shrd_this = weak_this.lock();
        if (shrd_this) {
            shrd_this->use_lm_ = !shrd_this->use_lm_;
            if (shrd_this->use_lm_) {
                shrd_this->scene_manager_->InitScene_PT();
                shrd_this->scene_manager_->ResetLightmaps_PT();
                shrd_this->invalidate_view_ = true;
            }
        }
        return true;
    });

    game_->RegisterCommand("debug_cull", [weak_this](const std::vector<std::string> &args) -> bool {
        auto shrd_this = weak_this.lock();
        if (shrd_this) {
            uint32_t flags = shrd_this->renderer_->render_flags();
            flags ^= DebugCulling;
            shrd_this->renderer_->set_render_flags(flags);
        }
        return true;
    });

    game_->RegisterCommand("debug_shadow", [weak_this](const std::vector<std::string> &args) -> bool {
        auto shrd_this = weak_this.lock();
        if (shrd_this) {
            uint32_t flags = shrd_this->renderer_->render_flags();
            flags ^= DebugShadow;
            shrd_this->renderer_->set_render_flags(flags);
        }
        return true;
    });

    game_->RegisterCommand("debug_reduce", [weak_this](const std::vector<std::string> &args) -> bool {
        auto shrd_this = weak_this.lock();
        if (shrd_this) {
            uint32_t flags = shrd_this->renderer_->render_flags();
            flags ^= DebugReduce;
            shrd_this->renderer_->set_render_flags(flags);
        }
        return true;
    });

    game_->RegisterCommand("debug_lights", [weak_this](const std::vector<std::string> &args) -> bool {
        auto shrd_this = weak_this.lock();
        if (shrd_this) {
            uint32_t flags = shrd_this->renderer_->render_flags();
            flags ^= DebugLights;
            shrd_this->renderer_->set_render_flags(flags);
        }
        return true;
    });

    game_->RegisterCommand("debug_decals", [weak_this](const std::vector<std::string> &args) -> bool {
        auto shrd_this = weak_this.lock();
        if (shrd_this) {
            uint32_t flags = shrd_this->renderer_->render_flags();
            flags ^= DebugDecals;
            shrd_this->renderer_->set_render_flags(flags);
        }
        return true;
    });

    game_->RegisterCommand("debug_deferred", [weak_this](const std::vector<std::string> &args) -> bool {
        auto shrd_this = weak_this.lock();
        if (shrd_this) {
            uint32_t flags = shrd_this->renderer_->render_flags();
            flags ^= DebugDeferred;
            shrd_this->renderer_->set_render_flags(flags);
        }
        return true;
    });

    game_->RegisterCommand("debug_blur", [weak_this](const std::vector<std::string> &args) -> bool {
        auto shrd_this = weak_this.lock();
        if (shrd_this) {
            uint32_t flags = shrd_this->renderer_->render_flags();
            flags ^= DebugBlur;
            shrd_this->renderer_->set_render_flags(flags);
        }
        return true;
    });

    game_->RegisterCommand("debug_ssao", [weak_this](const std::vector<std::string> &args) -> bool {
        auto shrd_this = weak_this.lock();
        if (shrd_this) {
            uint32_t flags = shrd_this->renderer_->render_flags();
            flags ^= DebugSSAO;
            shrd_this->renderer_->set_render_flags(flags);
        }
        return true;
    });

    game_->RegisterCommand("debug_timings", [weak_this](const std::vector<std::string> &args) -> bool {
        auto shrd_this = weak_this.lock();
        if (shrd_this) {
            uint32_t flags = shrd_this->renderer_->render_flags();
            flags ^= DebugTimings;
            shrd_this->renderer_->set_render_flags(flags);
        }
        return true;
    });
}

void GSDrawTest::LoadScene(const char *name) {
    Sys::AssetFile in_scene(name);
    size_t scene_size = in_scene.size();

    std::unique_ptr<uint8_t[]> scene_data(new uint8_t[scene_size]);
    in_scene.Read((char *)&scene_data[0], scene_size);

    Sys::MemBuf mem(&scene_data[0], scene_size);
    std::istream in_stream(&mem);

    //std::ifstream in_scene(name, std::ios::binary);

    JsObject js_scene;
    if (!js_scene.Read(in_stream)) {
        throw std::runtime_error("Cannot load scene!");
    }

    scene_manager_->LoadScene(js_scene);

    if (js_scene.Has("camera")) {
        const JsObject &js_cam = (const JsObject &)js_scene.at("camera");
        if (js_cam.Has("view_origin")) {
            const JsArray &js_orig = (const JsArray &)js_cam.at("view_origin");
            view_origin_[0] = (float)((const JsNumber &)js_orig.at(0)).val;
            view_origin_[1] = (float)((const JsNumber &)js_orig.at(1)).val;
            view_origin_[2] = (float)((const JsNumber &)js_orig.at(2)).val;
        }

        if (js_cam.Has("view_dir")) {
            const JsArray &js_dir = (const JsArray &)js_cam.at("view_dir");
            view_dir_[0] = (float)((const JsNumber &)js_dir.at(0)).val;
            view_dir_[1] = (float)((const JsNumber &)js_dir.at(1)).val;
            view_dir_[2] = (float)((const JsNumber &)js_dir.at(2)).val;
        }
    }

    /*view_origin_[0] = 17.834387f;
    view_origin_[1] = 4.055820f;
    view_origin_[2] = 14.737459f;

    view_dir_[0] = -0.351112f;
    view_dir_[1] = -0.208540f;
    view_dir_[2] = -0.912816f;*/
}

void GSDrawTest::Exit() {

}

void GSDrawTest::Draw(float dt_s) {
    using namespace GSDrawTestInternal;

    if (use_lm_) {
        if (!scene_manager_->PrepareLightmaps_PT()) {
            // Lightmap creation finished, convert textures
            Viewer::PrepareAssets();
            // Reload scene
            LoadScene(SCENE_NAME);
            // Switch back to normal mode
            use_lm_ = false;
        }
    } else if (use_pt_) {
        scene_manager_->SetupView_PT(view_origin_, (view_origin_ + view_dir_), Ren::Vec3f{ 0.0f, 1.0f, 0.0f });
        if (invalidate_view_) {
            scene_manager_->Clear_PT();
            invalidate_view_ = false;
        }
        scene_manager_->Draw_PT();
    } else {
        scene_manager_->SetupView(view_origin_, (view_origin_ + view_dir_), Ren::Vec3f{ 0.0f, 1.0f, 0.0f });
        scene_manager_->Draw();
    }

    //LOGI("(%f %f %f) (%f %f %f)", view_origin_[0], view_origin_[1], view_origin_[2],
    //                              view_dir_[0], view_dir_[1], view_dir_[2]);

    {
        // ui draw
        ui_renderer_->BeginDraw();

        if (cmdline_enabled_) {
            int font_height = (int)(0.5f * font_->height(ui_root_.get()) * game_->height);
#if defined(USE_GL_RENDER)
            glEnable(GL_SCISSOR_TEST);
            glScissor(0, game_->height - MAX_CMD_LINES * font_height, game_->width, MAX_CMD_LINES * font_height);

            glClearColor(0, 0.5f, 0.5f, 1);
            glClear(GL_COLOR_BUFFER_BIT);
            glDisable(GL_SCISSOR_TEST);
#endif
            float cur_y = 1.0f - font_->height(ui_root_.get());

            for (const auto &cmd : cmdline_history_) {
                font_->DrawText(ui_renderer_.get(), cmd.c_str(), { -1, cur_y }, ui_root_.get());
                cur_y -= font_->height(ui_root_.get());
            }
        }

        if (!use_pt_ && !use_lm_) {
            auto render_flags = scene_manager_->render_flags();
            auto render_info = scene_manager_->render_info();
            auto front_info = scene_manager_->frontend_info();
            auto back_info = scene_manager_->backend_info();

            uint64_t front_dur = front_info.end_timepoint_us - front_info.start_timepoint_us,
                        back_dur = back_info.cpu_end_timepoint_us - back_info.cpu_start_timepoint_us;

            LOGI("Frontend: %04lld\tBackend(cpu): %04lld", (long long)front_dur, (long long)back_dur);

            const char delimiter[] = "------------------";
            char text_buffer[256];

            float vertical_offset = 0.65f;

            {
                uint64_t cur_frame_time = Sys::GetTimeUs();

                double last_frame_dur = (cur_frame_time - last_frame_time_) * 0.000001;
                double last_frame_fps = 1.0 / last_frame_dur;

                last_frame_time_ = cur_frame_time;

                const double alpha = 0.05;
                cur_fps_ = alpha * last_frame_fps + (1.0 - alpha) * cur_fps_;

                sprintf(text_buffer, "        fps: %.1f", cur_fps_);
                font_->DrawText(ui_renderer_.get(), text_buffer, { -1.0f, vertical_offset }, ui_root_.get());
            }

            {
                vertical_offset -= font_->height(ui_root_.get());
                font_->DrawText(ui_renderer_.get(), delimiter, { -1.0f, vertical_offset }, ui_root_.get());

                vertical_offset -= font_->height(ui_root_.get());
                sprintf(text_buffer, "   occ_rast: %u us", front_info.occluders_time_us);
                font_->DrawText(ui_renderer_.get(), text_buffer, { -1.0f, vertical_offset }, ui_root_.get());

                vertical_offset -= font_->height(ui_root_.get());
                sprintf(text_buffer, "main_gather: %u us", front_info.main_gather_time_us);
                font_->DrawText(ui_renderer_.get(), text_buffer, { -1.0f, vertical_offset }, ui_root_.get());

                vertical_offset -= font_->height(ui_root_.get());
                sprintf(text_buffer, "shad_gather: %u us", front_info.shadow_gather_time_us);
                font_->DrawText(ui_renderer_.get(), text_buffer, { -1.0f, vertical_offset }, ui_root_.get());

                vertical_offset -= font_->height(ui_root_.get());
                sprintf(text_buffer, "item_assign: %u us", front_info.items_assignment_time_us);
                font_->DrawText(ui_renderer_.get(), text_buffer, { -1.0f, vertical_offset }, ui_root_.get());
            }

            {
                vertical_offset -= font_->height(ui_root_.get());
                font_->DrawText(ui_renderer_.get(), delimiter, { -1.0f, vertical_offset }, ui_root_.get());

                vertical_offset -= font_->height(ui_root_.get());
                sprintf(text_buffer, "shadow_maps: %u us", back_info.shadow_time_us);
                font_->DrawText(ui_renderer_.get(), text_buffer, { -1.0f, vertical_offset }, ui_root_.get());

                vertical_offset -= font_->height(ui_root_.get());
                sprintf(text_buffer, " depth_pass: %u us", back_info.depth_pass_time_us);
                font_->DrawText(ui_renderer_.get(), text_buffer, { -1.0f, vertical_offset }, ui_root_.get());

                vertical_offset -= font_->height(ui_root_.get());
                sprintf(text_buffer, "       ssao: %u us", back_info.ao_pass_time_us);
                font_->DrawText(ui_renderer_.get(), text_buffer, { -1.0f, vertical_offset }, ui_root_.get());

                vertical_offset -= font_->height(ui_root_.get());
                sprintf(text_buffer, "opaque_pass: %u us", back_info.opaque_pass_time_us);
                font_->DrawText(ui_renderer_.get(), text_buffer, { -1.0f, vertical_offset }, ui_root_.get());

                vertical_offset -= font_->height(ui_root_.get());
                sprintf(text_buffer, "  refl_pass: %u us", back_info.refl_pass_time_us);
                font_->DrawText(ui_renderer_.get(), text_buffer, { -1.0f, vertical_offset }, ui_root_.get());

                vertical_offset -= font_->height(ui_root_.get());
                sprintf(text_buffer, "  blur_pass: %u us", back_info.blur_pass_time_us);
                font_->DrawText(ui_renderer_.get(), text_buffer, { -1.0f, vertical_offset }, ui_root_.get());

                vertical_offset -= font_->height(ui_root_.get());
                sprintf(text_buffer, "  blit_pass: %u us", back_info.blit_pass_time_us);
                font_->DrawText(ui_renderer_.get(), text_buffer, { -1.0f, vertical_offset }, ui_root_.get());

                uint32_t gpu_total_us = (uint32_t)(back_info.gpu_end_timepoint_us - back_info.gpu_start_timepoint_us);

                vertical_offset -= font_->height(ui_root_.get());
                sprintf(text_buffer, "  gpu_total: %u us", gpu_total_us);
                font_->DrawText(ui_renderer_.get(), text_buffer, { -1.0f, vertical_offset }, ui_root_.get());
            }

            if (render_flags & (DebugLights | DebugDecals)) {
                vertical_offset -= font_->height(ui_root_.get());
                font_->DrawText(ui_renderer_.get(), delimiter, { -1.0f, vertical_offset }, ui_root_.get());

                vertical_offset -= font_->height(ui_root_.get());
                sprintf(text_buffer, " lights_cnt: %u", render_info.lights_count);
                font_->DrawText(ui_renderer_.get(), text_buffer, { -1.0f, vertical_offset }, ui_root_.get());

                vertical_offset -= font_->height(ui_root_.get());
                sprintf(text_buffer, "lights_data: %u kb", (render_info.lights_data_size / 1024));
                font_->DrawText(ui_renderer_.get(), text_buffer, { -1.0f, vertical_offset }, ui_root_.get());

                vertical_offset -= font_->height(ui_root_.get());
                sprintf(text_buffer, " decals_cnt: %u", render_info.decals_count);
                font_->DrawText(ui_renderer_.get(), text_buffer, { -1.0f, vertical_offset }, ui_root_.get());

                vertical_offset -= font_->height(ui_root_.get());
                sprintf(text_buffer, "decals_data: %u kb", (render_info.decals_data_size / 1024));
                font_->DrawText(ui_renderer_.get(), text_buffer, { -1.0f, vertical_offset }, ui_root_.get());

                vertical_offset -= font_->height(ui_root_.get());
                sprintf(text_buffer, " cells_data: %u kb", (render_info.cells_data_size / 1024));
                font_->DrawText(ui_renderer_.get(), text_buffer, { -1.0f, vertical_offset }, ui_root_.get());

                vertical_offset -= font_->height(ui_root_.get());
                sprintf(text_buffer, " items_data: %u kb", (render_info.items_data_size / 1024));
                font_->DrawText(ui_renderer_.get(), text_buffer, { -1.0f, vertical_offset }, ui_root_.get());
            }

            if (render_flags & DebugTimings) {
                double prev_cpu_start = double(prev_front_info_.start_timepoint_us),
                       prev_cpu_end = double(prev_front_info_.end_timepoint_us),
                       prev_gpu_start = double(prev_back_info_.gpu_start_timepoint_us),
                       prev_gpu_end = double(prev_back_info_.gpu_end_timepoint_us),
                       next_cpu_start = double(front_info.start_timepoint_us),
                       next_cpu_end = double(front_info.end_timepoint_us),
                       next_gpu_start = double(back_info.gpu_start_timepoint_us),
                       next_gpu_end = double(back_info.gpu_end_timepoint_us);

                prev_gpu_start -= double(prev_back_info_.gpu_cpu_time_diff_us);
                prev_gpu_end -= double(prev_back_info_.gpu_cpu_time_diff_us);
                next_gpu_start -= double(back_info.gpu_cpu_time_diff_us);
                next_gpu_end -= double(back_info.gpu_cpu_time_diff_us);

                prev_cpu_end -= prev_cpu_start;
                prev_gpu_start -= prev_cpu_start;
                prev_gpu_end -= prev_cpu_start;
                next_cpu_start -= prev_cpu_start;
                next_cpu_end -= prev_cpu_start;
                next_gpu_start -= prev_cpu_start;
                next_gpu_end -= prev_cpu_start;
                prev_cpu_start = 0.0;

                double dur = 0.0;
                int cc = 0;

                while (dur < std::max(next_cpu_end, next_gpu_end)) {
                    dur += 1000000.0 / 60.0;
                    cc++;
                }

                prev_cpu_end /= dur;
                prev_gpu_start /= dur;
                prev_gpu_end /= dur;

                next_cpu_start /= dur;
                next_cpu_end /= dur;
                next_gpu_start /= dur;
                next_gpu_end /= dur;

                text_buffer[0] = '[';
                text_buffer[101] = ']';

                for (int i = 0; i < 100; i++) {
                    double t = double(i) / 100;

                    if ((t >= prev_cpu_start && t <= prev_cpu_end) ||
                        (t >= next_cpu_start && t <= next_cpu_end)) {
                        text_buffer[i + 1] = 'F';
                    } else {
                        text_buffer[i + 1] = '_';
                    }
                }

                sprintf(&text_buffer[102], " [2 frames, %.1f ms]", cc * 1000.0 / 60.0);

                vertical_offset -= font_->height(ui_root_.get());
                font_->DrawText(ui_renderer_.get(), delimiter, { -1.0f, vertical_offset }, ui_root_.get());

                vertical_offset -= font_->height(ui_root_.get());
                font_->DrawText(ui_renderer_.get(), text_buffer, { -1.0f, vertical_offset }, ui_root_.get());

                //LOGI("%s", text_buffer);

                for (int i = 0; i < 100; i++) {
                    double t = double(i) / 100;

                    if ((t >= prev_gpu_start && t <= prev_gpu_end) ||
                        (t >= next_gpu_start && t <= next_gpu_end)) {
                        text_buffer[i + 1] = 'B';
                    } else {
                        text_buffer[i + 1] = '_';
                    }
                }

                vertical_offset -= font_->height(ui_root_.get());
                font_->DrawText(ui_renderer_.get(), text_buffer, { -1.0f, vertical_offset }, ui_root_.get());

                //LOGI("%s", text_buffer);

                prev_front_info_ = front_info;
                prev_back_info_ = back_info;
            }
        }

        //font_->DrawText(ui_renderer_.get(), s2.c_str(), { -1, 1.0f - 2 * font_->height(ui_root_.get()) }, ui_root_.get());
        //font_->DrawText(ui_renderer_.get(), s3.c_str(), { -1, 1.0f - 3 * font_->height(ui_root_.get()) }, ui_root_.get());
        //font_->DrawText(ui_renderer_.get(), s4.c_str(), { -1, 1.0f - 4 * font_->height(ui_root_.get()) }, ui_root_.get());

        ui_renderer_->EndDraw();
    }

    ctx_->ProcessTasks();
}

void GSDrawTest::Update(int dt_ms) {
    using namespace GSDrawTestInternal;

    Ren::Vec3f up = { 0, 1, 0 };
    Ren::Vec3f side = Normalize(Cross(view_dir_, up));

    float fwd_speed = std::max(std::min(fwd_press_speed_ + fwd_touch_speed_, FORWARD_SPEED), -FORWARD_SPEED);
    float side_speed = std::max(std::min(side_press_speed_ + side_touch_speed_, FORWARD_SPEED), -FORWARD_SPEED);

    view_origin_ += view_dir_ * fwd_speed;
    view_origin_ += side * side_speed;

    if (std::abs(fwd_speed) > 0.0f || std::abs(side_speed) > 0.0f) {
        invalidate_view_ = true;
    }
}

void GSDrawTest::HandleInput(InputManager::Event evt) {
    using namespace Ren;
    using namespace GSDrawTestInternal;

    // pt switch for touch controls
    if (evt.type == InputManager::RAW_INPUT_P1_DOWN || evt.type == InputManager::RAW_INPUT_P2_DOWN) {
        if (evt.point.x > ctx_->w() * 0.9f && evt.point.y < ctx_->h() * 0.1f) {
            auto new_time = Sys::GetTicks();
            if (new_time - click_time_ < 400) {
                use_pt_ = !use_pt_;
                if (use_pt_) {
                    scene_manager_->InitScene_PT();
                    invalidate_view_ = true;
                }
                click_time_ = 0;
            } else {
                click_time_ = new_time;
            }
        }
    }

    switch (evt.type) {
    case InputManager::RAW_INPUT_P1_DOWN:
        if (evt.point.x < ctx_->w() / 3 && move_pointer_ == 0) {
            move_pointer_ = 1;
        } else if (view_pointer_ == 0) {
            view_pointer_ = 1;
        }
        break;
    case InputManager::RAW_INPUT_P2_DOWN:
        if (evt.point.x < ctx_->w() / 3 && move_pointer_ == 0) {
            move_pointer_ = 2;
        } else if (view_pointer_ == 0) {
            view_pointer_ = 2;
        }
        break;
    case InputManager::RAW_INPUT_P1_UP:
        if (move_pointer_ == 1) {
            move_pointer_ = 0;
            fwd_touch_speed_ = 0;
            side_touch_speed_ = 0;
        } else if (view_pointer_ == 1) {
            view_pointer_ = 0;
        }
        break;
    case InputManager::RAW_INPUT_P2_UP:
        if (move_pointer_ == 2) {
            move_pointer_ = 0;
            fwd_touch_speed_ = 0;
            side_touch_speed_ = 0;
        } else if (view_pointer_ == 2) {
            view_pointer_ = 0;
        }
        break;
    case InputManager::RAW_INPUT_P1_MOVE:
        if (move_pointer_ == 1) {
            side_touch_speed_ += evt.move.dx * 0.01f;
            side_touch_speed_ = std::max(std::min(side_touch_speed_, FORWARD_SPEED), -FORWARD_SPEED);

            fwd_touch_speed_ -= evt.move.dy * 0.01f;
            fwd_touch_speed_ = std::max(std::min(fwd_touch_speed_, FORWARD_SPEED), -FORWARD_SPEED);
        } else if (view_pointer_ == 1) {
            Vec3f up = { 0, 1, 0 };
            Vec3f side = Normalize(Cross(view_dir_, up));
            up = Cross(side, view_dir_);

            Mat4f rot;
            rot = Rotate(rot, -0.01f * evt.move.dx, up);
            rot = Rotate(rot, -0.01f * evt.move.dy, side);

            auto rot_m3 = Mat3f(rot);
            view_dir_ = rot_m3 * view_dir_;

            invalidate_view_ = true;
        }
        break;
    case InputManager::RAW_INPUT_P2_MOVE:
        if (move_pointer_ == 2) {
            side_touch_speed_ += evt.move.dx * 0.01f;
            side_touch_speed_ = std::max(std::min(side_touch_speed_, FORWARD_SPEED), -FORWARD_SPEED);

            fwd_touch_speed_ -= evt.move.dy * 0.01f;
            fwd_touch_speed_ = std::max(std::min(fwd_touch_speed_, FORWARD_SPEED), -FORWARD_SPEED);
        } else if (view_pointer_ == 2) {
            Vec3f up = { 0, 1, 0 };
            Vec3f side = Normalize(Cross(view_dir_, up));
            up = Cross(side, view_dir_);

            Mat4f rot;
            rot = Rotate(rot, 0.01f * evt.move.dx, up);
            rot = Rotate(rot, 0.01f * evt.move.dy, side);

            auto rot_m3 = Mat3f(rot);
            view_dir_ = rot_m3 * view_dir_;

            invalidate_view_ = true;
        }
        break;
    case InputManager::RAW_INPUT_KEY_DOWN: {
        if (evt.key == InputManager::RAW_INPUT_BUTTON_UP || (evt.raw_key == 'w' && (!cmdline_enabled_ || view_pointer_))) {
            fwd_press_speed_ = FORWARD_SPEED;
        } else if (evt.key == InputManager::RAW_INPUT_BUTTON_DOWN || (evt.raw_key == 's' && (!cmdline_enabled_ || view_pointer_))) {
            fwd_press_speed_ = -FORWARD_SPEED;
        } else if (evt.key == InputManager::RAW_INPUT_BUTTON_LEFT || (evt.raw_key == 'a' && (!cmdline_enabled_ || view_pointer_))) {
            side_press_speed_ = -FORWARD_SPEED;
        } else if (evt.key == InputManager::RAW_INPUT_BUTTON_RIGHT || (evt.raw_key == 'd' && (!cmdline_enabled_ || view_pointer_))) {
            side_press_speed_ = FORWARD_SPEED;
        } else if (evt.key == InputManager::RAW_INPUT_BUTTON_SPACE) {

        } else if (evt.key == InputManager::RAW_INPUT_BUTTON_SHIFT) {
            shift_down_ = true;
        }
    }
    break;
    case InputManager::RAW_INPUT_KEY_UP: {
        if (evt.key == InputManager::RAW_INPUT_BUTTON_UP || (evt.raw_key == 'w' && (!cmdline_enabled_ || view_pointer_))) {
            fwd_press_speed_ = 0;
        } else if (evt.key == InputManager::RAW_INPUT_BUTTON_DOWN || (evt.raw_key == 's' && (!cmdline_enabled_ || view_pointer_))) {
            fwd_press_speed_ = 0;
        } else if (evt.key == InputManager::RAW_INPUT_BUTTON_LEFT || (evt.raw_key == 'a' && (!cmdline_enabled_ || view_pointer_))) {
            side_press_speed_ = 0;
        } else if (evt.key == InputManager::RAW_INPUT_BUTTON_RIGHT || (evt.raw_key == 'd' && (!cmdline_enabled_ || view_pointer_))) {
            side_press_speed_ = 0;
        } else if (evt.key == InputManager::RAW_INPUT_BUTTON_SHIFT) {
            shift_down_ = false;
        } else if (evt.key == InputManager::RAW_INPUT_BUTTON_BACKSPACE) {
            if (!cmdline_history_.back().empty()) {
                cmdline_history_.back().pop_back();
            }
        } else if (evt.key == InputManager::RAW_INPUT_BUTTON_RETURN) {
            if (cmdline_enabled_) {
                game_->ExecuteCommand(cmdline_history_.back(), {});
                cmdline_history_.emplace_back();
                if (cmdline_history_.size() > MAX_CMD_LINES) {
                    cmdline_history_.erase(cmdline_history_.begin());
                }
            }
        } else if (evt.raw_key == (int)'`') {
            cmdline_enabled_ = !cmdline_enabled_;
            if (cmdline_enabled_) {
                if (!cmdline_history_.back().empty()) {
                    cmdline_history_.emplace_back();
                    if (cmdline_history_.size() > MAX_CMD_LINES) {
                        cmdline_history_.erase(cmdline_history_.begin());
                    }
                }
            }
        } else if (cmdline_enabled_) {
            char ch = (char)evt.raw_key;
            if (shift_down_) {
                if (ch == '-') ch = '_';
            }
            cmdline_history_.back() += ch;
        }
    }
    case InputManager::RAW_INPUT_RESIZE:
        break;
    default:
        break;
    }
}
