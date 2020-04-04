#include "GSBaseState.h"

#include <cctype>

#include <fstream>
#include <memory>

#if !defined(RELEASE_FINAL) && !defined(__ANDROID__)
#include <vtune/ittnotify.h>
#endif

#include <Eng/GameStateManager.h>
#include <Eng/Random.h>
#include <Eng/Renderer/Renderer.h>
#include <Eng/Scene/SceneManager.h>
#include <Eng/Utils/Cmdline.h>
#include <Eng/Gui/Renderer.h>
#include <Ren/Context.h>
#include <Ren/GL.h>
#include <Ren/Utils.h>
#include <Sys/AssetFile.h>
#include <Sys/Json.h>
#include <Sys/MemBuf.h>
#include <Sys/Time_.h>

#include "../Gui/DebugInfoUI.h"
#include "../Gui/FontStorage.h"
#include "../Viewer.h"

namespace GSBaseStateInternal {
const int MAX_CMD_LINES = 8;
const bool USE_TWO_THREADS = true;
}

GSBaseState::GSBaseState(GameBase *game) : game_(game) {
    using namespace GSBaseStateInternal;

    cmdline_        = game->GetComponent<Cmdline>(CMDLINE_KEY);

    state_manager_  = game->GetComponent<GameStateManager>(STATE_MANAGER_KEY);
    ctx_            = game->GetComponent<Ren::Context>(REN_CONTEXT_KEY);
    log_            = game->GetComponent<Ren::ILog>(LOG_KEY);

    renderer_       = game->GetComponent<Renderer>(RENDERER_KEY);
    scene_manager_  = game->GetComponent<SceneManager>(SCENE_MANAGER_KEY);

    ui_renderer_    = game->GetComponent<Gui::Renderer>(UI_RENDERER_KEY);
    ui_root_        = game->GetComponent<Gui::BaseElement>(UI_ROOT_KEY);

    const std::shared_ptr<FontStorage> fonts = game->GetComponent<FontStorage>(UI_FONTS_KEY);
    font_ = fonts->FindFont("main_font");

    debug_ui_       = game->GetComponent<DebugInfoUI>(UI_DEBUG_KEY);

    swap_interval_  = game->GetComponent<TimeInterval>(SWAP_TIMER_KEY);

    random_         = game->GetComponent<Random>(RANDOM_KEY);

    // Prepare cam for probes updating
    temp_probe_cam_.Perspective(90.0f, 1.0f, 0.1f, 10000.0f);
    temp_probe_cam_.set_render_mask(uint32_t(Drawable::eDrVisibility::VisProbes));
}

