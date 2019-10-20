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

#include "../Gui/DebugInfoUI.h"
#include "../Gui/FontStorage.h"
#include "../Viewer.h"
#include "../Renderer/Renderer.h"
#include "../Scene/SceneManager.h"
#include "../Utils/Cmdline.h"

namespace GSDrawTestInternal {
const int MAX_CMD_LINES = 8;

#if defined(__ANDROID__)
const char SCENE_NAME[] = "assets/scenes/"
#else
const char SCENE_NAME[] = "assets_pc/scenes/"
#endif
    "font_test.json";
    //"skin_test.json";
    //"living_room_gumroad.json";
    //"bistro.json";
    //"pbr_test.json";

const bool USE_TWO_THREADS = true;
}

GSDrawTest::GSDrawTest(GameBase *game) : game_(game) {
    using namespace GSDrawTestInternal;

    cmdline_        = game->GetComponent<Cmdline>(CMDLINE_KEY);

    state_manager_  = game->GetComponent<GameStateManager>(STATE_MANAGER_KEY);
    ctx_            = game->GetComponent<Ren::Context>(REN_CONTEXT_KEY);

    renderer_       = game->GetComponent<Renderer>(RENDERER_KEY);
    scene_manager_  = game->GetComponent<SceneManager>(SCENE_MANAGER_KEY);

    ui_renderer_    = game->GetComponent<Gui::Renderer>(UI_RENDERER_KEY);
    ui_root_        = game->GetComponent<Gui::BaseElement>(UI_ROOT_KEY);

    const std::shared_ptr<FontStorage> fonts = game->GetComponent<FontStorage>(UI_FONTS_KEY);
    font_ = fonts->FindFont("main_font");

    debug_ui_       = game->GetComponent<DebugInfoUI>(UI_DEBUG_KEY);

    swap_interval_  = game->GetComponent<TimeInterval>(SWAP_TIMER_KEY);

    // Prepare cam for probes updating
    temp_probe_cam_.Perspective(90.0f, 1.0f, 0.1f, 10000.0f);
}

GSDrawTest::~GSDrawTest() {

}

