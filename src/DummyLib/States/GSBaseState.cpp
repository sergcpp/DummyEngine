#include "GSBaseState.h"

#include <cctype>

#include <fstream>
#include <memory>

#if !defined(RELEASE_FINAL) && !defined(__ANDROID__)
#include <vtune/ittnotify.h>
#endif

#include <Eng/GameStateManager.h>
#include <Eng/Gui/Image9Patch.h>
#include <Eng/Gui/Renderer.h>
#include <Eng/Random.h>
#include <Eng/Renderer/Renderer.h>
#include <Eng/Scene/PhysicsManager.h>
#include <Eng/Scene/SceneManager.h>
#include <Eng/Utils/Cmdline.h>
#include <Ren/Context.h>
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
} // namespace GSBaseStateInternal

GSBaseState::GSBaseState(GameBase *game) : game_(game) {
    using namespace GSBaseStateInternal;

    cmdline_ = game->GetComponent<Cmdline>(CMDLINE_KEY);

    state_manager_ = game->GetComponent<GameStateManager>(STATE_MANAGER_KEY);
    ren_ctx_ = game->GetComponent<Ren::Context>(REN_CONTEXT_KEY);
    snd_ctx_ = game->GetComponent<Snd::Context>(SND_CONTEXT_KEY);
    log_ = game->GetComponent<Ren::ILog>(LOG_KEY);

    renderer_ = game->GetComponent<Renderer>(RENDERER_KEY);
    scene_manager_ = game->GetComponent<SceneManager>(SCENE_MANAGER_KEY);
    physics_manager_ = game->GetComponent<PhysicsManager>(PHYSICS_MANAGER_KEY);
    shader_loader_ = game->GetComponent<ShaderLoader>(SHADER_LOADER_KEY);

    ui_renderer_ = game->GetComponent<Gui::Renderer>(UI_RENDERER_KEY);
    ui_root_ = game->GetComponent<Gui::BaseElement>(UI_ROOT_KEY);

    const std::shared_ptr<FontStorage> fonts = game->GetComponent<FontStorage>(UI_FONTS_KEY);
    font_ = fonts->FindFont("main_font");

    debug_ui_ = game->GetComponent<DebugInfoUI>(UI_DEBUG_KEY);

    cmdline_back_.reset(new Gui::Image9Patch(
        *ren_ctx_, (std::string(ASSETS_BASE_PATH) + "/textures/editor/dial_edit_back.uncompressed.tga").c_str(),
        Ren::Vec2f{1.5f, 1.5f}, 1.0f, Ren::Vec2f{-1.0f, -1.0f}, Ren::Vec2f{2.0f, 2.0f}, ui_root_.get()));

    swap_interval_ = game->GetComponent<TimeInterval>(SWAP_TIMER_KEY);

    random_ = game->GetComponent<Random>(RANDOM_KEY);

    // Prepare cam for probes updating
    temp_probe_cam_.Perspective(90.0f, 1.0f, 0.1f, 10000.0f);
    temp_probe_cam_.set_render_mask(uint32_t(Drawable::eDrVisibility::VisProbes));

    //
    // Create required staging buffers
    //
    Ren::BufferRef instances_stage_buf = ren_ctx_->LoadBuffer("Instances (Stage)", Ren::eBufType::Stage,
                                                              InstanceDataBufChunkSize * Ren::MaxFramesInFlight);
    Ren::BufferRef instance_indices_stage_buf = ren_ctx_->LoadBuffer(
        "Instance Indices (Stage)", Ren::eBufType::Stage, InstanceIndicesBufChunkSize * Ren::MaxFramesInFlight);
    Ren::BufferRef skin_transforms_stage_buf = ren_ctx_->LoadBuffer(
        "Skin Transforms (Stage)", Ren::eBufType::Stage, SkinTransformsBufChunkSize * Ren::MaxFramesInFlight);
    Ren::BufferRef shape_keys_stage_buf = ren_ctx_->LoadBuffer("Shape Keys (Stage)", Ren::eBufType::Stage,
                                                               ShapeKeysBufChunkSize * Ren::MaxFramesInFlight);
    Ren::BufferRef cells_stage_buf =
        ren_ctx_->LoadBuffer("Cells (Stage)", Ren::eBufType::Stage, CellsBufChunkSize * Ren::MaxFramesInFlight);
    Ren::BufferRef items_stage_buf =
        ren_ctx_->LoadBuffer("Items (Stage)", Ren::eBufType::Stage, ItemsBufChunkSize * Ren::MaxFramesInFlight);
    Ren::BufferRef lights_stage_buf =
        ren_ctx_->LoadBuffer("Lights (Stage)", Ren::eBufType::Stage, LightsBufChunkSize * Ren::MaxFramesInFlight);
    Ren::BufferRef decals_stage_buf =
        ren_ctx_->LoadBuffer("Decals (Stage)", Ren::eBufType::Stage, DecalsBufChunkSize * Ren::MaxFramesInFlight);
    Ren::BufferRef rt_obj_instances_stage_buf = ren_ctx_->LoadBuffer(
        "RT Obj Instances (Stage)", Ren::eBufType::Stage, RTObjInstancesBufChunkSize * Ren::MaxFramesInFlight);

    Ren::BufferRef shared_data_stage_buf =
        ren_ctx_->LoadBuffer("Shared Data (Stage)", Ren::eBufType::Stage, SharedDataBlockSize * Ren::MaxFramesInFlight);

    //
    // Initialize draw lists
    //
    for (int i = 0; i < 2; i++) {
        main_view_lists_[i].Init(shared_data_stage_buf, instances_stage_buf, instance_indices_stage_buf,
                                 skin_transforms_stage_buf, shape_keys_stage_buf, cells_stage_buf, items_stage_buf,
                                 lights_stage_buf, decals_stage_buf, rt_obj_instances_stage_buf);
    }
}