void GSBaseState::Enter() {
    using namespace GSBaseStateInternal;

    if (USE_TWO_THREADS) {
        background_thread_ = std::thread(std::bind(&GSBaseState::BackgroundProc, this));
    }

    {   // Create temporary buffer to update probes
        FrameBuf::ColorAttachmentDesc desc;
        desc.format = Ren::eTexFormat::RawRGB16F;
        desc.filter = Ren::eTexFilter::NoFilter;
        desc.repeat = Ren::eTexRepeat::ClampToEdge;

        const int res = scene_manager_->scene_data().probe_storage.res();
        temp_probe_buf_ = FrameBuf(res, res, &desc, 1, { FrameBuf::eDepthFormat::DepthNone }, 1, ctx_->log());
    }

    cmdline_history_.resize(MAX_CMD_LINES, "~");

    std::shared_ptr<GameStateManager> state_manager = state_manager_.lock();
    std::weak_ptr<GSBaseState> weak_this = std::dynamic_pointer_cast<GSBaseState>(state_manager->Peek());

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

    cmdline_->RegisterCommand("msaa", [weak_this](int argc, Cmdline::ArgData *argv) -> bool {
        auto shrd_this = weak_this.lock();
        if (shrd_this) {
            uint32_t flags = shrd_this->renderer_->render_flags();
            flags ^= EnableMsaa;
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

    cmdline_->RegisterCommand("taa", [weak_this](int argc, Cmdline::ArgData *argv) -> bool {
        auto shrd_this = weak_this.lock();
        if (shrd_this) {
            uint32_t flags = shrd_this->renderer_->render_flags();
            flags ^= EnableTaa;
            if (flags & EnableTaa) {
                flags &= ~(EnableMsaa | EnableFxaa);
            }
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

    cmdline_->RegisterCommand("zfill", [weak_this](int argc, Cmdline::ArgData *argv) -> bool {
        auto shrd_this = weak_this.lock();
        if (shrd_this) {
            uint32_t flags = shrd_this->renderer_->render_flags();
            flags ^= EnableZFill;
            shrd_this->renderer_->set_render_flags(flags);
        }
        return true;
    });

    cmdline_->RegisterCommand("update_probes", [weak_this](int argc, Cmdline::ArgData *argv) -> bool {
        auto shrd_this = weak_this.lock();
        if (shrd_this) {
            SceneData &scene_data = shrd_this->scene_manager_->scene_data();

            const int
                    res = scene_data.probe_storage.res(),
                    capacity = scene_data.probe_storage.capacity();
            scene_data.probe_storage.Resize(Ren::eTexFormat::RawRGBA8888, res, capacity, shrd_this->ctx_->log());

            shrd_this->update_all_probes_ = true;
        }
        return true;
    });

    cmdline_->RegisterCommand("cache_probes", [weak_this](int argc, Cmdline::ArgData *argv) -> bool {
        auto shrd_this = weak_this.lock();
        if (shrd_this) {
            const SceneData &scene_data = shrd_this->scene_manager_->scene_data();
            
            const CompStorage *lprobes = scene_data.comp_store[CompProbe];
            SceneManager::WriteProbeCache("assets/textures/probes_cache", scene_data.name.c_str(),
                    scene_data.probe_storage, lprobes, shrd_this->ctx_->log());

            // probe textures were written, convert them
            Viewer::PrepareAssets("pc");
        }
        return true;
    });

    cmdline_->RegisterCommand("load", [weak_this](int argc, Cmdline::ArgData *argv) -> bool {
        if (argc != 2 || argv[1].type != Cmdline::eArgType::ArgString) return false;

        auto shrd_this = weak_this.lock();
        if (shrd_this) {
            char buf[1024];
            sprintf(buf, "%s/scenes/%.*s", ASSETS_BASE_PATH, (int)argv[1].str.len, argv[1].str.str);
            shrd_this->LoadScene(buf);
        }
        return true;
    });

    cmdline_->RegisterCommand("save", [weak_this](int argc, Cmdline::ArgData *argv) -> bool {
        auto shrd_this = weak_this.lock();
        if (shrd_this) {
            JsObject out_scene;

            shrd_this->SaveScene(out_scene);

            {   // Write output file
                std::string name_str;

                {   // get scene file name
                    const SceneData &scene_data = shrd_this->scene_manager_->scene_data();
                    name_str = scene_data.name.c_str();
                }

                {   // rotate backup files
                    for (int i = 7; i > 0; i--) {
                        const std::string
                            name1 = "assets/scenes/" + name_str + ".json" + std::to_string(i),
                            name2 = "assets/scenes/" + name_str + ".json" + std::to_string(i + 1);

                        int ret = std::rename(name1.c_str(), name2.c_str());
                        if (ret) {
                            shrd_this->ctx_->log()->Error("Failed to rename file %s", name1.c_str());
                            return false;
                        }
                    }
                }

                {   // write scene file
                    const std::string
                        name1 = "assets/scenes/" + name_str + ".json",
                        name2 = "assets/scenes/" + name_str + ".json1";

                    int ret = std::rename(name1.c_str(), name2.c_str());
                    if (ret) {
                        shrd_this->ctx_->log()->Error("Failed to rename file %s", name1.c_str());
                        return false;
                    }

                    std::ofstream out_file(name1, std::ios::binary);
                    out_scene.Write(out_file);
                }
            }

            // scene file was written, copy it to assets_pc folder
            Viewer::PrepareAssets("pc");
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

    // Initialize first draw list
    UpdateFrame(0);
}

bool GSBaseState::LoadScene(const char *name) {
    using namespace GSBaseStateInternal;

    // wait for backgroud thread iteration
    if (USE_TWO_THREADS) {
        std::unique_lock<std::mutex> lock(mtx_);
        while (notified_) {
            thr_done_.wait(lock);
        }
    }

    JsObject js_scene, js_probe_cache;

    {   // Load scene data from file
        Sys::AssetFile in_scene(name);
        if (!in_scene) {
            log_->Error("Can not open scene file %s", name);
            return false;
        }

        size_t scene_size = in_scene.size();

        std::unique_ptr<uint8_t[]> scene_data(new uint8_t[scene_size]);
        in_scene.Read((char *)&scene_data[0], scene_size);

        Sys::MemBuf mem(&scene_data[0], scene_size);
        std::istream in_stream(&mem);

        if (!js_scene.Read(in_stream)) {
            throw std::runtime_error("Cannot load scene!");
        }
    }

    {   // Load probe cache data from file
        std::string cache_file =
#if defined(__ANDROID__)
            "assets/textures/probes_cache/";
#else
            "assets_pc/textures/probes_cache/";
#endif
        const char *_name = strrchr(name, '/');
        if (_name) {
            ++_name;
            cache_file += _name;
        }

        Sys::AssetFile in_cache(cache_file.c_str());

        if (in_cache) {
            size_t cache_size = in_cache.size();

            std::unique_ptr<uint8_t[]> cache_data(new uint8_t[cache_size]);
            in_cache.Read((char *)&cache_data[0], cache_size);

            Sys::MemBuf mem(&cache_data[0], cache_size);
            std::istream in_stream(&mem);

            if (!js_probe_cache.Read(in_stream)) {
                js_probe_cache.elements.clear();
            }
        }
    }

    OnPreloadScene(js_scene);

    try {
        scene_manager_->LoadScene(js_scene);
    } catch (std::exception &e) {
        log_->Info("Error loading scene: %s", e.what());
    }

    OnPostloadScene(js_scene);

    return true;
}

void GSBaseState::OnPreloadScene(JsObject &js_scene) {
    
}

void GSBaseState::OnPostloadScene(JsObject &js_scene) {
    // trigger probes update
    probes_dirty_ = false;
}

void GSBaseState::SaveScene(JsObject &js_scene) {
    scene_manager_->SaveScene(js_scene);
}

void GSBaseState::Exit() {
    using namespace GSBaseStateInternal;

    if (USE_TWO_THREADS) {
        if (background_thread_.joinable()) {
            shutdown_ = notified_ = true;
            thr_notify_.notify_all();
            background_thread_.join();
        }
    }
}

void GSBaseState::Draw(uint64_t dt_us) {
    using namespace GSBaseStateInternal;

    if (cmdline_enabled_) {
        // Process comandline input
        for (const InputManager::Event &evt : cmdline_input_) {
            if (evt.key_code == KeyDelete) {
                if (!cmdline_history_.back().empty()) {
                    cmdline_history_.back().pop_back();
                }
            } else if (evt.key_code == KeyReturn) {
                cmdline_->Execute(cmdline_history_.back().c_str());

                cmdline_history_.emplace_back();
                if (cmdline_history_.size() > MAX_CMD_LINES) {
                    cmdline_history_.erase(cmdline_history_.begin());
                }
            } else if (evt.key_code == KeyGrave) {
                if (!cmdline_history_.back().empty()) {
                    cmdline_history_.emplace_back();
                    if (cmdline_history_.size() > MAX_CMD_LINES) {
                        cmdline_history_.erase(cmdline_history_.begin());
                    }
                }
            } else {
                char ch = InputManager::CharFromKeycode(evt.key_code);
                if (shift_down_) {
                    if (ch == '-') ch = '_';
                    else ch = std::toupper(ch);
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
                        renderer_->BlitPixels(preview_pixels, w, h, Ren::eTexFormat::RawRGBA32F);
                    }
                } else {
                    // Lightmap creation finished, convert textures
                    Viewer::PrepareAssets("pc");
                    // Reload scene
                    //LoadScene(SCENE_NAME);
                    // Switch back to normal mode
                    use_lm_ = false;
                }

                back_list = -1;
            } else if (use_pt_) {
                // TODO: fix pt view setup (use current camera)
                //scene_manager_->SetupView_PT(view_origin_, (view_origin_ + view_dir_), Ren::Vec3f{ 0.0f, 1.0f, 0.0f }, view_fov_);
                if (invalidate_view_) {
                    scene_manager_->Clear_PT();
                    invalidate_view_ = false;
                }
                int w, h;
                const float *preview_pixels = scene_manager_->Draw_PT(&w, &h);
                if (preview_pixels) {
                    renderer_->BlitPixelsTonemap(preview_pixels, w, h, Ren::eTexFormat::RawRGBA32F);
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
                    bool done = renderer_->BlitProjectSH(
                        scene_data.probe_storage, probe_to_update_sh_->layer_index,
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
            //scene_manager_->SetupView(view_origin_, (view_origin_ + view_dir_), Ren::Vec3f{ 0.0f, 1.0f, 0.0f }, view_fov_);
            // Gather drawables for list 0
            UpdateFrame(0);
            back_list = 0;
        }

        if (back_list != -1) {
            // Render current frame (from back list)
            renderer_->ExecuteDrawList(main_view_lists_[back_list]);
        }
    }

    {
        ui_renderer_->BeginDraw();

        DrawUI(ui_renderer_.get(), ui_root_.get());

        ui_renderer_->EndDraw();
    }

    ctx_->ProcessTasks();
}

void GSBaseState::DrawUI(Gui::Renderer *r, Gui::BaseElement *root) {
    using namespace GSBaseStateInternal;

    const float font_height = font_->height(root);
    const uint8_t text_color[4] = { 255, 255, 255, 255 };

    if (cmdline_enabled_) {
        int ifont_height = (int)(0.5f * font_->height(root) * (float)game_->height);
#if defined(USE_GL_RENDER)
        glEnable(GL_SCISSOR_TEST);
        glScissor(0, game_->height - MAX_CMD_LINES * ifont_height - 2, game_->width, MAX_CMD_LINES * ifont_height + 2);

        glClearColor(0, 0.5f, 0.5f, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        glDisable(GL_SCISSOR_TEST);
#endif
        float cur_y = 1.0f - font_->height(root);

        for (const std::string &cmd : cmdline_history_) {
            font_->DrawText(r, cmd.c_str(), Ren::Vec2f{ -1, cur_y }, text_color, root);
            cur_y -= font_height;
        }
    }

    if (!use_pt_ && !use_lm_) {
        int back_list = (front_list_ + 1) % 2;

        uint32_t render_flags = renderer_->render_flags();
        FrontendInfo front_info = main_view_lists_[back_list].frontend_info;
        BackendInfo back_info = renderer_->backend_info();

        /*const uint64_t
            front_dur = front_info.end_timepoint_us - front_info.start_timepoint_us,
            back_dur = back_info.cpu_end_timepoint_us - back_info.cpu_start_timepoint_us;

        LOGI("Frontend: %04lld\tBackend(cpu): %04lld", (long long)front_dur, (long long)back_dur);*/

        ItemsInfo items_info;
        items_info.light_sources_count = main_view_lists_[back_list].light_sources.count;
        items_info.decals_count = main_view_lists_[back_list].decals.count;
        items_info.probes_count = main_view_lists_[back_list].probes.count;
        items_info.items_total = main_view_lists_[back_list].items.count;

        debug_ui_->UpdateInfo(front_info, back_info, items_info, *swap_interval_, render_flags);
        debug_ui_->Draw(r);
    }
}

void GSBaseState::Update(uint64_t dt_us) {
    
}

bool GSBaseState::HandleInput(const InputManager::Event &evt) {
    using namespace Ren;
    using namespace GSBaseStateInternal;

    switch (evt.type) {
    case RawInputEvent::EvP1Down: {
    } break;
    case RawInputEvent::EvP2Down: {
    } break;
    case RawInputEvent::EvP1Up: {
    } break;
    case RawInputEvent::EvP2Up: {
    } break;
    case RawInputEvent::EvP1Move: {
    } break;
    case RawInputEvent::EvP2Move: {
    } break;
    case RawInputEvent::EvKeyDown: {
        if (evt.key_code == KeyLeftShift || evt.key_code == KeyRightShift) {
            shift_down_ = true;
        }
    } break;
    case RawInputEvent::EvKeyUp: {
        if (evt.key_code == KeyLeftShift || evt.key_code == KeyRightShift) {
            shift_down_ = false;
        } else if (evt.key_code == KeyDelete) {
            if (cmdline_enabled_) {
                cmdline_input_.push_back(evt);
            }
        } else if (evt.key_code == KeyReturn) {
            if (cmdline_enabled_) {
                cmdline_input_.push_back(evt);
            }
        } else if (evt.key_code == KeyGrave) {
            cmdline_enabled_ = !cmdline_enabled_;
            if (cmdline_enabled_) {
                cmdline_input_.push_back(evt);
            }
        } else if (cmdline_enabled_) {
            cmdline_input_.push_back(evt);
        }
    }
    case RawInputEvent::EvResize:
        break;
    default:
        break;
    }

    return true;
}

void GSBaseState::BackgroundProc() {
#if !defined(RELEASE_FINAL) && !defined(__ANDROID__)
    __itt_thread_set_name("Renderer Frontend Thread");
#endif

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

void GSBaseState::UpdateFrame(int list_index) {
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

    OnUpdateScene();

    // Update invalidated objects
    scene_manager_->UpdateObjects();

    if (!use_pt_ && !use_lm_) {
        if (update_all_probes_) {
            if (probes_to_update_.empty()) {
                const int obj_count = (int)scene_manager_->scene_data().objects.size();
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
            log_->Info("Updating probe");
            SceneObject *probe_obj = scene_manager_->GetObject(probes_to_update_.back());
            auto *probe = (LightProbe *)scene_manager_->scene_data().comp_store[CompProbe]->Get(probe_obj->components[CompProbe]);
            auto *probe_tr = (Transform *)scene_manager_->scene_data().comp_store[CompTransform]->Get(probe_obj->components[CompTransform]);

            auto pos = Ren::Vec4f{ probe->offset[0], probe->offset[1], probe->offset[2], 1.0f };
            pos = probe_tr->mat * pos;
            pos /= pos[3];

            static const Ren::Vec3f axises[] = {
                Ren::Vec3f{ 1.0f,  0.0f,  0.0f },
                Ren::Vec3f{ -1.0f,  0.0f,  0.0f },
                Ren::Vec3f{ 0.0f,  1.0f,  0.0f },
                Ren::Vec3f{ 0.0f, -1.0f,  0.0f },
                Ren::Vec3f{ 0.0f,  0.0f,  1.0f },
                Ren::Vec3f{ 0.0f,  0.0f, -1.0f }
            };

            static const Ren::Vec3f ups[] = {
                Ren::Vec3f{ 0.0f, -1.0f, 0.0f },
                Ren::Vec3f{ 0.0f, -1.0f, 0.0f },
                Ren::Vec3f{ 0.0f,  0.0f, 1.0f },
                Ren::Vec3f{ 0.0f,  0.0f, -1.0f },
                Ren::Vec3f{ 0.0f, -1.0f, 0.0f },
                Ren::Vec3f{ 0.0f, -1.0f, 0.0f }
            };

            const auto center = Ren::Vec3f{ pos[0], pos[1], pos[2] };

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
        
        main_view_lists_[list_index].render_flags = render_flags_;

        renderer_->PrepareDrawList(scene_manager_->scene_data(), scene_manager_->main_cam(), main_view_lists_[list_index]);
    }
}