void GSDrawTest::Enter() {
    using namespace GSDrawTestInternal;

    if (USE_TWO_THREADS) {
        background_thread_ = std::thread(std::bind(&GSDrawTest::BackgroundProc, this));
    }

    LOGI("GSDrawTest: Loading scene!");
    LoadScene(SCENE_NAME);

    {   // Create temporary buffer to update probes
        FrameBuf::ColorAttachmentDesc desc;
        desc.format = Ren::RawRGB16F;
        desc.filter = Ren::NoFilter;
        desc.repeat = Ren::ClampToEdge;

        const int res = scene_manager_->scene_data().probe_storage.res();
        temp_probe_buf_ = FrameBuf(res, res, &desc, 1, { FrameBuf::DepthNone });
    }

    cmdline_history_.resize(MAX_CMD_LINES, "~");

    std::shared_ptr<GameStateManager> state_manager = state_manager_.lock();

    std::weak_ptr<GSDrawTest> weak_this = std::dynamic_pointer_cast<GSDrawTest>(state_manager->Peek());

    cmdline_->RegisterCommand("wireframe", [weak_this](int argc, Cmdline::ArgData *argv) -> bool {
        auto shrd_this = weak_this.lock();
        if (shrd_this) {
            uint32_t flags = shrd_this->renderer_->render_flags();
            flags ^= DebugWireframe;
            shrd_this->renderer_->set_render_flags(flags);
        }
        return true;
    });

    cmdline_->RegisterCommand("culling", [weak_this](int argc, Cmdline::ArgData *argv) -> bool {
        auto shrd_this = weak_this.lock();
        if (shrd_this) {
            uint32_t flags = shrd_this->renderer_->render_flags();
            flags ^= EnableCulling;
            shrd_this->renderer_->set_render_flags(flags);
        }
        return true;
    });

    cmdline_->RegisterCommand("lightmap", [weak_this](int argc, Cmdline::ArgData *argv) -> bool {
        auto shrd_this = weak_this.lock();
        if (shrd_this) {
            uint32_t flags = shrd_this->renderer_->render_flags();
            flags ^= EnableLightmap;
            shrd_this->renderer_->set_render_flags(flags);
        }
        return true;
    });

    cmdline_->RegisterCommand("lights", [weak_this](int argc, Cmdline::ArgData *argv) -> bool {
        auto shrd_this = weak_this.lock();
        if (shrd_this) {
            uint32_t flags = shrd_this->renderer_->render_flags();
            flags ^= EnableLights;
            shrd_this->renderer_->set_render_flags(flags);
        }
        return true;
    });

    cmdline_->RegisterCommand("decals", [weak_this](int argc, Cmdline::ArgData *argv) -> bool {
        auto shrd_this = weak_this.lock();
        if (shrd_this) {
            uint32_t flags = shrd_this->renderer_->render_flags();
            flags ^= EnableDecals;
            shrd_this->renderer_->set_render_flags(flags);
        }
        return true;
    });

    cmdline_->RegisterCommand("shadows", [weak_this](int argc, Cmdline::ArgData *argv) -> bool {
        auto shrd_this = weak_this.lock();
        if (shrd_this) {
            uint32_t flags = shrd_this->renderer_->render_flags();
            flags ^= EnableShadows;
            shrd_this->renderer_->set_render_flags(flags);
        }
        return true;
    });

    cmdline_->RegisterCommand("fxaa", [weak_this](int argc, Cmdline::ArgData *argv) -> bool {
        auto shrd_this = weak_this.lock();
        if (shrd_this) {
            uint32_t flags = shrd_this->renderer_->render_flags();
            flags ^= EnableFxaa;
            shrd_this->renderer_->set_render_flags(flags);
        }
        return true;
    });

    cmdline_->RegisterCommand("pt", [weak_this](int argc, Cmdline::ArgData *argv) ->bool {
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

    cmdline_->RegisterCommand("lm", [weak_this](int argc, Cmdline::ArgData *argv) ->bool {
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

    cmdline_->RegisterCommand("oit", [weak_this](int argc, Cmdline::ArgData *argv) -> bool {
        auto shrd_this = weak_this.lock();
        if (shrd_this) {
            uint32_t flags = shrd_this->renderer_->render_flags();
            flags ^= EnableOIT;
            shrd_this->renderer_->set_render_flags(flags);
        }
        return true;
    });

    cmdline_->RegisterCommand("debug_cull", [weak_this](int argc, Cmdline::ArgData *argv) -> bool {
        auto shrd_this = weak_this.lock();
        if (shrd_this) {
            uint32_t flags = shrd_this->renderer_->render_flags();
            flags ^= DebugCulling;
            shrd_this->renderer_->set_render_flags(flags);
        }
        return true;
    });

    cmdline_->RegisterCommand("debug_shadow", [weak_this](int argc, Cmdline::ArgData *argv) -> bool {
        auto shrd_this = weak_this.lock();
        if (shrd_this) {
            uint32_t flags = shrd_this->renderer_->render_flags();
            flags ^= DebugShadow;
            shrd_this->renderer_->set_render_flags(flags);
        }
        return true;
    });

    cmdline_->RegisterCommand("debug_reduce", [weak_this](int argc, Cmdline::ArgData *argv) -> bool {
        auto shrd_this = weak_this.lock();
        if (shrd_this) {
            uint32_t flags = shrd_this->renderer_->render_flags();
            flags ^= DebugReduce;
            shrd_this->renderer_->set_render_flags(flags);
        }
        return true;
    });

    cmdline_->RegisterCommand("debug_lights", [weak_this](int argc, Cmdline::ArgData *argv) -> bool {
        auto shrd_this = weak_this.lock();
        if (shrd_this) {
            uint32_t flags = shrd_this->renderer_->render_flags();
            flags ^= DebugLights;
            shrd_this->renderer_->set_render_flags(flags);
        }
        return true;
    });

    cmdline_->RegisterCommand("debug_decals", [weak_this](int argc, Cmdline::ArgData *argv) -> bool {
        auto shrd_this = weak_this.lock();
        if (shrd_this) {
            uint32_t flags = shrd_this->renderer_->render_flags();
            flags ^= DebugDecals;
            shrd_this->renderer_->set_render_flags(flags);
        }
        return true;
    });

    cmdline_->RegisterCommand("debug_deferred", [weak_this](int argc, Cmdline::ArgData *argv) -> bool {
        auto shrd_this = weak_this.lock();
        if (shrd_this) {
            uint32_t flags = shrd_this->renderer_->render_flags();
            flags ^= DebugDeferred;
            shrd_this->renderer_->set_render_flags(flags);
        }
        return true;
    });

    cmdline_->RegisterCommand("debug_blur", [weak_this](int argc, Cmdline::ArgData *argv) -> bool {
        auto shrd_this = weak_this.lock();
        if (shrd_this) {
            uint32_t flags = shrd_this->renderer_->render_flags();
            flags ^= DebugBlur;
            shrd_this->renderer_->set_render_flags(flags);
        }
        return true;
    });

    cmdline_->RegisterCommand("debug_ssao", [weak_this](int argc, Cmdline::ArgData *argv) -> bool {
        auto shrd_this = weak_this.lock();
        if (shrd_this) {
            uint32_t flags = shrd_this->renderer_->render_flags();
            flags ^= DebugSSAO;
            shrd_this->renderer_->set_render_flags(flags);
        }
        return true;
    });

    cmdline_->RegisterCommand("debug_timings", [weak_this](int argc, Cmdline::ArgData *argv) -> bool {
        auto shrd_this = weak_this.lock();
        if (shrd_this) {
            uint32_t flags = shrd_this->renderer_->render_flags();
            flags ^= DebugTimings;
            shrd_this->renderer_->set_render_flags(flags);
        }
        return true;
    });

    cmdline_->RegisterCommand("debug_bvh", [weak_this](int argc, Cmdline::ArgData *argv) -> bool {
        auto shrd_this = weak_this.lock();
        if (shrd_this) {
            uint32_t flags = shrd_this->renderer_->render_flags();
            flags ^= DebugBVH;
            shrd_this->renderer_->set_render_flags(flags);
        }
        return true;
    });

    cmdline_->RegisterCommand("debug_probes", [weak_this](int argc, Cmdline::ArgData *argv) -> bool {
        auto shrd_this = weak_this.lock();
        if (shrd_this) {
            uint32_t flags = shrd_this->renderer_->render_flags();
            flags ^= DebugProbes;
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

    try {
        scene_manager_->LoadScene(js_scene);
    } catch (std::exception &e) {
        LOGI("Error loading scene: %s", e.what());
    }

    cam_follow_path_.clear();
    cam_follow_point_ = 0;
    cam_follow_param_ = 0.0f;

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

        if (js_cam.Has("fwd_speed")) {
            const JsNumber &js_fwd_speed = (const JsNumber &)js_cam.at("fwd_speed");
            max_fwd_speed_ = (float)js_fwd_speed.val;
        }

        if (js_cam.Has("fov")) {
            const JsNumber &js_fov = (const JsNumber &)js_cam.at("fov");
            view_fov_ = (float)js_fov.val;
        }

        if (js_cam.Has("follow_path")) {
            const JsArray &js_points = (const JsArray &)js_cam.at("follow_path");
            for (const JsElement &el : js_points.elements) {
                const JsArray &js_point = (const JsArray &)el;

                const JsNumber &x = (const JsNumber &)js_point.at(0),
                               &y = (const JsNumber &)js_point.at(1),
                               &z = (const JsNumber &)js_point.at(2);

                cam_follow_path_.emplace_back((float)x.val, (float)y.val, (float)z.val);
            }
        }
    }

    SceneData &scene = scene_manager_->scene_data();

    {
        char wolf_name[] = "wolf00";

        for (int j = 0; j < 4; j++) {
            for (int i = 0; i < 8; i++) {
                int index = j * 8 + i;

                wolf_name[4] = '0' + j;
                wolf_name[5] = '0' + i;

                uint32_t wolf_index = scene_manager_->FindObject(wolf_name);
                wolf_indices_[index] = wolf_index;

                if (wolf_index != 0xffffffff) {
                    SceneObject *wolf = scene_manager_->GetObject(wolf_index);

                    uint32_t mask = CompDrawableBit | CompAnimStateBit;
                    if ((wolf->comp_mask & mask) == mask) {
                        auto *as = (AnimState *)scene.comp_store[CompAnimState]->Get(wolf->components[CompAnimState]);
                        as->anim_time_s = 4.0f * (float(rand()) / RAND_MAX);
                    }
                }
            }
        }
    }

    {
        font_meshes_[0] = scene_manager_->FindObject("font_mesh0");
        font_meshes_[1] = scene_manager_->FindObject("font_mesh1");
        font_meshes_[2] = scene_manager_->FindObject("font_mesh2");
        font_meshes_[3] = scene_manager_->FindObject("font_mesh3");
    }

    {
        char scooter_name[] = "scooter_00";

        for (int j = 0; j < 2; j++) {
            for (int i = 0; i < 8; i++) {
                scooter_name[8] = '0' + j;
                scooter_name[9] = '0' + i;

                uint32_t scooter_index = scene_manager_->FindObject(scooter_name);
                scooter_indices_[j * 8 + i] = scooter_index;
            }
        }
    }

    {
        char sophia_name[] = "sophia_00";

        for (int i = 0; i < 2; i++) {
            sophia_name[8] = '0' + i;

            uint32_t sophia_index = scene_manager_->FindObject(sophia_name);
            sophia_indices_[i] = sophia_index;
        }
    }

    {
        char eric_name[] = "eric_00";

        for (int i = 0; i < 2; i++) {
            eric_name[6] = '0' + i;

            uint32_t eric_index = scene_manager_->FindObject(eric_name);
            eric_indices_[i] = eric_index;
        }
    }

    probes_dirty_ = true;

    //view_origin_ = { 1.6273377f, 2.3731651f, 0.9859568f };// { 5.0337372f, 1.1156000f, -6.3602295f };
    //view_dir_ = { 0.1691239f, 0.0714032f, 0.9829926f };// { -0.7934315f, -0.1813142f, 0.5810235f };
}

void GSDrawTest::Exit() {
    using namespace GSDrawTestInternal;

    if (USE_TWO_THREADS) {
        if (background_thread_.joinable()) {
            shutdown_ = notified_ = true;
            thr_notify_.notify_all();
            background_thread_.join();
        }
    }
}

void GSDrawTest::Draw(uint64_t dt_us) {
    using namespace GSDrawTestInternal;

    if (cmdline_enabled_) {
        // Process comandline input
        for (const InputManager::Event &evt : cmdline_input_) {
            if (evt.key == InputManager::RAW_INPUT_BUTTON_BACKSPACE) {
                if (!cmdline_history_.back().empty()) {
                    cmdline_history_.back().pop_back();
                }

            } else if (evt.key == InputManager::RAW_INPUT_BUTTON_RETURN) {
                cmdline_->Execute(cmdline_history_.back().c_str());

                cmdline_history_.emplace_back();
                if (cmdline_history_.size() > MAX_CMD_LINES) {
                    cmdline_history_.erase(cmdline_history_.begin());
                }
            } else if (evt.raw_key == (int)'`') {
                if (!cmdline_history_.back().empty()) {
                    cmdline_history_.emplace_back();
                    if (cmdline_history_.size() > MAX_CMD_LINES) {
                        cmdline_history_.erase(cmdline_history_.begin());
                    }
                }
            } else {
                char ch = (char)evt.raw_key;
                if (shift_down_) {
                    if (ch == '-') ch = '_';
                }
                cmdline_history_.back() += ch;
            }
        }

        cmdline_input_.clear();
    }

    {
        int back_list;

        if (USE_TWO_THREADS) {
            std::unique_lock<std::mutex> lock(mtx_);
            while (notified_) {
                thr_done_.wait(lock);
            }

            if (use_lm_) {
                int w, h;
                const float *preview_pixels = nullptr;
                if (scene_manager_->PrepareLightmaps_PT(&preview_pixels, &w, &h)) {
                    if (preview_pixels) {
                        renderer_->BlitPixels(preview_pixels, w, h, Ren::RawRGBA32F);
                    }
                } else {
                    // Lightmap creation finished, convert textures
                    Viewer::PrepareAssets("pc");
                    // Reload scene
                    LoadScene(SCENE_NAME);
                    // Switch back to normal mode
                    use_lm_ = false;
                }

                back_list = -1;
            } else if (use_pt_) {
                scene_manager_->SetupView_PT(view_origin_, (view_origin_ + view_dir_), Ren::Vec3f{ 0.0f, 1.0f, 0.0f }, view_fov_);
                if (invalidate_view_) {
                    scene_manager_->Clear_PT();
                    invalidate_view_ = false;
                }
                int w, h;
                const float *preview_pixels = scene_manager_->Draw_PT(&w, &h);
                if (preview_pixels) {
                    renderer_->BlitPixelsTonemap(preview_pixels, w, h, Ren::RawRGBA32F);
                }

                back_list = -1;
            } else {
                back_list = front_list_;
                front_list_ = (front_list_ + 1) % 2;

                if (probes_dirty_ && scene_manager_->load_complete()) {
                    // Perform first update of reflection probes
                    update_all_probes_ = true;
                    probes_dirty_ = false;
                }

                const SceneData &scene_data = scene_manager_->scene_data();

                if (probe_to_update_sh_) {
                    bool done = renderer_->BlitProjectSH(scene_data.probe_storage, probe_to_update_sh_->layer_index,
                                                         probe_sh_update_iteration_, *probe_to_update_sh_);
                    probe_sh_update_iteration_++;

                    if (done) {
                        probe_sh_update_iteration_ = 0;
                        probe_to_update_sh_ = nullptr;
                    }
                }

                // Render probe cubemap
                if (probe_to_render_) {
                    for (int i = 0; i < 6; i++) {
                        renderer_->ExecuteDrawList(temp_probe_lists_[i], &temp_probe_buf_);
                        renderer_->BlitToTempProbeFace(temp_probe_buf_, scene_data.probe_storage, i);
                    }

                    renderer_->BlitPrefilterFromTemp(scene_data.probe_storage, probe_to_render_->layer_index);

                    probe_to_update_sh_ = probe_to_render_;
                    probe_to_render_ = nullptr;
                }
            }

            notified_ = true;
            thr_notify_.notify_one();
        } else {
            scene_manager_->SetupView(view_origin_, (view_origin_ + view_dir_), Ren::Vec3f{ 0.0f, 1.0f, 0.0f }, view_fov_);
            // Gather drawables for list 0
            UpdateFrame(0);
            back_list = 0;
        }

        if (back_list != -1) {
            // Render current frame (from back list)
            renderer_->ExecuteDrawList(main_view_lists_[back_list]);
        }
    }

    //LOGI("{ %.7f, %.7f, %.7f } { %.7f, %.7f, %.7f }", view_origin_[0], view_origin_[1], view_origin_[2],
    //                                                  view_dir_[0], view_dir_[1], view_dir_[2]);

    {   // ui draw
        ui_renderer_->BeginDraw();

        const float font_height = font_->height(ui_root_.get());

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

            for (const std::string &cmd : cmdline_history_) {
                font_->DrawText(ui_renderer_.get(), cmd.c_str(), { -1, cur_y }, ui_root_.get());
                cur_y -= font_height;
            }
        }

        {
            char buf[128];
            sprintf(buf, " Shader (current is %i): 0 - passthrough (baseline); 1 - MSDF; 2 - mixed (passthrough); 3 - mixed (MSDF)", cur_font_test_);
            font_->DrawText(ui_renderer_.get(), buf, { -1.0f, 0.98f - font_height }, ui_root_.get());
        }

        if (!use_pt_ && !use_lm_) {
            int back_list = (front_list_ + 1) % 2;

            uint32_t render_flags = renderer_->render_flags();
            FrontendInfo front_info = main_view_lists_[back_list].frontend_info;
            BackendInfo back_info = renderer_->backend_info();

            uint64_t front_dur = front_info.end_timepoint_us - front_info.start_timepoint_us,
                     back_dur = back_info.cpu_end_timepoint_us - back_info.cpu_start_timepoint_us;

            //LOGI("Frontend: %04lld\tBackend(cpu): %04lld", (long long)front_dur, (long long)back_dur);

            uint64_t transp_dur = back_info.gpu_end_timepoint_us - back_info.gpu_start_timepoint_us;// back_info.opaque_pass_time_us + back_info.transp_pass_time_us;
            LOGI("Transparent draw: %04lu us", transp_dur);

            ItemsInfo items_info;
            items_info.light_sources_count  = main_view_lists_[back_list].light_sources.count;
            items_info.decals_count         = main_view_lists_[back_list].decals.count;
            items_info.probes_count         = main_view_lists_[back_list].probes.count;
            items_info.items_total          = main_view_lists_[back_list].items.count;

            debug_ui_->UpdateInfo(front_info, back_info, items_info, *swap_interval_, render_flags);
            //debug_ui_->Draw(ui_renderer_.get());
        }

        ui_renderer_->EndDraw();
    }

    ctx_->ProcessTasks();
}

void GSDrawTest::Update(uint64_t dt_us) {
    using namespace GSDrawTestInternal;

    Ren::Vec3f up = { 0, 1, 0 };
    Ren::Vec3f side = Normalize(Cross(view_dir_, up));

    if (cam_follow_path_.size() < 3) {
        float fwd_speed = std::max(std::min(fwd_press_speed_ + fwd_touch_speed_, max_fwd_speed_), -max_fwd_speed_);
        float side_speed = std::max(std::min(side_press_speed_ + side_touch_speed_, max_fwd_speed_), -max_fwd_speed_);

        view_origin_ += view_dir_ * fwd_speed;
        view_origin_ += side * side_speed;

        if (std::abs(fwd_speed) > 0.0f || std::abs(side_speed) > 0.0f) {
            invalidate_view_ = true;
        }
    } else {
        int next_point = (cam_follow_point_ + 1) % cam_follow_path_.size();

        {   // update param
            const Ren::Vec3f &p1 = cam_follow_path_[cam_follow_point_],
                             &p2 = cam_follow_path_[next_point];

            cam_follow_param_ += 0.000005f * dt_us / Ren::Distance(p1, p2);
            while (cam_follow_param_ > 1.0f) {
                cam_follow_point_ = (cam_follow_point_ + 1) % cam_follow_path_.size();
                cam_follow_param_ -= 1.0f;
            }
        }

        next_point = (cam_follow_point_ + 1) % cam_follow_path_.size();

        const Ren::Vec3f &p1 = cam_follow_path_[cam_follow_point_],
                         &p2 = cam_follow_path_[next_point];

        view_origin_ = 0.95f * view_origin_ + 0.05f * Ren::Mix(p1, p2, cam_follow_param_);
        view_dir_ = 0.9f * view_dir_ + 0.1f * Ren::Normalize(p2 - view_origin_);
        view_dir_ = Ren::Normalize(view_dir_);

        invalidate_view_ = true;
    }

    uint32_t mask = CompTransformBit | CompDrawableBit;
#if 0
    static float t = 0.0f;
    t += 0.04f;

    //const uint32_t monkey_ids[] = { 12, 13, 14, 15, 16 };
    const uint32_t monkey_ids[] = { 28, 29, 30, 31, 32 };

    SceneObject *monkey1 = scene_manager_->GetObject(monkey_ids[0]);
    if ((monkey1->comp_mask & mask) == mask) {
        auto *tr = monkey1->tr.get();
        tr->mat = Ren::Translate(tr->mat, Ren::Vec3f{ 0.0f, 0.0f + 0.02f * std::cos(t), 0.05f + 0.04f * std::cos(t) });
    }

    SceneObject *monkey2 = scene_manager_->GetObject(monkey_ids[1]);
    if ((monkey2->comp_mask & mask) == mask) {
        auto *tr = monkey2->tr.get();
        tr->mat = Ren::Translate(tr->mat, Ren::Vec3f{ 0.0f, 0.0f + 0.02f * std::cos(1.5f + t), 0.05f + 0.04f * std::cos(t) });
    }

    SceneObject *monkey3 = scene_manager_->GetObject(monkey_ids[2]);
    if ((monkey3->comp_mask & mask) == mask) {
        auto *tr = monkey3->tr.get();
        tr->mat = Ren::Translate(tr->mat, Ren::Vec3f{ 0.0f, 0.0f + 0.02f * std::cos(1.5f + t), 0.05f + 0.04f * std::cos(t) });
    }

    SceneObject *monkey4 = scene_manager_->GetObject(monkey_ids[3]);
    if ((monkey4->comp_mask & mask) == mask) {
        auto *tr = monkey4->tr.get();
        tr->mat = Ren::Translate(tr->mat, Ren::Vec3f{ 0.0f, 0.0f + 0.02f * std::cos(t), 0.05f + 0.04f * std::cos(t) });
    }

    SceneObject *monkey5 = scene_manager_->GetObject(monkey_ids[4]);
    if ((monkey5->comp_mask & mask) == mask) {
        auto *tr = monkey5->tr.get();
        tr->mat = Ren::Translate(tr->mat, Ren::Vec3f{ 0.0f, 0.0f + 0.02f * std::cos(t), 0.05f + 0.04f * std::cos(t) });
    }

    scene_manager_->InvalidateObjects(monkey_ids, 5, CompTransformBit);
#endif
    SceneData &scene = scene_manager_->scene_data();

    if (scooter_indices_[0] != 0xffffffff) {
        const Ren::Vec3f rot_center = { 44.5799f, 0.15f, 24.7763f };

        scooters_angle_ += 0.0000005f * dt_us;

        for (int i = 0; i < 16; i++) {
            if (scooter_indices_[i] == 0xffffffff) break;

            SceneObject *scooter = scene_manager_->GetObject(scooter_indices_[i]);

            uint32_t mask = CompTransform;
            if ((scooter->comp_mask & mask) == mask) {
                auto *tr = (Transform *)scene.comp_store[CompTransform]->Get(scooter->components[CompTransform]);

                tr->mat = Ren::Mat4f{ 1.0f };
                tr->mat = Ren::Translate(tr->mat, rot_center);
                
                if (i < 8) {
                    // inner circle
                    tr->mat = Ren::Rotate(tr->mat, scooters_angle_ + i * 0.25f * Ren::Pi<float>(), { 0.0f, 1.0f, 0.0f });
                    tr->mat = Ren::Translate(tr->mat, { 6.5f, 0.0f, 0.0f });
                } else {
                    // outer circle
                    tr->mat = Ren::Rotate(tr->mat, -scooters_angle_ + (i - 8) * 0.25f * Ren::Pi<float>(), { 0.0f, 1.0f, 0.0f });
                    tr->mat = Ren::Translate(tr->mat, { -8.5f, 0.0f, 0.0f });
                }
            }
        }

        scene_manager_->InvalidateObjects(scooter_indices_, 16, CompTransformBit);
    }
}

void GSDrawTest::HandleInput(const InputManager::Event &evt) {
    using namespace Ren;
    using namespace GSDrawTestInternal;

    // pt switch for touch controls
    if (evt.type == InputManager::RAW_INPUT_P1_DOWN || evt.type == InputManager::RAW_INPUT_P2_DOWN) {
        if (evt.point.x > ctx_->w() * 0.9f && evt.point.y < ctx_->h() * 0.1f) {
            uint32_t new_time = Sys::GetTimeMs();
            if (new_time - click_time_ < 400) {
                /*use_pt_ = !use_pt_;
                if (use_pt_) {
                    scene_manager_->InitScene_PT();
                    invalidate_view_ = true;
                }*/

                if (probes_to_update_.empty()) {
                    int obj_count = (int)scene_manager_->scene_data().objects.size();
                    for (int i = 0; i < obj_count; i++) {
                        SceneObject *obj = scene_manager_->GetObject(i);
                        if (obj->comp_mask & CompProbeBit) {
                            probes_to_update_.push_back(i);
                        }
                    }
                }

                click_time_ = 0;
            } else {
                click_time_ = new_time;
            }
        }
    }

    switch (evt.type) {
    case InputManager::RAW_INPUT_P1_DOWN:
        /*if (evt.point.x < ctx_->w() / 3 && move_pointer_ == 0) {
            move_pointer_ = 1;
        } else if (view_pointer_ == 0) {
            view_pointer_ = 1;
        }*/
        break;
    case InputManager::RAW_INPUT_P2_DOWN:
        /*if (evt.point.x < ctx_->w() / 3 && move_pointer_ == 0) {
            move_pointer_ = 2;
        } else if (view_pointer_ == 0) {
            view_pointer_ = 2;
        }*/
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
            side_touch_speed_ += evt.move.dx * 0.002f;
            side_touch_speed_ = std::max(std::min(side_touch_speed_, max_fwd_speed_), -max_fwd_speed_);

            fwd_touch_speed_ -= evt.move.dy * 0.002f;
            fwd_touch_speed_ = std::max(std::min(fwd_touch_speed_, max_fwd_speed_), -max_fwd_speed_);
        } else if (view_pointer_ == 1) {
            Vec3f up = { 0, 1, 0 };
            Vec3f side = Normalize(Cross(view_dir_, up));
            up = Cross(side, view_dir_);

            Mat4f rot;
            rot = Rotate(rot, -0.005f * evt.move.dx, up);
            rot = Rotate(rot, -0.005f * evt.move.dy, side);

            auto rot_m3 = Mat3f(rot);
            view_dir_ = rot_m3 * view_dir_;

            invalidate_view_ = true;
        }
        break;
    case InputManager::RAW_INPUT_P2_MOVE:
        if (move_pointer_ == 2) {
            side_touch_speed_ += evt.move.dx * 0.002f;
            side_touch_speed_ = std::max(std::min(side_touch_speed_, max_fwd_speed_), -max_fwd_speed_);

            fwd_touch_speed_ -= evt.move.dy * 0.002f;
            fwd_touch_speed_ = std::max(std::min(fwd_touch_speed_, max_fwd_speed_), -max_fwd_speed_);
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
            fwd_press_speed_ = max_fwd_speed_;
        } else if (evt.key == InputManager::RAW_INPUT_BUTTON_DOWN || (evt.raw_key == 's' && (!cmdline_enabled_ || view_pointer_))) {
            fwd_press_speed_ = -max_fwd_speed_;
        } else if (evt.key == InputManager::RAW_INPUT_BUTTON_LEFT || (evt.raw_key == 'a' && (!cmdline_enabled_ || view_pointer_))) {
            side_press_speed_ = -max_fwd_speed_;
        } else if (evt.key == InputManager::RAW_INPUT_BUTTON_RIGHT || (evt.raw_key == 'd' && (!cmdline_enabled_ || view_pointer_))) {
            side_press_speed_ = max_fwd_speed_;
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
        } else if (evt.key == InputManager::RAW_INPUT_BUTTON_SPACE) {
            if (probes_to_update_.empty()) {
                int obj_count = (int)scene_manager_->scene_data().objects.size();
                for (int i = 0; i < obj_count; i++) {
                    SceneObject *obj = scene_manager_->GetObject(i);
                    if (obj->comp_mask & CompProbeBit) {
                        probes_to_update_.push_back(i);
                    }
                }
            }
        } else if (evt.key == InputManager::RAW_INPUT_BUTTON_SHIFT) {
            shift_down_ = false;
        } else if (evt.key == InputManager::RAW_INPUT_BUTTON_BACKSPACE) {
            if (cmdline_enabled_) {
                cmdline_input_.push_back(evt);
            }
        } else if (evt.key == InputManager::RAW_INPUT_BUTTON_RETURN) {
            if (cmdline_enabled_) {
                cmdline_input_.push_back(evt);
            }
        } else if (evt.raw_key == (int)'`') {
            cmdline_enabled_ = !cmdline_enabled_;
            if (cmdline_enabled_) {
                cmdline_input_.push_back(evt);
            }
        } else if (cmdline_enabled_) {
            cmdline_input_.push_back(evt);
        } else if (evt.raw_key >= 48 && evt.raw_key < 52) {
            cur_font_test_ = (int)evt.raw_key - 48;
            view_origin_ = { (evt.raw_key - 48) * 10.0f, 0.0f, 10.0f };
            view_dir_ = { 0.0f, 0.0f, -1.0f };
        }
    }
    case InputManager::RAW_INPUT_RESIZE:
        break;
    default:
        break;
    }
}

void GSDrawTest::BackgroundProc() {
    std::unique_lock<std::mutex> lock(mtx_);
    while (!shutdown_) {
        while (!notified_) {
            thr_notify_.wait(lock);
        }

        // Gather drawables for list 1
        UpdateFrame(front_list_);

        notified_ = false;
        thr_done_.notify_one();
    }
}

void GSDrawTest::UpdateFrame(int list_index) {
    {   // Update loop with fixed timestep
        auto input_manager = game_->GetComponent<InputManager>(INPUT_MANAGER_KEY);

        FrameInfo &fr = fr_info_;

        fr.cur_time_us = Sys::GetTimeUs();
        if (fr.cur_time_us < fr.prev_time_us) fr.prev_time_us = 0;
        fr.delta_time_us = fr.cur_time_us - fr.prev_time_us;
        if (fr.delta_time_us > 200000) {
            fr.delta_time_us = 200000;
        }
        fr.prev_time_us = fr.cur_time_us;
        fr.time_acc_us += fr.delta_time_us;

        uint64_t poll_time_point = fr.cur_time_us - fr.time_acc_us;

        while (fr.time_acc_us >= UPDATE_DELTA) {
            InputManager::Event evt;
            while (input_manager->PollEvent(poll_time_point, evt)) {
                this->HandleInput(evt);
            }

            this->Update(UPDATE_DELTA);
            fr.time_acc_us -= UPDATE_DELTA;

            poll_time_point += UPDATE_DELTA;
        }

        fr.time_fract = double(fr.time_acc_us) / UPDATE_DELTA;
    }


    float delta_time_s = fr_info_.delta_time_us * 0.000001f;

    // test test test
    TestUpdateAnims(delta_time_s);

    // Update camera
    scene_manager_->SetupView(view_origin_, (view_origin_ + view_dir_), Ren::Vec3f{ 0.0f, 1.0f, 0.0f }, view_fov_);

    // Update invalidated objects
    scene_manager_->UpdateObjects();

    if (!use_pt_ && !use_lm_) {
        if (update_all_probes_) {
            if (probes_to_update_.empty()) {
                int obj_count = (int)scene_manager_->scene_data().objects.size();
                for (int i = 0; i < obj_count; i++) {
                    SceneObject *obj = scene_manager_->GetObject(i);
                    if (obj->comp_mask & CompProbeBit) {
                        probes_to_update_.push_back(i);
                    }
                }
            }
            update_all_probes_ = false;
        }

        if (!probes_to_update_.empty() && !probe_to_render_ && !probe_to_update_sh_) {
            LOGI("Updating probe");
            SceneObject *probe_obj = scene_manager_->GetObject(probes_to_update_.back());
            auto *probe = (LightProbe *)scene_manager_->scene_data().comp_store[CompProbe]->Get(probe_obj->components[CompProbe]);
            auto *probe_tr = (Transform *)scene_manager_->scene_data().comp_store[CompTransform]->Get(probe_obj->components[CompTransform]);

            Ren::Vec4f pos = { probe->offset[0], probe->offset[1], probe->offset[2], 1.0f };
            pos = probe_tr->mat * pos;
            pos /= pos[3];

            static const Ren::Vec3f axises[] = {
                { 1.0f,  0.0f,  0.0f },
                { -1.0f,  0.0f,  0.0f },
                { 0.0f,  1.0f,  0.0f },
                { 0.0f, -1.0f,  0.0f },
                { 0.0f,  0.0f,  1.0f },
                { 0.0f,  0.0f, -1.0f }
            };

            static const Ren::Vec3f ups[] = {
                { 0.0f, -1.0f, 0.0f },
                { 0.0f, -1.0f, 0.0f },
                { 0.0f,  0.0f, 1.0f },
                { 0.0f,  0.0f, -1.0f },
                { 0.0f, -1.0f, 0.0f },
                { 0.0f, -1.0f, 0.0f }
            };

            const Ren::Vec3f center = { pos[0], pos[1], pos[2] };

            for (int i = 0; i < 6; i++) {
                const Ren::Vec3f target = center + axises[i];
                temp_probe_cam_.SetupView(center, target, ups[i]);
                temp_probe_cam_.UpdatePlanes();

                temp_probe_lists_[i].render_flags = EnableZFill | EnableCulling | EnableLightmap | EnableLights | EnableDecals | EnableShadows | EnableProbes;

                renderer_->PrepareDrawList(scene_manager_->scene_data(), temp_probe_cam_, temp_probe_lists_[i]);
            }

            probe_to_render_ = probe;
            probes_to_update_.pop_back();
        }

        // Enable all flags, Renderer will mask out what is not enabled
        main_view_lists_[list_index].render_flags = 0xffffffff;

        renderer_->PrepareDrawList(scene_manager_->scene_data(), scene_manager_->main_cam(), main_view_lists_[list_index]);
    }
}

void GSDrawTest::TestUpdateAnims(float delta_time_s) {
    const SceneData &scene = scene_manager_->scene_data();

    if (wolf_indices_[0] != 0xffffffff) {
        for (int i = 0; i < 32; i++) {
            if (wolf_indices_[i] == 0xffffffff) break;

            SceneObject *wolf = scene_manager_->GetObject(wolf_indices_[i]);

            uint32_t mask = CompDrawableBit | CompAnimStateBit;
            if ((wolf->comp_mask & mask) == mask) {
                auto *dr = (Drawable *)scene.comp_store[CompDrawable]->Get(wolf->components[CompDrawable]);
                auto *as = (AnimState *)scene.comp_store[CompAnimState]->Get(wolf->components[CompAnimState]);

                as->anim_time_s += delta_time_s;

                Ren::Mesh *mesh = dr->mesh.get();
                Ren::Skeleton *skel = mesh->skel();

                skel->UpdateAnim(0, as->anim_time_s);
                skel->ApplyAnim(0);
                skel->UpdateBones(as->matr_palette);
            }
        }
    }

    if (font_meshes_[0] != 0xffffffff) {
        for (uint32_t font_mesh : font_meshes_) {
            SceneObject *font = scene_manager_->GetObject(font_mesh);

            uint32_t mask = CompDrawableBit | CompAnimStateBit;
            if ((font->comp_mask & mask) == mask) {
                auto *dr = (Drawable *)scene.comp_store[CompDrawable]->Get(font->components[CompDrawable]);
                auto *as = (AnimState *)scene.comp_store[CompAnimState]->Get(font->components[CompAnimState]);

                as->anim_time_s += delta_time_s;

                Ren::Mesh *mesh = dr->mesh.get();
                Ren::Skeleton *skel = mesh->skel();

                skel->UpdateAnim(0, as->anim_time_s);
                skel->ApplyAnim(0);
                skel->UpdateBones(as->matr_palette);
            }
        }
    }

    if (scooter_indices_[0] != 0xffffffff) {
        for (int i = 0; i < 16; i++) {
            if (scooter_indices_[i] == 0xffffffff) break;

            SceneObject *scooter = scene_manager_->GetObject(scooter_indices_[i]);

            uint32_t mask = CompDrawableBit | CompAnimStateBit;
            if ((scooter->comp_mask & mask) == mask) {
                auto *dr = (Drawable *)scene.comp_store[CompDrawable]->Get(scooter->components[CompDrawable]);
                auto *as = (AnimState *)scene.comp_store[CompAnimState]->Get(scooter->components[CompAnimState]);

                as->anim_time_s += delta_time_s;

                Ren::Mesh *mesh = dr->mesh.get();
                Ren::Skeleton *skel = mesh->skel();

                const int anim_index = (i < 8) ? 2 : 3;

                skel->UpdateAnim(anim_index, as->anim_time_s);
                skel->ApplyAnim(anim_index);
                skel->UpdateBones(as->matr_palette);
            }
        }
    }

    if (sophia_indices_[0] != 0xffffffff) {
        for (int i = 0; i < 2; i++) {
            if (sophia_indices_[i] == 0xffffffff) break;

            SceneObject *sophia = scene_manager_->GetObject(sophia_indices_[i]);

            uint32_t mask = CompDrawableBit | CompAnimStateBit;
            if ((sophia->comp_mask & mask) == mask) {
                auto *dr = (Drawable *)scene.comp_store[CompDrawable]->Get(sophia->components[CompDrawable]);
                auto *as = (AnimState *)scene.comp_store[CompAnimState]->Get(sophia->components[CompAnimState]);

                as->anim_time_s += delta_time_s;

                Ren::Mesh *mesh = dr->mesh.get();
                Ren::Skeleton *skel = mesh->skel();

                const int anim_index = 0;

                skel->UpdateAnim(anim_index, as->anim_time_s);
                skel->ApplyAnim(anim_index);
                skel->UpdateBones(as->matr_palette);
            }
        }
    }

    if (eric_indices_[0] != 0xffffffff) {
        for (int i = 0; i < 2; i++) {
            if (eric_indices_[i] == 0xffffffff) break;

            SceneObject *eric = scene_manager_->GetObject(eric_indices_[i]);

            uint32_t mask = CompDrawableBit | CompAnimStateBit;
            if ((eric->comp_mask & mask) == mask) {
                auto *dr = (Drawable *)scene.comp_store[CompDrawable]->Get(eric->components[CompDrawable]);
                auto *as = (AnimState *)scene.comp_store[CompAnimState]->Get(eric->components[CompAnimState]);

                as->anim_time_s += delta_time_s;

                Ren::Mesh *mesh = dr->mesh.get();
                Ren::Skeleton *skel = mesh->skel();

                const int anim_index = 0;

                skel->UpdateAnim(anim_index, as->anim_time_s);
                skel->ApplyAnim(anim_index);
                skel->UpdateBones(as->matr_palette);
            }
        }
    }
}