GSBaseState::~GSBaseState() = default;

void GSBaseState::Enter() {
    using namespace GSBaseStateInternal;

    if (USE_TWO_THREADS) {
        background_thread_ = std::thread(std::bind(&GSBaseState::BackgroundProc, this));
    }

    { // Create temporary buffer to update probes
        FrameBuf::ColorAttachmentDesc desc;
        desc.format = Ren::eTexFormat::RawRGB16F;
        desc.filter = Ren::eTexFilter::NoFilter;
        desc.wrap = Ren::eTexWrap::ClampToEdge;

        const int res = scene_manager_->scene_data().probe_storage.res();
        temp_probe_buf_ = FrameBuf("Temp probe", *ren_ctx_, res, res, &desc, 1, {}, 1, ren_ctx_->log());
    }

    cmdline_history_.emplace_back();

    std::shared_ptr<GameStateManager> state_manager = state_manager_.lock();
    std::weak_ptr<GSBaseState> weak_this = std::dynamic_pointer_cast<GSBaseState>(state_manager->Peek());

    cmdline_->RegisterCommand("r_wireframe", [weak_this](const int argc, Cmdline::ArgData *argv) -> bool {
        auto shrd_this = weak_this.lock();
        if (shrd_this) {
            uint64_t flags = shrd_this->renderer_->render_flags();
            flags ^= DebugWireframe;
            shrd_this->renderer_->set_render_flags(flags);
        }
        return true;
    });

    cmdline_->RegisterCommand("r_culling", [weak_this](const int argc, Cmdline::ArgData *argv) -> bool {
        auto shrd_this = weak_this.lock();
        if (shrd_this) {
            uint64_t flags = shrd_this->renderer_->render_flags();
            flags ^= EnableCulling;
            shrd_this->renderer_->set_render_flags(flags);
        }
        return true;
    });

    cmdline_->RegisterCommand("r_lightmap", [weak_this](const int argc, Cmdline::ArgData *argv) -> bool {
        auto shrd_this = weak_this.lock();
        if (shrd_this) {
            uint64_t flags = shrd_this->renderer_->render_flags();
            flags ^= EnableLightmap;
            shrd_this->renderer_->set_render_flags(flags);
        }
        return true;
    });

    cmdline_->RegisterCommand("r_lights", [weak_this](const int argc, Cmdline::ArgData *argv) -> bool {
        auto shrd_this = weak_this.lock();
        if (shrd_this) {
            uint64_t flags = shrd_this->renderer_->render_flags();
            flags ^= EnableLights;
            shrd_this->renderer_->set_render_flags(flags);
        }
        return true;
    });

    cmdline_->RegisterCommand("r_decals", [weak_this](const int argc, Cmdline::ArgData *argv) -> bool {
        auto shrd_this = weak_this.lock();
        if (shrd_this) {
            uint64_t flags = shrd_this->renderer_->render_flags();
            flags ^= EnableDecals;
            shrd_this->renderer_->set_render_flags(flags);
        }
        return true;
    });

    cmdline_->RegisterCommand("r_shadows", [weak_this](const int argc, Cmdline::ArgData *argv) -> bool {
        auto shrd_this = weak_this.lock();
        if (shrd_this) {
            uint64_t flags = shrd_this->renderer_->render_flags();
            flags ^= EnableShadows;
            shrd_this->renderer_->set_render_flags(flags);
        }
        return true;
    });

    cmdline_->RegisterCommand("r_msaa", [weak_this](const int argc, Cmdline::ArgData *argv) -> bool {
        auto shrd_this = weak_this.lock();
        if (shrd_this) {
            uint64_t flags = shrd_this->renderer_->render_flags();
            flags ^= EnableMsaa;
            shrd_this->renderer_->set_render_flags(flags);
        }
        return true;
    });

    cmdline_->RegisterCommand("r_fxaa", [weak_this](const int argc, Cmdline::ArgData *argv) -> bool {
        auto shrd_this = weak_this.lock();
        if (shrd_this) {
            uint64_t flags = shrd_this->renderer_->render_flags();
            flags ^= EnableFxaa;
            shrd_this->renderer_->set_render_flags(flags);
        }
        return true;
    });

    cmdline_->RegisterCommand("r_taa", [weak_this](const int argc, Cmdline::ArgData *argv) -> bool {
        auto shrd_this = weak_this.lock();
        if (shrd_this) {
            uint64_t flags = shrd_this->renderer_->render_flags();
            flags ^= EnableTaa;
            if (flags & EnableTaa) {
                flags &= ~(EnableMsaa | EnableFxaa);
            }
            shrd_this->renderer_->set_render_flags(flags);
        }
        return true;
    });

    cmdline_->RegisterCommand("r_pt", [weak_this](const int argc, Cmdline::ArgData *argv) -> bool {
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

    cmdline_->RegisterCommand("r_lm", [weak_this](const int argc, Cmdline::ArgData *argv) -> bool {
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

    cmdline_->RegisterCommand("r_oit", [weak_this](const int argc, Cmdline::ArgData *argv) -> bool {
        auto shrd_this = weak_this.lock();
        if (shrd_this) {
            uint64_t flags = shrd_this->renderer_->render_flags();
            flags ^= EnableOIT;
            shrd_this->renderer_->set_render_flags(flags);
        }
        return true;
    });

    cmdline_->RegisterCommand("r_zfill", [weak_this](const int argc, Cmdline::ArgData *argv) -> bool {
        auto shrd_this = weak_this.lock();
        if (shrd_this) {
            uint64_t flags = shrd_this->renderer_->render_flags();
            flags ^= EnableZFill;
            shrd_this->renderer_->set_render_flags(flags);
        }
        return true;
    });

    cmdline_->RegisterCommand("r_deferred", [weak_this](const int argc, Cmdline::ArgData *argv) -> bool {
        auto shrd_this = weak_this.lock();
        if (shrd_this) {
            uint64_t flags = shrd_this->renderer_->render_flags();
            flags ^= EnableDeferred;
            shrd_this->renderer_->set_render_flags(flags);
        }
        return true;
    });

    cmdline_->RegisterCommand("r_updateProbes", [weak_this](const int argc, Cmdline::ArgData *argv) -> bool {
        auto shrd_this = weak_this.lock();
        if (shrd_this) {
            SceneData &scene_data = shrd_this->scene_manager_->scene_data();

            const int res = scene_data.probe_storage.res(), capacity = scene_data.probe_storage.capacity();
            const bool result = scene_data.probe_storage.Resize(
                shrd_this->ren_ctx_->api_ctx(), shrd_this->ren_ctx_->default_mem_allocs(), Ren::eTexFormat::RawRGBA8888,
                res, capacity, shrd_this->ren_ctx_->log());
            assert(result);

            shrd_this->update_all_probes_ = true;
        }
        return true;
    });

    cmdline_->RegisterCommand("r_cacheProbes", [weak_this](const int argc, Cmdline::ArgData *argv) -> bool {
        auto shrd_this = weak_this.lock();
        if (shrd_this) {
            const SceneData &scene_data = shrd_this->scene_manager_->scene_data();

            const CompStorage *lprobes = scene_data.comp_store[CompProbe];
            SceneManager::WriteProbeCache("assets/textures/probes_cache", scene_data.name.c_str(),
                                          scene_data.probe_storage, lprobes, shrd_this->ren_ctx_->log());

            // probe textures were written, convert them
            Viewer::PrepareAssets("pc");
        }
        return true;
    });

    cmdline_->RegisterCommand("map", [weak_this](const int argc, Cmdline::ArgData *argv) -> bool {
        if (argc != 2 || argv[1].type != Cmdline::eArgType::ArgString) {
            return false;
        }

        auto shrd_this = weak_this.lock();
        if (shrd_this) {
            char buf[1024];
            sprintf(buf, "%s/scenes/%.*s", ASSETS_BASE_PATH, (int)argv[1].str.len, argv[1].str.str);
            shrd_this->LoadScene(buf);
        }
        return true;
    });

    cmdline_->RegisterCommand("save", [weak_this](const int argc, Cmdline::ArgData *argv) -> bool {
        auto shrd_this = weak_this.lock();
        if (shrd_this) {
            JsObjectP out_scene(shrd_this->scene_manager_->mp_alloc());

            shrd_this->SaveScene(out_scene);

            { // Write output file
                std::string name_str;

                { // get scene file name
                    const SceneData &scene_data = shrd_this->scene_manager_->scene_data();
                    name_str = scene_data.name.c_str();
                }

                { // rotate backup files
                    for (int i = 7; i > 0; i--) {
                        const std::string name1 = "assets/scenes/" + name_str + ".json" + std::to_string(i),
                                          name2 = "assets/scenes/" + name_str + ".json" + std::to_string(i + 1);
                        if (!std::ifstream(name1).good()) {
                            continue;
                        }

                        if (i == 7 && std::ifstream(name2).good()) {
                            const int ret = std::remove(name2.c_str());
                            if (ret) {
                                shrd_this->ren_ctx_->log()->Error("Failed to remove file %s", name2.c_str());
                                return false;
                            }
                        }

                        const int ret = std::rename(name1.c_str(), name2.c_str());
                        if (ret) {
                            shrd_this->ren_ctx_->log()->Error("Failed to rename file %s", name1.c_str());
                            return false;
                        }
                    }
                }

                { // write scene file
                    const std::string name1 = "assets/scenes/" + name_str + ".json",
                                      name2 = "assets/scenes/" + name_str + ".json1";

                    const int ret = std::rename(name1.c_str(), name2.c_str());
                    if (ret) {
                        shrd_this->ren_ctx_->log()->Error("Failed to rename file %s", name1.c_str());
                        return false;
                    }

                    std::ofstream out_file(name1, std::ios::binary);
                    out_file.precision(std::numeric_limits<double>::max_digits10);
                    out_scene.Write(out_file);
                }
            }

            // scene file was written, copy it to assets_pc folder
            Viewer::PrepareAssets("pc");
        }
        return true;
    });

    cmdline_->RegisterCommand("r_reloadTextures", [weak_this](const int argc, Cmdline::ArgData *argv) -> bool {
        auto shrd_this = weak_this.lock();
        if (shrd_this) {
            shrd_this->scene_manager_->ForceTextureReload();
        }
        return true;
    });

    cmdline_->RegisterCommand("r_showCull", [weak_this](const int argc, Cmdline::ArgData *argv) -> bool {
        auto shrd_this = weak_this.lock();
        if (shrd_this) {
            uint64_t flags = shrd_this->renderer_->render_flags();
            flags ^= DebugCulling;
            shrd_this->renderer_->set_render_flags(flags);
        }
        return true;
    });

    cmdline_->RegisterCommand("r_showShadows", [weak_this](const int argc, Cmdline::ArgData *argv) -> bool {
        auto shrd_this = weak_this.lock();
        if (shrd_this) {
            uint64_t flags = shrd_this->renderer_->render_flags();
            flags ^= DebugShadow;
            shrd_this->renderer_->set_render_flags(flags);
        }
        return true;
    });

    cmdline_->RegisterCommand("r_showLights", [weak_this](const int argc, Cmdline::ArgData *argv) -> bool {
        auto shrd_this = weak_this.lock();
        if (shrd_this) {
            uint64_t flags = shrd_this->renderer_->render_flags();
            flags ^= DebugLights;
            shrd_this->renderer_->set_render_flags(flags);
        }
        return true;
    });

    cmdline_->RegisterCommand("r_showDecals", [weak_this](const int argc, Cmdline::ArgData *argv) -> bool {
        auto shrd_this = weak_this.lock();
        if (shrd_this) {
            uint64_t flags = shrd_this->renderer_->render_flags();
            flags ^= DebugDecals;
            shrd_this->renderer_->set_render_flags(flags);
        }
        return true;
    });

    cmdline_->RegisterCommand("r_showDeferred", [weak_this](const int argc, Cmdline::ArgData *argv) -> bool {
        auto shrd_this = weak_this.lock();
        if (shrd_this) {
            uint64_t flags = shrd_this->renderer_->render_flags();
            flags ^= DebugDeferred;
            shrd_this->renderer_->set_render_flags(flags);
        }
        return true;
    });

    cmdline_->RegisterCommand("r_showBlur", [weak_this](const int argc, Cmdline::ArgData *argv) -> bool {
        auto shrd_this = weak_this.lock();
        if (shrd_this) {
            uint64_t flags = shrd_this->renderer_->render_flags();
            flags ^= DebugBlur;
            shrd_this->renderer_->set_render_flags(flags);
        }
        return true;
    });

    cmdline_->RegisterCommand("r_showSSAO", [weak_this](const int argc, Cmdline::ArgData *argv) -> bool {
        auto shrd_this = weak_this.lock();
        if (shrd_this) {
            uint64_t flags = shrd_this->renderer_->render_flags();
            flags ^= DebugSSAO;
            shrd_this->renderer_->set_render_flags(flags);
        }
        return true;
    });

    cmdline_->RegisterCommand("r_showTimings", [weak_this](const int argc, Cmdline::ArgData *argv) -> bool {
        auto shrd_this = weak_this.lock();
        if (shrd_this) {
            uint64_t flags = shrd_this->renderer_->render_flags();
            flags ^= DebugTimings;
            shrd_this->renderer_->set_render_flags(flags);
        }
        return true;
    });

    cmdline_->RegisterCommand("r_showBVH", [weak_this](const int argc, Cmdline::ArgData *argv) -> bool {
        auto shrd_this = weak_this.lock();
        if (shrd_this) {
            uint64_t flags = shrd_this->renderer_->render_flags();
            flags ^= DebugBVH;
            shrd_this->renderer_->set_render_flags(flags);
        }
        return true;
    });

    cmdline_->RegisterCommand("r_showProbes", [weak_this](const int argc, Cmdline::ArgData *argv) -> bool {
        auto shrd_this = weak_this.lock();
        if (shrd_this) {
            uint64_t flags = shrd_this->renderer_->render_flags();
            flags ^= DebugProbes;
            shrd_this->renderer_->set_render_flags(flags);
        }
        return true;
    });

    cmdline_->RegisterCommand("r_showEllipsoids", [weak_this](const int argc, Cmdline::ArgData *argv) -> bool {
        auto shrd_this = weak_this.lock();
        if (shrd_this) {
            uint64_t flags = shrd_this->renderer_->render_flags();
            flags ^= DebugEllipsoids;
            shrd_this->renderer_->set_render_flags(flags);
        }
        return true;
    });

    cmdline_->RegisterCommand("r_showRT", [weak_this](const int argc, Cmdline::ArgData *argv) -> bool {
        auto shrd_this = weak_this.lock();
        if (shrd_this) {
            uint64_t flags = shrd_this->renderer_->render_flags();
            flags ^= DebugRT;
            shrd_this->renderer_->set_render_flags(flags);
        }
        return true;
    });

    cmdline_->RegisterCommand("r_showDenoise", [weak_this](const int argc, Cmdline::ArgData *argv) -> bool {
        auto shrd_this = weak_this.lock();
        if (shrd_this) {
            uint64_t flags = shrd_this->renderer_->render_flags();
            flags ^= DebugDenoise;
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

    // clear outdated draw data
    main_view_lists_[0].Clear();
    main_view_lists_[1].Clear();

    JsObjectP js_scene(scene_manager_->mp_alloc()), js_probe_cache(scene_manager_->mp_alloc());

    { // Load scene data from file
        Sys::AssetFile in_scene(name);
        if (!in_scene) {
            log_->Error("Can not open scene file %s", name);
            return false;
        }

        const size_t scene_size = in_scene.size();

        std::unique_ptr<uint8_t[]> scene_data(new uint8_t[scene_size]);
        in_scene.Read((char *)&scene_data[0], scene_size);

        Sys::MemBuf mem(&scene_data[0], scene_size);
        std::istream in_stream(&mem);

        if (!js_scene.Read(in_stream)) {
            throw std::runtime_error("Cannot load scene!");
        }
    }

    { // Load probe cache data from file
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
            const size_t cache_size = in_cache.size();

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

void GSBaseState::OnPreloadScene(JsObjectP &js_scene) {}

void GSBaseState::OnPostloadScene(JsObjectP &js_scene) {
    // trigger probes update
    probes_dirty_ = false;
}

void GSBaseState::SaveScene(JsObjectP &js_scene) { scene_manager_->SaveScene(js_scene); }

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

void GSBaseState::UpdateAnim(const uint64_t dt_us) {
    cmdline_cursor_blink_us_ += dt_us;
    if (cmdline_cursor_blink_us_ > 1000000 || !cmdline_input_.empty()) {
        cmdline_cursor_blink_us_ = 0;
    }
}

void GSBaseState::Draw() {
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
                cmdline_history_index_ = -1;
                if (cmdline_history_.size() > MAX_CMD_LINES) {
                    cmdline_history_.erase(cmdline_history_.begin());
                }
            } else if (evt.key_code == KeyGrave) {
                if (!cmdline_history_.back().empty()) {
                    cmdline_history_.emplace_back();
                    cmdline_history_index_ = -1;
                    if (cmdline_history_.size() > MAX_CMD_LINES) {
                        cmdline_history_.erase(cmdline_history_.begin());
                    }
                }
            } else if (evt.key_code == KeyUp) {
                cmdline_history_index_ = std::min(++cmdline_history_index_, int(cmdline_history_.size()) - 2);
                cmdline_history_.back() = cmdline_history_[cmdline_history_.size() - 2 - cmdline_history_index_];
            } else if (evt.key_code == KeyDown) {
                cmdline_history_index_ = std::max(--cmdline_history_index_, 0);
                cmdline_history_.back() = cmdline_history_[cmdline_history_.size() - 2 - cmdline_history_index_];
            } else {
                char ch = InputManager::CharFromKeycode(evt.key_code);
                if (shift_down_) {
                    if (ch == '-') {
                        ch = '_';
                    } else {
                        ch = std::toupper(ch);
                    }
                }
                cmdline_history_.back() += ch;
            }
        }

        cmdline_input_.clear();
    }

    scene_manager_->Serve();

    {
        int back_list;

        if (USE_TWO_THREADS) {
            std::unique_lock<std::mutex> lock(mtx_);
            while (notified_) {
                thr_done_.wait(lock);
            }

            renderer_->InitBackendInfo();

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
                    // LoadScene(SCENE_NAME);
                    // Switch back to normal mode
                    use_lm_ = false;
                }

                back_list = -1;
            } else if (use_pt_) {
                const Ren::Camera &cam = scene_manager_->main_cam();
                scene_manager_->SetupView_PT(cam.world_position(), (cam.world_position() - cam.view_dir()),
                                             Ren::Vec3f{0.0f, 1.0f, 0.0f}, cam.angle());
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
                    const bool done =
                        renderer_->BlitProjectSH(scene_data.probe_storage, probe_to_update_sh_->layer_index,
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
                        renderer_->ExecuteDrawList(temp_probe_lists_[i], scene_manager_->persistent_data(),
                                                   &temp_probe_buf_);
                        renderer_->BlitToTempProbeFace(temp_probe_buf_, scene_data.probe_storage, i);
                    }

                    renderer_->BlitPrefilterFromTemp(scene_data.probe_storage, probe_to_render_->layer_index);

                    probe_to_update_sh_ = probe_to_render_;
                    probe_to_render_ = nullptr;
                }
            }

            // Target frontend to next frame
            ren_ctx_->frontend_frame = (ren_ctx_->backend_frame() + 1) % Ren::MaxFramesInFlight;

            notified_ = true;
            thr_notify_.notify_one();
        } else {
            renderer_->InitBackendInfo();
            // scene_manager_->SetupView(view_origin_, (view_origin_ + view_dir_),
            // Ren::Vec3f{ 0.0f, 1.0f, 0.0f }, view_fov_);
            // Gather drawables for list 0
            UpdateFrame(0);
            // Target frontend to current frame
            ren_ctx_->frontend_frame = ren_ctx_->backend_frame();
            back_list = 0;
        }

        if (back_list != -1) {
            // Render current frame (from back list)
            renderer_->ExecuteDrawList(main_view_lists_[back_list], scene_manager_->persistent_data());
        }
    }

    ui_renderer_->Draw(ren_ctx_->w(), ren_ctx_->h());

    ren_ctx_->ProcessTasks();
}

void GSBaseState::DrawUI(Gui::Renderer *r, Gui::BaseElement *root) {
    using namespace GSBaseStateInternal;

    const float font_height = font_->height(root);
    const uint8_t text_color[4] = {255, 255, 255, 255};

    if (cmdline_enabled_) {
        float cur_y = 1.0f - font_height * float(MAX_CMD_LINES - cmdline_history_.size() + 1);

        const float total_height = (float(MAX_CMD_LINES) + 0.4f) * font_height;

        cmdline_back_->Resize(Ren::Vec2f{-1.0f, 1.0f - total_height}, Ren::Vec2f{2.0f, total_height}, root);
        cmdline_back_->Draw(r);

        for (size_t i = 0; i < cmdline_history_.size(); i++) {
            const std::string &cmd = cmdline_history_[i];

            const float width = font_->DrawText(r, cmd.c_str(), Ren::Vec2f{-1, cur_y}, text_color, root);
            if (i == cmdline_history_.size() - 1 && cmdline_cursor_blink_us_ < 500000) {
                // draw cursor
                font_->DrawText(r, "_", Ren::Vec2f{-1.0f + width, cur_y}, text_color, root);
            }
            cur_y -= font_height;
        }

        if (!cmdline_history_.empty() && !cmdline_history_.back().empty()) {
            const char *cmd = cmdline_history_.back().c_str();
            Ren::String hint_str;
            int index = cmdline_->NextHint(cmd, -1, hint_str);
            while (index != -1) {
                font_->DrawText(r, hint_str.c_str(), Ren::Vec2f{-1.0f, cur_y}, text_color, root);
                cur_y -= font_height;
                index = cmdline_->NextHint(cmd, index, hint_str);
            }
        }
    }

    if (!use_pt_ && !use_lm_) {
        const int back_list = (front_list_ + 1) % 2;

        uint64_t render_flags = renderer_->render_flags();
        FrontendInfo front_info = main_view_lists_[back_list].frontend_info;
        const BackendInfo &back_info = renderer_->backend_info();

        /*const uint64_t
            front_dur = front_info.end_timepoint_us - front_info.start_timepoint_us,
            back_dur = back_info.cpu_end_timepoint_us - back_info.cpu_start_timepoint_us;

        LOGI("Frontend: %04lld\tBackend(cpu): %04lld", (long long)front_dur, (long
        long)back_dur);*/

        ItemsInfo items_info;
        items_info.lights_count = main_view_lists_[back_list].lights.count;
        items_info.decals_count = main_view_lists_[back_list].decals.count;
        items_info.probes_count = main_view_lists_[back_list].probes.count;
        items_info.items_total = main_view_lists_[back_list].items.count;

        debug_ui_->UpdateInfo(front_info, back_info, items_info, *swap_interval_, render_flags);
        debug_ui_->Draw(r);
    }
}

void GSBaseState::UpdateFixed(const uint64_t dt_us) {
    physics_manager_->Update(scene_manager_->scene_data(), float(dt_us * 0.000001));

    { // invalidate objects updated by physics manager
        uint32_t updated_count = 0;
        const uint32_t *updated_objects = physics_manager_->updated_objects(updated_count);
        scene_manager_->InvalidateObjects(updated_objects, updated_count, CompPhysicsBit);
    }
}

bool GSBaseState::HandleInput(const InputManager::Event &evt) {
    using namespace Ren;
    using namespace GSBaseStateInternal;

    switch (evt.type) {
    case RawInputEv::P1Down:
    case RawInputEv::P2Down:
    case RawInputEv::P1Up:
    case RawInputEv::P2Up:
    case RawInputEv::P1Move:
    case RawInputEv::P2Move: {
    } break;
    case RawInputEv::KeyDown: {
        if (evt.key_code == KeyLeftShift || evt.key_code == KeyRightShift) {
            shift_down_ = true;
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
    } break;
    case RawInputEv::KeyUp: {
        if (evt.key_code == KeyLeftShift || evt.key_code == KeyRightShift) {
            shift_down_ = false;
        }
    }
    case RawInputEv::Resize:
    default:
        break;
    }

    return true;
}

void GSBaseState::BackgroundProc() {
    __itt_thread_set_name("Renderer Frontend Thread");

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
    { // Update loop using fixed timestep
        auto input_manager = game_->GetComponent<InputManager>(INPUT_MANAGER_KEY);

        FrameInfo &fr = fr_info_;

        fr.cur_time_us = Sys::GetTimeUs();
        if (fr.cur_time_us < fr.prev_time_us) {
            fr.prev_time_us = 0;
        }
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

            this->UpdateFixed(UPDATE_DELTA);
            fr.time_acc_us -= UPDATE_DELTA;

            poll_time_point += UPDATE_DELTA;
        }

        fr.time_fract = double(fr.time_acc_us) / UPDATE_DELTA;
    }

    this->UpdateAnim(fr_info_.delta_time_us);

    // Update invalidated objects
    scene_manager_->UpdateObjects();

    if (!use_pt_ && !use_lm_) {
        if (update_all_probes_) {
            if (probes_to_update_.empty()) {
                const int obj_count = (int)scene_manager_->scene_data().objects.size();
                for (int i = 0; i < obj_count; i++) {
                    const SceneObject *obj = scene_manager_->GetObject(i);
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
            auto *probe =
                (LightProbe *)scene_manager_->scene_data().comp_store[CompProbe]->Get(probe_obj->components[CompProbe]);
            auto *probe_tr = (Transform *)scene_manager_->scene_data().comp_store[CompTransform]->Get(
                probe_obj->components[CompTransform]);

            auto pos = Ren::Vec4f{probe->offset[0], probe->offset[1], probe->offset[2], 1.0f};
            pos = probe_tr->world_from_object * pos;
            pos /= pos[3];

            static const Ren::Vec3f axises[] = {Ren::Vec3f{1.0f, 0.0f, 0.0f}, Ren::Vec3f{-1.0f, 0.0f, 0.0f},
                                                Ren::Vec3f{0.0f, 1.0f, 0.0f}, Ren::Vec3f{0.0f, -1.0f, 0.0f},
                                                Ren::Vec3f{0.0f, 0.0f, 1.0f}, Ren::Vec3f{0.0f, 0.0f, -1.0f}};

            static const Ren::Vec3f ups[] = {Ren::Vec3f{0.0f, -1.0f, 0.0f}, Ren::Vec3f{0.0f, -1.0f, 0.0f},
                                             Ren::Vec3f{0.0f, 0.0f, 1.0f},  Ren::Vec3f{0.0f, 0.0f, -1.0f},
                                             Ren::Vec3f{0.0f, -1.0f, 0.0f}, Ren::Vec3f{0.0f, -1.0f, 0.0f}};

            const auto center = Ren::Vec3f{pos[0], pos[1], pos[2]};

            for (int i = 0; i < 6; i++) {
                const Ren::Vec3f target = center + axises[i];
                temp_probe_cam_.SetupView(center, target, ups[i]);
                temp_probe_cam_.UpdatePlanes();

                temp_probe_lists_[i].render_flags = EnableZFill | EnableCulling | EnableLightmap | EnableLights |
                                                    EnableDecals | EnableShadows | EnableProbes;

                renderer_->PrepareDrawList(scene_manager_->scene_data(), temp_probe_cam_, temp_probe_lists_[i]);
            }

            probe_to_render_ = probe;
            probes_to_update_.pop_back();
        }

        auto &list = main_view_lists_[list_index];

        list.render_flags = render_flags_;

        renderer_->PrepareDrawList(scene_manager_->scene_data(), scene_manager_->main_cam(), list);

        scene_manager_->UpdateTexturePriorities(list.visible_textures.data, list.visible_textures.count,
                                                list.desired_textures.data, list.desired_textures.count);
    }

    DrawUI(ui_renderer_.get(), ui_root_.get());
}
