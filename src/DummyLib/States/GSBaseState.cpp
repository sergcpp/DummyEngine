#include "GSBaseState.h"

#include <cctype>

#include <fstream>
#include <future>
#include <map>
#include <memory>

#include <optick/optick.h>
#if !defined(RELEASE_FINAL) && !defined(__ANDROID__)
#include <vtune/ittnotify.h>
#endif

#include <stb/stb_image.h>
#include <stb/stb_image_write.h>

#include <Eng/Log.h>
#include <Eng/ViewerStateManager.h>
#include <Eng/gui/Image9Patch.h>
#include <Eng/gui/Renderer.h>
#include <Eng/renderer/Renderer.h>
#include <Eng/scene/PhysicsManager.h>
#include <Eng/scene/SceneManager.h>
#include <Eng/utils/Cmdline.h>
#include <Eng/utils/Load.h>
#include <Eng/utils/Random.h>
#include <Ren/Context.h>
#include <Sys/AssetFile.h>
#include <Sys/Json.h>
#include <Sys/MemBuf.h>
#include <Sys/ScopeExit.h>
#include <Sys/ThreadPool.h>
#include <Sys/Time_.h>
#undef GetObject

#include "../Gui/DebugInfoUI.h"
#include "../Gui/FontStorage.h"
#include "../Viewer.h"

namespace Ray {
extern const int LUT_DIMS;
extern const uint32_t *transform_luts[];
} // namespace Ray

namespace RendererInternal {
extern const int TaaSampleCountStatic;
}

namespace GSBaseStateInternal {
const int MAX_CMD_LINES = 8;
const bool USE_TWO_THREADS = true;
} // namespace GSBaseStateInternal

GSBaseState::GSBaseState(Viewer *viewer) : viewer_(viewer) {
    using namespace GSBaseStateInternal;

    cmdline_ = viewer->cmdline();

    ren_ctx_ = viewer->ren_ctx();
    snd_ctx_ = viewer->snd_ctx();
    log_ = viewer->log();

    renderer_ = viewer->renderer();
    scene_manager_ = viewer->scene_manager();
    physics_manager_ = viewer->physics_manager();
    shader_loader_ = viewer->shader_loader();
    threads_ = viewer->threads();

    ui_renderer_ = viewer->ui_renderer();
    ui_root_ = viewer->ui_root();

    font_ = viewer->font_storage()->FindFont("main_font");

    debug_ui_ = viewer->debug_ui();

    cmdline_back_ = std::make_unique<Gui::Image9Patch>(
        *ren_ctx_, (std::string(ASSETS_BASE_PATH) + "/textures/editor/dial_edit_back.uncompressed.tga").c_str(),
        Ren::Vec2f{1.5f, 1.5f}, 1.0f, Ren::Vec2f{-1.0f, -1.0f}, Ren::Vec2f{2.0f, 2.0f}, ui_root_);

    random_ = viewer->random();

    // Prepare cam for probes updating
    temp_probe_cam_.Perspective(90.0f, 1.0f, 0.1f, 10000.0f);
    temp_probe_cam_.set_render_mask(uint32_t(Eng::Drawable::eVisibility::Probes));

    //
    // Create required staging buffers
    //
    Ren::BufferRef instance_indices_stage_buf = ren_ctx_->LoadBuffer(
        "Instance Indices (Upload)", Ren::eBufType::Upload, Eng::InstanceIndicesBufChunkSize * Ren::MaxFramesInFlight);
    Ren::BufferRef skin_transforms_stage_buf = ren_ctx_->LoadBuffer(
        "Skin Transforms (Upload)", Ren::eBufType::Upload, Eng::SkinTransformsBufChunkSize * Ren::MaxFramesInFlight);
    Ren::BufferRef shape_keys_stage_buf = ren_ctx_->LoadBuffer("Shape Keys (Upload)", Ren::eBufType::Upload,
                                                               Eng::ShapeKeysBufChunkSize * Ren::MaxFramesInFlight);
    Ren::BufferRef cells_stage_buf =
        ren_ctx_->LoadBuffer("Cells (Upload)", Ren::eBufType::Upload, Eng::CellsBufChunkSize * Ren::MaxFramesInFlight);
    Ren::BufferRef rt_cells_stage_buf = ren_ctx_->LoadBuffer("RT Cells (Upload)", Ren::eBufType::Upload,
                                                             Eng::CellsBufChunkSize * Ren::MaxFramesInFlight);
    Ren::BufferRef items_stage_buf =
        ren_ctx_->LoadBuffer("Items (Upload)", Ren::eBufType::Upload, Eng::ItemsBufChunkSize * Ren::MaxFramesInFlight);
    Ren::BufferRef rt_items_stage_buf = ren_ctx_->LoadBuffer("RT Items (Upload)", Ren::eBufType::Upload,
                                                             Eng::ItemsBufChunkSize * Ren::MaxFramesInFlight);
    Ren::BufferRef lights_stage_buf = ren_ctx_->LoadBuffer("Lights (Upload)", Ren::eBufType::Upload,
                                                           Eng::LightsBufChunkSize * Ren::MaxFramesInFlight);
    Ren::BufferRef decals_stage_buf = ren_ctx_->LoadBuffer("Decals (Upload)", Ren::eBufType::Upload,
                                                           Eng::DecalsBufChunkSize * Ren::MaxFramesInFlight);
    Ren::BufferRef rt_geo_instances_stage_buf = ren_ctx_->LoadBuffer(
        "RT Geo Instances (Upload)", Ren::eBufType::Upload, Eng::RTGeoInstancesBufChunkSize * Ren::MaxFramesInFlight);
    Ren::BufferRef rt_sh_geo_instances_stage_buf =
        ren_ctx_->LoadBuffer("RT Shadow Geo Instances (Upload)", Ren::eBufType::Upload,
                             Eng::RTGeoInstancesBufChunkSize * Ren::MaxFramesInFlight);
    Ren::BufferRef rt_obj_instances_stage_buf, rt_sh_obj_instances_stage_buf, rt_tlas_nodes_stage_buf,
        rt_sh_tlas_nodes_stage_buf;
    if (ren_ctx_->capabilities.raytracing) {
        rt_obj_instances_stage_buf = ren_ctx_->LoadBuffer("RT Obj Instances (Upload)", Ren::eBufType::Upload,
                                                          Eng::HWRTObjInstancesBufChunkSize * Ren::MaxFramesInFlight);
        rt_sh_obj_instances_stage_buf =
            ren_ctx_->LoadBuffer("RT Shadow Obj Instances (Upload)", Ren::eBufType::Upload,
                                 Eng::HWRTObjInstancesBufChunkSize * Ren::MaxFramesInFlight);
    } else if (ren_ctx_->capabilities.swrt) {
        rt_obj_instances_stage_buf = ren_ctx_->LoadBuffer("RT Obj Instances (Upload)", Ren::eBufType::Upload,
                                                          Eng::SWRTObjInstancesBufChunkSize * Ren::MaxFramesInFlight);
        rt_sh_obj_instances_stage_buf =
            ren_ctx_->LoadBuffer("RT Shadow Obj Instances (Upload)", Ren::eBufType::Upload,
                                 Eng::SWRTObjInstancesBufChunkSize * Ren::MaxFramesInFlight);
        rt_tlas_nodes_stage_buf = ren_ctx_->LoadBuffer("SWRT TLAS Nodes (Upload)", Ren::eBufType::Upload,
                                                       Eng::SWRTTLASNodesBufChunkSize * Ren::MaxFramesInFlight);
        rt_sh_tlas_nodes_stage_buf = ren_ctx_->LoadBuffer("SWRT Shadow TLAS Nodes (Upload)", Ren::eBufType::Upload,
                                                          Eng::SWRTTLASNodesBufChunkSize * Ren::MaxFramesInFlight);
    }

    Ren::BufferRef shared_data_stage_buf = ren_ctx_->LoadBuffer("Shared Data (Upload)", Ren::eBufType::Upload,
                                                                Eng::SharedDataBlockSize * Ren::MaxFramesInFlight);

    //
    // Initialize draw lists
    //
    for (int i = 0; i < 2; i++) {
        main_view_lists_[i].Init(shared_data_stage_buf, instance_indices_stage_buf, skin_transforms_stage_buf,
                                 shape_keys_stage_buf, cells_stage_buf, rt_cells_stage_buf, items_stage_buf,
                                 rt_items_stage_buf, lights_stage_buf, decals_stage_buf, rt_geo_instances_stage_buf,
                                 rt_sh_geo_instances_stage_buf, rt_obj_instances_stage_buf,
                                 rt_sh_obj_instances_stage_buf, rt_tlas_nodes_stage_buf, rt_sh_tlas_nodes_stage_buf);
    }
}

GSBaseState::~GSBaseState() = default;

void GSBaseState::Enter() {
    using namespace GSBaseStateInternal;

    if (USE_TWO_THREADS) {
        background_thread_ = std::thread(std::bind(&GSBaseState::BackgroundProc, this));
    }

    /*{ // Create temporary buffer to update probes
        FrameBuf::ColorAttachmentDesc desc;
        desc.format = Ren::eTexFormat::RawRGB16F;
        desc.filter = Ren::eTexFilter::NoFilter;
        desc.wrap = Ren::eTexWrap::ClampToEdge;

        const int res = scene_manager_->scene_data().probe_storage.res();
        temp_probe_buf_ = FrameBuf("Temp probe", *ren_ctx_, res, res, &desc, 1, {}, 1, ren_ctx_->log());
    }*/

    cmdline_history_.emplace_back();

    cmdline_->RegisterCommand("r_wireframe", [this](const int argc, Eng::Cmdline::ArgData *argv) -> bool {
        renderer_->settings.debug_wireframe = !renderer_->settings.debug_wireframe;
        return true;
    });

    cmdline_->RegisterCommand("r_culling", [this](const int argc, Eng::Cmdline::ArgData *argv) -> bool {
        renderer_->settings.enable_culling = !renderer_->settings.enable_culling;
        return true;
    });

    cmdline_->RegisterCommand("r_lightmap", [this](const int argc, Eng::Cmdline::ArgData *argv) -> bool {
        renderer_->settings.enable_lightmap = !renderer_->settings.enable_lightmap;
        return true;
    });

    cmdline_->RegisterCommand("r_lights", [this](const int argc, Eng::Cmdline::ArgData *argv) -> bool {
        renderer_->settings.enable_lights = !renderer_->settings.enable_lights;
        return true;
    });

    cmdline_->RegisterCommand("r_decals", [this](const int argc, Eng::Cmdline::ArgData *argv) -> bool {
        renderer_->settings.enable_decals = !renderer_->settings.enable_decals;
        return true;
    });

    cmdline_->RegisterCommand("r_shadows", [this](const int argc, Eng::Cmdline::ArgData *argv) -> bool {
        if (argc > 1) {
            if (argv[1].val > 1.5) {
                renderer_->settings.shadows_quality = Eng::eShadowsQuality::Raytraced;
            } else if (argv[1].val > 0.5) {
                renderer_->settings.shadows_quality = Eng::eShadowsQuality::High;
            } else {
                renderer_->settings.shadows_quality = Eng::eShadowsQuality::Off;
            }
        }
        return true;
    });

    cmdline_->RegisterCommand("r_reflections", [this](const int argc, Eng::Cmdline::ArgData *argv) -> bool {
        if (argc > 1) {
            if (argv[1].val > 2.5) {
                renderer_->settings.reflections_quality = Eng::eReflectionsQuality::Raytraced_High;
            } else if (argv[1].val > 1.5) {
                renderer_->settings.reflections_quality = Eng::eReflectionsQuality::Raytraced_Normal;
            } else if (argv[1].val > 0.5) {
                renderer_->settings.reflections_quality = Eng::eReflectionsQuality::High;
            } else {
                renderer_->settings.reflections_quality = Eng::eReflectionsQuality::Off;
            }
        }
        return true;
    });

    cmdline_->RegisterCommand("r_taa", [this](const int argc, Eng::Cmdline::ArgData *argv) -> bool {
        if (argc > 1) {
            if (argv[1].val > 1.5) {
                renderer_->settings.taa_mode = Eng::eTAAMode::Static;
            } else if (argv[1].val > 0.5) {
                renderer_->settings.taa_mode = Eng::eTAAMode::Dynamic;
            } else {
                renderer_->settings.taa_mode = Eng::eTAAMode::Off;
            }
        }
        return true;
    });

    cmdline_->RegisterCommand("r_tonemap", [this](const int argc, Eng::Cmdline::ArgData *argv) -> bool {
        if (argc > 1) {
            if (argv[1].val > 3.5) {
                renderer_->settings.tonemap_mode = Eng::eTonemapMode::LUT;
                renderer_->SetTonemapLUT(
                    Ray::LUT_DIMS, Ren::eTexFormat::RawRGB10_A2,
                    Ren::Span<const uint8_t>(reinterpret_cast<const uint8_t *>(
                                                 Ray::transform_luts[int(Ray::eViewTransform::Filmic_HighContrast)]),
                                             4 * Ray::LUT_DIMS * Ray::LUT_DIMS * Ray::LUT_DIMS));
            } else if (argv[1].val > 2.5) {
                renderer_->settings.tonemap_mode = Eng::eTonemapMode::LUT;
                renderer_->SetTonemapLUT(
                    Ray::LUT_DIMS, Ren::eTexFormat::RawRGB10_A2,
                    Ren::Span<const uint8_t>(reinterpret_cast<const uint8_t *>(
                                                 Ray::transform_luts[int(Ray::eViewTransform::Filmic_MediumContrast)]),
                                             4 * Ray::LUT_DIMS * Ray::LUT_DIMS * Ray::LUT_DIMS));
            } else if (argv[1].val > 1.5) {
                renderer_->settings.tonemap_mode = Eng::eTonemapMode::LUT;
                renderer_->SetTonemapLUT(
                    Ray::LUT_DIMS, Ren::eTexFormat::RawRGB10_A2,
                    Ren::Span<const uint8_t>(
                        reinterpret_cast<const uint8_t *>(Ray::transform_luts[int(Ray::eViewTransform::AgX)]),
                        reinterpret_cast<const uint8_t *>(Ray::transform_luts[int(Ray::eViewTransform::AgX)]) +
                            4 * Ray::LUT_DIMS * Ray::LUT_DIMS * Ray::LUT_DIMS));
            } else if (argv[1].val > 0.5) {
                renderer_->settings.tonemap_mode = Eng::eTonemapMode::Standard;
            } else {
                renderer_->settings.tonemap_mode = Eng::eTonemapMode::Off;
            }
        }
        return true;
    });

    cmdline_->RegisterCommand("r_pt", [this](const int argc, Eng::Cmdline::ArgData *argv) -> bool {
        use_pt_ = !use_pt_;
        if (use_pt_) {
            InitRenderer_PT();
            InitScene_PT();
            invalidate_view_ = true;
        }
        return true;
    });

    /*cmdline_->RegisterCommand("r_lm", [this](const int argc, Eng::Cmdline::ArgData *argv) -> bool {
        use_lm_ = !use_lm_;
        if (use_lm_) {
            // shrd_this->scene_manager_->InitScene_PT();
            // shrd_this->scene_manager_->ResetLightmaps_PT();
            invalidate_view_ = true;
        }
        return true;
    });*/

    cmdline_->RegisterCommand("r_zfill", [this](const int argc, Eng::Cmdline::ArgData *argv) -> bool {
        renderer_->settings.enable_zfill = !renderer_->settings.enable_zfill;
        return true;
    });

    cmdline_->RegisterCommand("r_mode", [this](const int argc, Eng::Cmdline::ArgData *argv) -> bool {
        if (argv[1].val > 0.5) {
            renderer_->settings.render_mode = Eng::eRenderMode::Forward;
        } else {
            renderer_->settings.render_mode = Eng::eRenderMode::Deferred;
        }
        return true;
    });

    cmdline_->RegisterCommand("r_gi", [this](const int argc, Eng::Cmdline::ArgData *argv) -> bool {
        if (argv[1].val > 2.5) {
            renderer_->settings.gi_quality = Eng::eGIQuality::Ultra;
        } else if (argv[1].val > 1.5) {
            renderer_->settings.gi_quality = Eng::eGIQuality::High;
        } else if (argv[1].val > 0.5) {
            renderer_->settings.gi_quality = Eng::eGIQuality::Medium;
        } else {
            renderer_->settings.gi_quality = Eng::eGIQuality::Off;
        }
        return true;
    });

    cmdline_->RegisterCommand("r_sky", [this](const int argc, Eng::Cmdline::ArgData *argv) -> bool {
        if (argv[1].val > 0.5) {
            renderer_->settings.sky_quality = Eng::eSkyQuality::High;
        } else {
            renderer_->settings.sky_quality = Eng::eSkyQuality::Low;
        }
        return true;
    });

    cmdline_->RegisterCommand("r_updateProbes", [this](const int argc, Eng::Cmdline::ArgData *argv) -> bool {
        Eng::SceneData &scene_data = scene_manager_->scene_data();

        const int res = scene_data.probe_storage.res(), capacity = scene_data.probe_storage.capacity();
        const bool result =
            scene_data.probe_storage.Resize(ren_ctx_->api_ctx(), ren_ctx_->default_mem_allocs(),
                                            Ren::eTexFormat::RawRGBA8888, res, capacity, ren_ctx_->log());
        assert(result);

        update_all_probes_ = true;

        return true;
    });

    cmdline_->RegisterCommand("r_cacheProbes", [this](const int argc, Eng::Cmdline::ArgData *argv) -> bool {
        const Eng::SceneData &scene_data = scene_manager_->scene_data();

        const Eng::CompStorage *lprobes = scene_data.comp_store[Eng::CompProbe];
        Eng::SceneManager::WriteProbeCache("assets/textures/probes_cache", scene_data.name.c_str(),
                                           scene_data.probe_storage, lprobes, ren_ctx_->log());

        // probe textures were written, convert them
        Viewer::PrepareAssets("pc");

        return true;
    });

    cmdline_->RegisterCommand("map", [this](const int argc, Eng::Cmdline::ArgData *argv) -> bool {
        if (argc != 2 || argv[1].type != Eng::Cmdline::eArgType::ArgString) {
            return false;
        }

        // TODO: refactor this
        char buf[1024];
        snprintf(buf, sizeof(buf), "%s/scenes/%.*s", ASSETS_BASE_PATH, int(argv[1].str.length()), argv[1].str.data());
        LoadScene(buf);

        return true;
    });

    cmdline_->RegisterCommand("save", [this](const int argc, Eng::Cmdline::ArgData *argv) -> bool {
        Sys::MultiPoolAllocator<char> alloc(32, 512);
        JsObjectP out_scene(alloc);

        SaveScene(out_scene);

        { // Write output file
            std::string name_str;

            { // get scene file name
                const Eng::SceneData &scene_data = scene_manager_->scene_data();
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
                            ren_ctx_->log()->Error("Failed to remove file %s", name2.c_str());
                            return false;
                        }
                    }

                    const int ret = std::rename(name1.c_str(), name2.c_str());
                    if (ret) {
                        ren_ctx_->log()->Error("Failed to rename file %s", name1.c_str());
                        return false;
                    }
                }
            }

            { // write scene file
                const std::string name1 = "assets/scenes/" + name_str + ".json",
                                  name2 = "assets/scenes/" + name_str + ".json1";

                const int ret = std::rename(name1.c_str(), name2.c_str());
                if (ret) {
                    ren_ctx_->log()->Error("Failed to rename file %s", name1.c_str());
                    return false;
                }

                std::ofstream out_file(name1, std::ios::binary);
                out_file.precision(std::numeric_limits<double>::max_digits10);
                out_scene.Write(out_file);
            }
        }

        // scene file was written, copy it to assets_pc folder
        Viewer::PrepareAssets("pc");

        return true;
    });

    cmdline_->RegisterCommand("r_reloadTextures", [this](const int argc, Eng::Cmdline::ArgData *argv) -> bool {
        scene_manager_->ForceTextureReload();
        return true;
    });

    cmdline_->RegisterCommand("r_showCull", [this](const int argc, Eng::Cmdline::ArgData *argv) -> bool {
        renderer_->settings.debug_culling = !renderer_->settings.debug_culling;
        return true;
    });

    cmdline_->RegisterCommand("r_showShadows", [this](const int argc, Eng::Cmdline::ArgData *argv) -> bool {
        renderer_->settings.debug_shadows = !renderer_->settings.debug_shadows;
        return true;
    });

    cmdline_->RegisterCommand("r_showLights", [this](const int argc, Eng::Cmdline::ArgData *argv) -> bool {
        renderer_->settings.debug_lights = !renderer_->settings.debug_lights;
        return true;
    });

    cmdline_->RegisterCommand("r_showDecals", [this](const int argc, Eng::Cmdline::ArgData *argv) -> bool {
        renderer_->settings.debug_decals = !renderer_->settings.debug_decals;
        return true;
    });

    cmdline_->RegisterCommand("r_showDeferred", [this](const int argc, Eng::Cmdline::ArgData *argv) -> bool {
        renderer_->settings.debug_deferred = !renderer_->settings.debug_deferred;
        return true;
    });

    cmdline_->RegisterCommand("r_showBlur", [this](const int argc, Eng::Cmdline::ArgData *argv) -> bool {
        renderer_->settings.debug_blur = !renderer_->settings.debug_blur;
        return true;
    });

    cmdline_->RegisterCommand("r_showSSAO", [this](const int argc, Eng::Cmdline::ArgData *argv) -> bool {
        renderer_->settings.debug_ssao = !renderer_->settings.debug_ssao;
        return true;
    });

    cmdline_->RegisterCommand("r_showTimings", [this](const int argc, Eng::Cmdline::ArgData *argv) -> bool {
        renderer_->settings.debug_timings = !renderer_->settings.debug_timings;
        return true;
    });

    cmdline_->RegisterCommand("r_showBVH", [this](const int argc, Eng::Cmdline::ArgData *argv) -> bool {
        renderer_->settings.debug_bvh = !renderer_->settings.debug_bvh;
        return true;
    });

    cmdline_->RegisterCommand("r_showProbes", [this](const int argc, Eng::Cmdline::ArgData *argv) -> bool {
        renderer_->settings.debug_probes = !renderer_->settings.debug_probes;
        return true;
    });

    cmdline_->RegisterCommand("r_showEllipsoids", [this](const int argc, Eng::Cmdline::ArgData *argv) -> bool {
        renderer_->settings.debug_ellipsoids = !renderer_->settings.debug_ellipsoids;
        return true;
    });

    cmdline_->RegisterCommand("r_showRT", [this](const int argc, Eng::Cmdline::ArgData *argv) -> bool {
        if (argc > 1) {
            if (argv[1].val > 0.5) {
                renderer_->settings.debug_rt = Eng::eDebugRT::Shadow;
            } else {
                renderer_->settings.debug_rt = Eng::eDebugRT::Main;
            }
        } else {
            renderer_->settings.debug_rt = Eng::eDebugRT::Off;
        }
        return true;
    });

    cmdline_->RegisterCommand("r_showDenoise", [this](const int argc, Eng::Cmdline::ArgData *argv) -> bool {
        if (argc > 1) {
            if (argv[1].val < 0.5) {
                renderer_->settings.debug_denoise = Eng::eDebugDenoise::Reflection;
            } else if (argv[1].val < 1.5) {
                renderer_->settings.debug_denoise = Eng::eDebugDenoise::GI;
            } else if (argv[2].val < 2.5) {
                renderer_->settings.debug_denoise = Eng::eDebugDenoise::Shadow;
            }
        } else {
            renderer_->settings.debug_denoise = Eng::eDebugDenoise::Off;
        }
        return true;
    });

    cmdline_->RegisterCommand("r_showMotion", [this](const int argc, Eng::Cmdline::ArgData *argv) -> bool {
        renderer_->settings.debug_motion = !renderer_->settings.debug_motion;
        return true;
    });

    cmdline_->RegisterCommand("r_showUI", [this](const int argc, Eng::Cmdline::ArgData *argv) -> bool {
        ui_enabled_ = !ui_enabled_;
        return true;
    });

    cmdline_->RegisterCommand("r_freeze", [this](const int argc, Eng::Cmdline::ArgData *argv) -> bool {
        renderer_->settings.debug_freeze = !renderer_->settings.debug_freeze;
        return true;
    });

    // Initialize first draw list
    UpdateFrame(0);
}

bool GSBaseState::LoadScene(std::string_view name) {
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

    Sys::MultiPoolAllocator<char> alloc(32, 512);
    JsObjectP js_scene(alloc), js_probe_cache(alloc);

    { // Load scene data from file
        std::string scene_file =
#if defined(__ANDROID__)
            "assets/";
#else
            "assets_pc/";
#endif
        scene_file += name;
        Sys::AssetFile in_scene(scene_file);
        if (!in_scene) {
            log_->Error("Can not open scene file %s", scene_file.c_str());
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
        cache_file += name;

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

    if (!viewer_->app_params.ref_name.empty()) {
        Ren::Tex2DParams params;
        params.w = viewer_->width;
        params.h = viewer_->height;
        params.format = Ren::eTexFormat::RawRGBA8888;
        params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
        params.sampling.wrap = Ren::eTexWrap::ClampToEdge;
        params.usage = Ren::eTexUsage::RenderTarget | Ren::eTexUsage::Transfer;
#if defined(USE_GL_RENDER)
        params.flags = Ren::eTexFlagBits::SRGB;
#endif

        Ren::eTexLoadStatus status;
        capture_result_ = ren_ctx_->LoadTexture2D("Capture Result", params, ren_ctx_->default_mem_allocs(), &status);
        assert(status == Ren::eTexLoadStatus::CreatedDefault);
    }

    sun_dir_ = scene_manager_->scene_data().env.sun_dir;
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
    OPTICK_EVENT("GSBaseState::UpdateAnim");
    cmdline_cursor_blink_us_ += dt_us;
    if (cmdline_cursor_blink_us_ > 1000000 || !cmdline_input_.empty()) {
        cmdline_cursor_blink_us_ = 0;
    }
}

void GSBaseState::Draw() {
    using namespace GSBaseStateInternal;

    OPTICK_GPU_EVENT("Draw");

    const bool animate_texture_lod = viewer_->app_params.ref_name.empty();
    if (streaming_finished_ && !viewer_->app_params.ref_name.empty() && !capture_started_) {
        if (viewer_->app_params.pt) {
            InitRenderer_PT();
            InitScene_PT();
            use_pt_ = true;
            invalidate_view_ = true;
        } else {
            if (USE_TWO_THREADS) {
                std::unique_lock<std::mutex> lock(mtx_);
                while (notified_) {
                    thr_done_.wait(lock);
                }
            }
            main_view_lists_[0].Clear();
            main_view_lists_[1].Clear();
            random_->Reset(0);
            renderer_->settings.taa_mode = Eng::eTAAMode::Static;
            renderer_->settings.gi_quality = Eng::eGIQuality::Ultra;
            renderer_->settings.reflections_quality = Eng::eReflectionsQuality::Raytraced_High;
            main_view_lists_[0].render_settings = renderer_->settings;
            renderer_->reset_accumulation();
        }
        capture_started_ = true;
        log_->Info("Starting capture!");
    }

    Ren::Tex2DRef render_target;
    if (capture_started_) {
        if (use_pt_) {
            const int iteration = ray_reg_ctx_.empty() ? 0 : ray_reg_ctx_[0].iteration;
            if (iteration < viewer_->app_params.pt_max_samples) {
                render_target = capture_result_;
                log_->Info("Capturing iteration #%i", iteration);
            } else {
                log_->Info("Capture finished! (%i samples)", iteration);
                viewer_->exit_status = WriteAndValidateCaptureResult();
                viewer_->Quit();
                return;
            }
        } else {
            if (renderer_->accumulated_frames() < RendererInternal::TaaSampleCountStatic) {
                render_target = capture_result_;
                log_->Info("Capturing iteration #%i", renderer_->accumulated_frames());
            } else {
                log_->Info("Capture finished! (%i samples)", renderer_->accumulated_frames());
                viewer_->exit_status = WriteAndValidateCaptureResult();
                viewer_->Quit();
                return;
            }
        }
    }

    if (cmdline_enabled_) {
        // Process comandline input
        for (const Eng::InputManager::Event &evt : cmdline_input_) {
            if (evt.key_code == Eng::KeyDelete) {
                if (!cmdline_history_.back().empty()) {
                    cmdline_history_.back().pop_back();
                }
            } else if (evt.key_code == Eng::KeyReturn) {
                cmdline_->Execute(cmdline_history_.back().c_str());

                cmdline_history_.emplace_back();
                cmdline_history_index_ = -1;
                if (cmdline_history_.size() > MAX_CMD_LINES) {
                    cmdline_history_.erase(cmdline_history_.begin());
                }
            } else if (evt.key_code == Eng::KeyTab) {
                Ren::String hint_str;
                const int index = cmdline_->NextHint(cmdline_history_.back().c_str(), -1, hint_str);
                if (!hint_str.empty()) {
                    cmdline_history_.back() = hint_str.c_str();
                }
            } else if (evt.key_code == Eng::KeyGrave) {
                if (!cmdline_history_.back().empty()) {
                    cmdline_history_.emplace_back();
                    cmdline_history_index_ = -1;
                    if (cmdline_history_.size() > MAX_CMD_LINES) {
                        cmdline_history_.erase(cmdline_history_.begin());
                    }
                }
            } else if (evt.key_code == Eng::KeyUp) {
                cmdline_history_index_ = std::min(++cmdline_history_index_, int(cmdline_history_.size()) - 2);
                cmdline_history_.back() = cmdline_history_[cmdline_history_.size() - 2 - cmdline_history_index_];
            } else if (evt.key_code == Eng::KeyDown) {
                cmdline_history_index_ = std::max(--cmdline_history_index_, 0);
                cmdline_history_.back() = cmdline_history_[cmdline_history_.size() - 2 - cmdline_history_index_];
            } else {
                char ch = Eng::InputManager::CharFromKeycode(evt.key_code);
                if (shift_down_) {
                    if (ch == '-') {
                        ch = '_';
                    } else {
                        ch = toupper(ch);
                    }
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

            streaming_finished_ = scene_manager_->Serve(4, animate_texture_lod);
            renderer_->InitBackendInfo();

            if (use_pt_) {
                const Ren::Camera &cam = scene_manager_->main_cam();
                SetupView_PT(cam.world_position(), -cam.view_dir(), cam.up(), cam.angle());
                if (invalidate_view_) {
                    Clear_PT();
                    invalidate_view_ = false;
                }

                back_list = -1;
            } else {
                back_list = front_list_;
                front_list_ = (front_list_ + 1) % 2;

                if (invalidate_view_) {
                    renderer_->reset_accumulation();
                    invalidate_view_ = false;
                }
            }

            // Target frontend to next frame
            ren_ctx_->frontend_frame = (ren_ctx_->backend_frame() + 1) % Ren::MaxFramesInFlight;

            scene_manager_->scene_data().env.sun_dir = sun_dir_;
            if (prev_sun_dir_[0] != sun_dir_[0] || prev_sun_dir_[1] != sun_dir_[1] || prev_sun_dir_[2] != sun_dir_[2]) {
                if (renderer_->settings.taa_mode == Eng::eTAAMode::Static) {
                    renderer_->reset_accumulation();
                }
                prev_sun_dir_ = scene_manager_->scene_data().env.sun_dir;
            }

            notified_ = true;
            thr_notify_.notify_one();
        } else {
            streaming_finished_ = scene_manager_->Serve(4, animate_texture_lod);
            renderer_->InitBackendInfo();

            scene_manager_->scene_data().env.sun_dir = sun_dir_;
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
            renderer_->ExecuteDrawList(main_view_lists_[back_list], scene_manager_->persistent_data(), render_target,
                                       true);
        } else {
            Draw_PT(render_target);
        }
    }

    ui_renderer_->Draw(ren_ctx_->w(), ren_ctx_->h());
}

void GSBaseState::DrawUI(Gui::Renderer *r, Gui::BaseElement *root) {
    using namespace GSBaseStateInternal;

    OPTICK_EVENT();

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

        const bool debug_items = renderer_->settings.debug_lights || renderer_->settings.debug_decals;
        const Eng::FrontendInfo front_info = main_view_lists_[back_list].frontend_info;
        const Eng::BackendInfo &back_info = renderer_->backend_info();

        /*const uint64_t
            front_dur = front_info.end_timepoint_us - front_info.start_timepoint_us,
            back_dur = back_info.cpu_end_timepoint_us - back_info.cpu_start_timepoint_us;

        LOGI("Frontend: %04lld\tBackend(cpu): %04lld", (long long)front_dur, (long
        long)back_dur);*/

        Eng::ItemsInfo items_info;
        items_info.lights_count = uint32_t(main_view_lists_[back_list].lights.size());
        items_info.decals_count = uint32_t(main_view_lists_[back_list].decals.size());
        items_info.probes_count = uint32_t(main_view_lists_[back_list].probes.size());
        items_info.items_total = main_view_lists_[back_list].items.count;

        debug_ui_->UpdateInfo(front_info, back_info, items_info, debug_items);
        debug_ui_->Draw(r);
    }
}

void GSBaseState::UpdateFixed(const uint64_t dt_us) {
    physics_manager_->Update(scene_manager_->scene_data(), float(dt_us * 0.000001));

    { // invalidate objects updated by physics manager
        Ren::Span<const uint32_t> updated_objects = physics_manager_->updated_objects();
        scene_manager_->InvalidateObjects(updated_objects, Eng::CompPhysicsBit);
    }
}

bool GSBaseState::HandleInput(const Eng::InputManager::Event &evt) {
    using namespace Ren;
    using namespace GSBaseStateInternal;

    switch (evt.type) {
    case Eng::RawInputEv::P1Down:
    case Eng::RawInputEv::P2Down:
    case Eng::RawInputEv::P1Up:
    case Eng::RawInputEv::P2Up:
    case Eng::RawInputEv::P1Move:
    case Eng::RawInputEv::P2Move: {
    } break;
    case Eng::RawInputEv::KeyDown: {
        if (evt.key_code == Eng::KeyLeftShift || evt.key_code == Eng::KeyRightShift) {
            shift_down_ = true;
        } else if (evt.key_code == Eng::KeyDelete || evt.key_code == Eng::KeyReturn || evt.key_code == Eng::KeyTab) {
            if (cmdline_enabled_) {
                cmdline_input_.push_back(evt);
            }
        } else if (evt.key_code == Eng::KeyGrave) {
            cmdline_enabled_ = !cmdline_enabled_;
            if (cmdline_enabled_) {
                cmdline_input_.push_back(evt);
            }
        } else if (cmdline_enabled_) {
            cmdline_input_.push_back(evt);
        }
    } break;
    case Eng::RawInputEv::KeyUp: {
        if (evt.key_code == Eng::KeyLeftShift || evt.key_code == Eng::KeyRightShift) {
            shift_down_ = false;
        }
    } break;
    default:
        break;
    }

    return true;
}

void GSBaseState::BackgroundProc() {
    __itt_thread_set_name("Renderer Frontend Thread");
    OPTICK_FRAME("Renderer Frontend Thread");

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
        OPTICK_EVENT("Update Loop");
        Eng::InputManager *input_manager = viewer_->input_manager();

        Eng::FrameInfo &fr = fr_info_;

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

        while (fr.time_acc_us >= Eng::UPDATE_DELTA) {
            Eng::InputManager::Event evt;
            while (input_manager->PollEvent(poll_time_point, evt)) {
                this->HandleInput(evt);
            }

            this->UpdateFixed(Eng::UPDATE_DELTA);
            fr.time_acc_us -= Eng::UPDATE_DELTA;

            poll_time_point += Eng::UPDATE_DELTA;
        }

        fr.time_fract = double(fr.time_acc_us) / Eng::UPDATE_DELTA;
    }

    this->UpdateAnim(fr_info_.delta_time_us);

    // Update invalidated objects
    scene_manager_->UpdateObjects();

    if (!use_pt_ && !use_lm_) {
        OPTICK_EVENT("Prepare Frame");

        if (update_all_probes_) {
            if (probes_to_update_.empty()) {
                const int obj_count = (int)scene_manager_->scene_data().objects.size();
                for (int i = 0; i < obj_count; i++) {
                    const Eng::SceneObject *obj = scene_manager_->GetObject(i);
                    if (obj->comp_mask & Eng::CompProbeBit) {
                        probes_to_update_.push_back(i);
                    }
                }
            }
            update_all_probes_ = false;
        }

        if (!probes_to_update_.empty() && !probe_to_render_ && !probe_to_update_sh_) {
            log_->Info("Updating probe");
            Eng::SceneObject *probe_obj = scene_manager_->GetObject(probes_to_update_.back());
            auto *probe = (Eng::LightProbe *)scene_manager_->scene_data().comp_store[Eng::CompProbe]->Get(
                probe_obj->components[Eng::CompProbe]);
            auto *probe_tr = (Eng::Transform *)scene_manager_->scene_data().comp_store[Eng::CompTransform]->Get(
                probe_obj->components[Eng::CompTransform]);

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

                temp_probe_lists_[i].render_settings.reflections_quality = Eng::eReflectionsQuality::Off;

                renderer_->PrepareDrawList(scene_manager_->scene_data(), temp_probe_cam_, temp_probe_cam_,
                                           temp_probe_lists_[i]);
            }

            probe_to_render_ = probe;
            probes_to_update_.pop_back();
        }

        auto &list = main_view_lists_[list_index];

        list.render_settings = renderer_->settings;

        renderer_->PrepareDrawList(scene_manager_->scene_data(), scene_manager_->main_cam(), scene_manager_->ext_cam(),
                                   list);

        scene_manager_->UpdateTexturePriorities(list.visible_textures, list.desired_textures);
    }

    if (ui_enabled_) {
        DrawUI(ui_renderer_, ui_root_);
    }
}

void GSBaseState::InitRenderer_PT() {
    if (!ray_renderer_) {
        Ray::settings_t s;
        s.w = ren_ctx_->w();
        s.h = ren_ctx_->h();
        s.use_spatial_cache = viewer_->app_params.ref_name.empty();
        if (!viewer_->app_params.device_name.empty()) {
            s.preferred_device = viewer_->app_params.device_name.c_str();
        }
        s.use_hwrt = !viewer_->app_params.pt_nohwrt;
        s.validation_level = viewer_->app_params.validation_level;
        ray_renderer_ = std::unique_ptr<Ray::RendererBase>(Ray::CreateRenderer(s, viewer_->ray_log()));

        Ray::unet_filter_properties_t unet_props;
        ray_renderer_->InitUNetFilter(true, unet_props);
        unet_filter_passes_count_ = unet_props.pass_count;
    }
}

void GSBaseState::InitScene_PT() {
    const Eng::SceneData &scene_data = scene_manager_->scene_data();
    const Eng::render_settings_t &settings = renderer_->settings;

    ray_scene_ = std::unique_ptr<Ray::SceneBase>(ray_renderer_->CreateScene());
    { // Setup environment
        Ray::environment_desc_t env_desc;
        env_desc.env_col[0] = env_desc.back_col[0] = scene_data.env.env_col[0];
        env_desc.env_col[1] = env_desc.back_col[1] = scene_data.env.env_col[1];
        env_desc.env_col[2] = env_desc.back_col[2] = scene_data.env.env_col[2];

        if (!scene_data.env.env_map_name.empty()) {
            if (scene_data.env.env_map_name == "physical_sky") {
                env_desc.back_map = env_desc.env_map = Ray::PhysicalSkyTexture;
            } else {
                std::string env_map_path = "assets_pc/textures/";
                env_map_path += scene_data.env.env_map_name;
                env_map_path.replace(env_map_path.length() - 3, 3, "hdr");

                int width, height;
                const std::vector<uint8_t> image_rgbe = Eng::LoadHDR(env_map_path.c_str(), width, height);

                Ray::tex_desc_t tex_desc;
                tex_desc.w = width;
                tex_desc.h = height;
                tex_desc.data = image_rgbe;
                tex_desc.format = Ray::eTextureFormat::RGBA8888;
                tex_desc.is_srgb = false;
                tex_desc.force_no_compression = true;
                env_desc.env_map = env_desc.back_map = ray_scene_->AddTexture(tex_desc);
            }
        }

        env_desc.env_map_rotation = env_desc.back_map_rotation = scene_data.env.env_map_rot;

        ray_scene_->SetEnvironment(env_desc);
    }
    { // Add main camera
        Ray::camera_desc_t cam_desc;
        cam_desc.type = Ray::eCamType::Persp;
        cam_desc.view_transform = Ray::eViewTransform::Standard;
        cam_desc.filter = Ray::ePixelFilter::Box;
        if (settings.pixel_filter == Eng::ePixelFilter::Gaussian) {
            cam_desc.filter = Ray::ePixelFilter::Gaussian;
        } else if (settings.pixel_filter == Eng::ePixelFilter::BlackmanHarris) {
            cam_desc.filter = Ray::ePixelFilter::BlackmanHarris;
        }
        cam_desc.filter_width = settings.pixel_filter_width;
        cam_desc.origin[0] = cam_desc.origin[1] = cam_desc.origin[2] = 0.0f;
        cam_desc.fwd[0] = cam_desc.fwd[1] = 0.0f;
        cam_desc.fwd[2] = -1.0f;
        cam_desc.fov = scene_manager_->main_cam().angle();
        ray_scene_->AddCamera(cam_desc);
    }
    if (Length2(scene_data.env.sun_col) > 0.0f) {
        // Add sun light
        Ray::directional_light_desc_t sun_desc;
        memcpy(sun_desc.color, ValuePtr(scene_data.env.sun_col), 3 * sizeof(float));
        memcpy(sun_desc.direction, ValuePtr(scene_data.env.sun_dir), 3 * sizeof(float));
        sun_desc.angle = scene_data.env.sun_angle;
        pt_sun_light_ = ray_scene_->AddLight(sun_desc);
        prev_sun_dir_ = scene_data.env.sun_dir;
    }

    // Add default material
    Ray::principled_mat_desc_t default_mat_desc;
    default_mat_desc.base_color[0] = default_mat_desc.base_color[1] = default_mat_desc.base_color[2] = 0.5f;
    Ray::MaterialHandle default_mat = ray_scene_->AddMaterial(default_mat_desc);

    std::map<std::string, Ray::MeshHandle> loaded_meshes;
    std::map<std::string, Ray::MaterialHandle> loaded_materials;
    std::map<std::string, Ray::TextureHandle> loaded_textures;

    auto load_texture = [&](const Ren::Texture2D &tex, bool is_srgb = false, bool is_YCoCg = false) {
        if (tex.name() == "default_basecolor.dds" || tex.name() == "default_normalmap.dds" ||
            tex.name() == "default_roughness.dds" || tex.name() == "default_metallic.dds") {
            return Ray::InvalidTextureHandle;
        }
        auto tex_it = loaded_textures.find(tex.name().c_str());
        if (tex_it == loaded_textures.end()) {
            const int data_len = GetMipDataLenBytes(tex.params.w, tex.params.h, tex.params.format, tex.params.block);
            Ren::Buffer temp_stage_buf("Temp staging buf", ren_ctx_->api_ctx(), Ren::eBufType::Readback, data_len);
            Ren::CommandBuffer cmd_buf = ren_ctx_->BegTempSingleTimeCommands();
            tex.CopyTextureData(temp_stage_buf, cmd_buf, 0);
            const Ren::TransitionInfo transitions[] = {{&tex, Ren::eResState::ShaderResource}};
            Ren::TransitionResourceStates(ren_ctx_->api_ctx(), cmd_buf, Ren::AllStages, Ren::AllStages, transitions);
            ren_ctx_->EndTempSingleTimeCommands(cmd_buf);

            Ray::tex_desc_t tex_desc;
            switch (tex.params.format) {
            case Ren::eTexFormat::BC1:
                tex_desc.format = Ray::eTextureFormat::BC1;
                break;
            case Ren::eTexFormat::BC3:
                tex_desc.format = Ray::eTextureFormat::BC3;
                break;
            case Ren::eTexFormat::BC4:
                tex_desc.format = Ray::eTextureFormat::BC4;
                break;
            case Ren::eTexFormat::BC5:
                tex_desc.format = Ray::eTextureFormat::BC5;
                break;
            default:
                assert(false);
            }
            tex_desc.w = tex.params.w;
            tex_desc.h = tex.params.h;
            tex_desc.name = tex.name().c_str();
            tex_desc.is_srgb = is_srgb;
            tex_desc.is_YCoCg = is_YCoCg;
            tex_desc.reconstruct_z = true;

            const uint8_t *mapped_ptr = temp_stage_buf.Map();
            tex_desc.data = {mapped_ptr, temp_stage_buf.size()};

            const Ray::TextureHandle new_tex = ray_scene_->AddTexture(tex_desc);
            tex_it = loaded_textures.emplace(tex.name().c_str(), new_tex).first;

            temp_stage_buf.Unmap();
            temp_stage_buf.FreeImmediate();
        }
        return tex_it->second;
    };

    const auto *transforms = (Eng::Transform *)scene_data.comp_store[Eng::CompTransform]->SequentialData();
    const auto *drawables = (Eng::Drawable *)scene_data.comp_store[Eng::CompDrawable]->SequentialData();
    const auto *acc_structs = (Eng::AccStructure *)scene_data.comp_store[Eng::CompAccStructure]->SequentialData();
    const auto *lights_src = (Eng::LightSource *)scene_data.comp_store[Eng::CompLightSource]->SequentialData();

    for (const Eng::SceneObject &obj : scene_data.objects) {
        const uint32_t drawable_flags = Eng::CompDrawableBit | Eng::CompTransformBit;
        if ((obj.comp_mask & drawable_flags) == drawable_flags) {
            const Eng::Drawable &dr = drawables[obj.components[Eng::CompDrawable]];
            const Ren::Mesh *mesh = dr.mesh.get();
            assert(mesh->type() == Ren::eMeshType::Simple);
            const float *attribs = reinterpret_cast<const float *>(mesh->attribs());
            const int vtx_count = int(mesh->attribs_buf1().size / 16);
            const uint32_t *indices = reinterpret_cast<const uint32_t *>(mesh->indices());
            const int ndx_count = int(mesh->indices_buf().size / sizeof(uint32_t));

            Ray::MeshHandle mesh_handle = Ray::InvalidMeshHandle;

            if (dr.material_override.empty()) {
                auto mesh_it = loaded_meshes.find(mesh->name().c_str());
                if (mesh_it != loaded_meshes.end()) {
                    mesh_handle = mesh_it->second;
                }
            }

            if (mesh_handle == Ray::InvalidMeshHandle) {
                Ray::mesh_desc_t mesh_desc;
                mesh_desc.name = mesh->name().c_str();
                mesh_desc.prim_type = Ray::ePrimType::TriangleList;
                mesh_desc.vtx_positions = {{attribs, 13 * vtx_count}, 0, 13};
                mesh_desc.vtx_normals = {{attribs, 13 * vtx_count}, 3, 13};
                mesh_desc.vtx_binormals = {{attribs, 13 * vtx_count}, 6, 13};
                mesh_desc.vtx_uvs = {{attribs, 13 * vtx_count}, 9, 13};
                mesh_desc.vtx_indices = {indices, ndx_count};

                std::vector<Ray::mat_group_desc_t> mat_groups;

                const Ren::Span<const Ren::TriGroup> groups = mesh->groups();
                for (int j = 0; j < int(groups.size()); ++j) {
                    const Ren::TriGroup &grp = groups[j];

                    const Ren::Material *front_mat =
                        (j >= dr.material_override.size()) ? grp.front_mat.get() : dr.material_override[j].first.get();
                    const char *mat_name = front_mat->name().c_str();

                    std::pair<Ray::MaterialHandle, Ray::MaterialHandle> mat_handles;

                    auto mat_it = loaded_materials.find(mat_name);
                    if (mat_it == loaded_materials.end()) {
                        Ray::principled_mat_desc_t mat_desc;
                        memcpy(mat_desc.base_color, ValuePtr(front_mat->params[0]), 3 * sizeof(float));
                        mat_desc.base_texture = load_texture(*front_mat->textures[0], true, true);
                        mat_desc.roughness = front_mat->params[0][3];
                        mat_desc.roughness_texture = load_texture(*front_mat->textures[2]);
                        mat_desc.specular = 0.0f;
                        if (front_mat->params.size() > 1) {
                            mat_desc.sheen = front_mat->params[1][0];
                            mat_desc.sheen_tint = front_mat->params[1][1];
                            mat_desc.specular = front_mat->params[1][2];
                            mat_desc.specular_tint = front_mat->params[1][3];
                        }
                        if (front_mat->params.size() > 2) {
                            mat_desc.metallic = front_mat->params[2][0];
                            mat_desc.transmission = front_mat->params[2][1];
                            mat_desc.clearcoat = front_mat->params[2][2];
                            mat_desc.clearcoat_roughness = front_mat->params[2][3];
                        }
                        if (front_mat->textures.size() > 3) {
                            mat_desc.metallic_texture = load_texture(*front_mat->textures[3]);
                        }
                        if (front_mat->textures.size() > 4) {
                            mat_desc.alpha_texture = load_texture(*front_mat->textures[4]);
                        }
                        mat_desc.normal_map = load_texture(*front_mat->textures[1]);

                        const Ray::MaterialHandle new_mat = ray_scene_->AddMaterial(mat_desc);
                        mat_it = loaded_materials.emplace(mat_name, new_mat).first;
                    }
                    mat_handles = {mat_it->second, mat_it->second};

                    const Ren::Material *back_mat =
                        (j >= dr.material_override.size()) ? grp.back_mat.get() : dr.material_override[j].second.get();
                    if (front_mat != back_mat) {
                        Ray::principled_mat_desc_t mat_desc;
                        memcpy(mat_desc.base_color, ValuePtr(back_mat->params[0]), 3 * sizeof(float));
                        mat_desc.base_texture = load_texture(*back_mat->textures[0], true, true);
                        mat_desc.roughness = back_mat->params[0][3];
                        mat_desc.roughness_texture = load_texture(*back_mat->textures[2]);
                        mat_desc.specular = 0.0f;
                        if (back_mat->params.size() > 1) {
                            mat_desc.sheen = back_mat->params[1][0];
                            mat_desc.sheen_tint = back_mat->params[1][1];
                            mat_desc.specular = back_mat->params[1][2];
                            mat_desc.specular_tint = back_mat->params[1][3];
                        }
                        if (back_mat->params.size() > 2) {
                            mat_desc.metallic = back_mat->params[2][0];
                            mat_desc.transmission = back_mat->params[2][1];
                            mat_desc.clearcoat = back_mat->params[2][2];
                            mat_desc.clearcoat_roughness = back_mat->params[2][3];
                        }
                        if (back_mat->textures.size() > 3) {
                            mat_desc.metallic_texture = load_texture(*back_mat->textures[3]);
                        }
                        if (back_mat->textures.size() > 4) {
                            mat_desc.alpha_texture = load_texture(*back_mat->textures[4]);
                        }
                        mat_desc.normal_map = load_texture(*back_mat->textures[1]);

                        mat_handles.second = ray_scene_->AddMaterial(mat_desc);
                    }

                    mat_groups.emplace_back(mat_handles.first, mat_handles.second,
                                            size_t(grp.offset / sizeof(uint32_t)), size_t(grp.num_indices));
                }
                mesh_desc.groups = mat_groups;

                mesh_handle = ray_scene_->AddMesh(mesh_desc);
                if (dr.material_override.empty()) {
                    loaded_meshes.emplace(mesh->name().c_str(), mesh_handle).first;
                }
            }

            const Eng::Transform &tr = transforms[obj.components[Eng::CompTransform]];
            Ray::mesh_instance_desc_t mi;
            mi.xform = ValuePtr(tr.world_from_object);
            mi.mesh = mesh_handle;
            mi.camera_visibility = bool(dr.vis_mask & Eng::Drawable::eVisibility::Camera);
            mi.shadow_visibility = bool(dr.vis_mask & Eng::Drawable::eVisibility::Shadow);
            if ((obj.comp_mask & Eng::CompAccStructureBit) != 0) {
                const Eng::AccStructure &acc = acc_structs[obj.components[Eng::CompAccStructure]];
                mi.diffuse_visibility = bool(acc.vis_mask & Eng::AccStructure::eRayType::Diffuse);
                mi.specular_visibility = bool(acc.vis_mask & Eng::AccStructure::eRayType::Specular);
                mi.refraction_visibility = bool(acc.vis_mask & Eng::AccStructure::eRayType::Refraction);
            }
            [[maybe_unused]] const Ray::MeshInstanceHandle new_mi = ray_scene_->AddMeshInstance(mi);
        }

        const uint32_t lightsource_mask = Eng::CompLightSourceBit | Eng::CompTransformBit;
        if ((obj.comp_mask & lightsource_mask) == lightsource_mask) {
            const Eng::Transform &tr = transforms[obj.components[Eng::CompTransform]];
            const Eng::LightSource &ls = lights_src[obj.components[Eng::CompLightSource]];
            if (ls.power > 0.0f) {
                if (ls.type == Eng::eLightType::Sphere) {
                    auto pos = Ren::Vec4f{ls.offset[0], ls.offset[1], ls.offset[2], 1.0f};
                    pos = tr.world_from_object * pos;
                    pos /= pos[3];

                    if (ls.spot_angle < Ren::Pi<float>()) {
                        auto dir = Ren::Vec4f{ls.dir[0], ls.dir[1], ls.dir[2], 0.0f};
                        dir = tr.world_from_object * dir;

                        Ray::spot_light_desc_t spot_light_desc;
                        memcpy(spot_light_desc.color, ValuePtr(0.25f * ls.power * ls.col / ls.area), 3 * sizeof(float));
                        memcpy(spot_light_desc.position, ValuePtr(pos), 3 * sizeof(float));
                        memcpy(spot_light_desc.direction, ValuePtr(dir), 3 * sizeof(float));
                        spot_light_desc.radius = ls.radius;
                        spot_light_desc.spot_size = ls.angle_deg;
                        spot_light_desc.spot_blend = ls.spot_blend;
                        const Ray::LightHandle new_light = ray_scene_->AddLight(spot_light_desc);
                    } else {
                        Ray::sphere_light_desc_t sphere_light_desc;
                        memcpy(sphere_light_desc.color, ValuePtr(0.25f * ls.power * ls.col / ls.area),
                               3 * sizeof(float));
                        memcpy(sphere_light_desc.position, ValuePtr(pos), 3 * sizeof(float));
                        sphere_light_desc.radius = ls.radius;
                        const Ray::LightHandle new_light = ray_scene_->AddLight(sphere_light_desc);
                    }
                } else if (ls.type == Eng::eLightType::Rect) {
                    Ray::rect_light_desc_t rect_light_desc;
                    if (!ls.sky_portal) {
                        memcpy(rect_light_desc.color, ValuePtr(0.25f * ls.power * ls.col / ls.area), 3 * sizeof(float));
                    }
                    rect_light_desc.width = ls.width;
                    rect_light_desc.height = ls.height;
                    rect_light_desc.sky_portal = ls.sky_portal;
                    const Ray::LightHandle new_light =
                        ray_scene_->AddLight(rect_light_desc, ValuePtr(tr.world_from_object));
                } else if (ls.type == Eng::eLightType::Disk) {
                    Ray::disk_light_desc_t disk_light_desc;
                    memcpy(disk_light_desc.color, ValuePtr(0.25f * ls.power * ls.col / ls.area), 3 * sizeof(float));
                    disk_light_desc.size_x = ls.width;
                    disk_light_desc.size_y = ls.height;
                    const Ray::LightHandle new_light =
                        ray_scene_->AddLight(disk_light_desc, ValuePtr(tr.world_from_object));
                } else if (ls.type == Eng::eLightType::Line) {
                    Ray::line_light_desc_t line_light_desc;
                    memcpy(line_light_desc.color, ValuePtr(0.25f * ls.power * ls.col / ls.area), 3 * sizeof(float));
                    line_light_desc.radius = ls.radius;
                    line_light_desc.height = ls.height;
                    const Ray::LightHandle new_light =
                        ray_scene_->AddLight(line_light_desc, ValuePtr(tr.world_from_object));
                }
            }
        }
    }
    using namespace std::placeholders;
    ray_scene_->Finalize(std::bind(&Sys::ThreadPool::ParallelFor<Ray::ParallelForFunction>, threads_, _1, _2, _3));
}

void GSBaseState::SetupView_PT(const Ren::Vec3f &origin, const Ren::Vec3f &fwd, const Ren::Vec3f &up, const float fov) {
    Ray::camera_desc_t cam_desc;
    ray_scene_->GetCamera(Ray::CameraHandle{0}, cam_desc);

    memcpy(&cam_desc.origin[0], ValuePtr(origin), 3 * sizeof(float));
    memcpy(&cam_desc.fwd[0], ValuePtr(fwd), 3 * sizeof(float));
    memcpy(&cam_desc.up[0], ValuePtr(up), 3 * sizeof(float));
    cam_desc.fov = fov;

    ray_scene_->SetCamera(Ray::CameraHandle{0}, cam_desc);
}

void GSBaseState::Clear_PT() {
    for (Ray::RegionContext &c : ray_reg_ctx_) {
        c.Clear();
    }
    ray_renderer_->Clear({});
}

void GSBaseState::Draw_PT(const Ren::Tex2DRef &target) {
    auto [res_x, res_y] = ray_renderer_->size();

    if (res_x != ren_ctx_->w() || res_y != ren_ctx_->h()) {
        ray_reg_ctx_.clear();
        ray_renderer_->Resize(ren_ctx_->w(), ren_ctx_->h());
        res_x = ren_ctx_->w();
        res_y = ren_ctx_->h();
    }

    const Eng::SceneData &scene_data = scene_manager_->scene_data();
    if (Distance2(prev_sun_dir_, scene_data.env.sun_dir) > 0.001f && pt_sun_light_ != Ray::InvalidLightHandle) {
        ray_scene_->RemoveLight(pt_sun_light_);
        // Re-add sun light
        Ray::directional_light_desc_t sun_desc;
        memcpy(sun_desc.color, ValuePtr(scene_data.env.sun_col), 3 * sizeof(float));
        memcpy(sun_desc.direction, ValuePtr(scene_data.env.sun_dir), 3 * sizeof(float));
        sun_desc.angle = scene_data.env.sun_angle;
        pt_sun_light_ = ray_scene_->AddLight(sun_desc);
        prev_sun_dir_ = scene_data.env.sun_dir;

        using namespace std::placeholders;
        ray_scene_->Finalize(std::bind(&Sys::ThreadPool::ParallelFor<Ray::ParallelForFunction>, threads_, _1, _2, _3));
        invalidate_view_ = true;
    }

    if (ray_reg_ctx_.empty()) {
        if (Ray::RendererSupportsMultithreading(ray_renderer_->type())) {
            static const int TILE_SIZE = 64;
            for (int y = 0; y < res_y + TILE_SIZE - 1; y += TILE_SIZE) {
                for (int x = 0; x < res_x + TILE_SIZE - 1; x += TILE_SIZE) {
                    auto rect = Ray::rect_t{x, y, std::min(TILE_SIZE, res_x - x), std::min(TILE_SIZE, res_y - y)};
                    if (rect.w > 0 && rect.h > 0) {
                        ray_reg_ctx_.emplace_back(rect);
                    }
                }
            }
        } else {
            ray_reg_ctx_.emplace_back(Ray::rect_t{0, 0, res_x, res_y});
        }
    }

    if (Ray::RendererSupportsMultithreading(ray_renderer_->type())) {
        auto render_task = [this](const int i) { ray_renderer_->RenderScene(*ray_scene_, ray_reg_ctx_[i]); };
        std::vector<std::future<void>> ev(ray_reg_ctx_.size());
        for (int i = 0; i < int(ray_reg_ctx_.size()); i++) {
            ev[i] = threads_->Enqueue(render_task, i);
        }
        for (const std::future<void> &e : ev) {
            e.wait();
        }
    } else {
        ray_renderer_->UpdateSpatialCache(*ray_scene_, ray_reg_ctx_[0]);
        ray_renderer_->ResolveSpatialCache(*ray_scene_);
        ray_renderer_->RenderScene(*ray_scene_, ray_reg_ctx_[0]);
        for (int i = 0;
             i < unet_filter_passes_count_ && ray_reg_ctx_[0].iteration > 1 && viewer_->app_params.pt_denoise; ++i) {
            ray_renderer_->DenoiseImage(i, ray_reg_ctx_[0]);
        }
    }

    const Ray::color_data_rgba_t pixels = ray_renderer_->get_raw_pixels_ref();
    renderer_->BlitPixelsTonemap(reinterpret_cast<const uint8_t *>(pixels.ptr), res_x, res_y, pixels.pitch,
                                 Ren::eTexFormat::RawRGBA32F, scene_manager_->main_cam().gamma,
                                 scene_manager_->main_cam().min_exposure, scene_manager_->main_cam().max_exposure,
                                 target, false, true);
}

int GSBaseState::WriteAndValidateCaptureResult() {
    Ren::BufferRef stage_buf =
        ren_ctx_->LoadBuffer("Temp readback buf", Ren::eBufType::Readback, 4 * viewer_->width * viewer_->height);

    { // Download result
        Ren::CommandBuffer cmd_buf = ren_ctx_->BegTempSingleTimeCommands();
        capture_result_->CopyTextureData(*stage_buf, cmd_buf, 0);
        ren_ctx_->InsertReadbackMemoryBarrier(cmd_buf);
        ren_ctx_->EndTempSingleTimeCommands(cmd_buf);
    }

    const uint8_t *img_data = stage_buf->Map();
    SCOPE_EXIT({ stage_buf->Unmap(); })

    const std::string base_name =
        viewer_->app_params.scene_name.substr(7, viewer_->app_params.scene_name.size() - 7 - 5);
    const std::string out_name = base_name + ".png";
    const std::string diff_name = base_name + "_diff.png";

    int ref_w, ref_h, ref_channels;
    uint8_t *ref_img = stbi_load(("assets/" + viewer_->app_params.ref_name).c_str(), &ref_w, &ref_h, &ref_channels, 4);
    SCOPE_EXIT({ stbi_image_free(ref_img); })

    if (!ref_img || ref_w != viewer_->width || ref_h != viewer_->height) {
        log_->Error("Invalid reference image! (%s)", viewer_->app_params.ref_name.c_str());
        return -1;
    }

    const bool flip_y =
#if defined(USE_GL_RENDER)
        true;
#else
        false;
#endif

    double mse = 0.0;
    std::unique_ptr<uint8_t[]> diff_data_u8(new uint8_t[ref_w * ref_h * 3]);

    for (int j = 0; j < ref_h; ++j) {
        const int y = flip_y ? (ref_h - j - 1) : j;
        for (int i = 0; i < ref_w; ++i) {
            const uint8_t r = img_data[4 * (y * ref_w + i) + 0];
            const uint8_t g = img_data[4 * (y * ref_w + i) + 1];
            const uint8_t b = img_data[4 * (y * ref_w + i) + 2];

            const uint8_t diff_r = std::abs(r - ref_img[4 * (j * ref_w + i) + 0]);
            const uint8_t diff_g = std::abs(g - ref_img[4 * (j * ref_w + i) + 1]);
            const uint8_t diff_b = std::abs(b - ref_img[4 * (j * ref_w + i) + 2]);

            diff_data_u8[3 * (j * ref_w + i) + 0] = diff_r;
            diff_data_u8[3 * (j * ref_w + i) + 1] = diff_g;
            diff_data_u8[3 * (j * ref_w + i) + 2] = diff_b;

            mse += diff_r * diff_r;
            mse += diff_g * diff_g;
            mse += diff_b * diff_b;
        }
    }

    mse /= 3.0;
    mse /= (ref_w * ref_h);

    double psnr = -10.0 * std::log10(mse / (255.0 * 255.0));
    psnr = std::floor(psnr * 100.0) / 100.0;

    log_->Info("PSNR: %.2f/%.2f dB", psnr, viewer_->app_params.psnr);

    stbi_flip_vertically_on_write(flip_y);
    stbi_write_png(out_name.c_str(), viewer_->width, viewer_->height, 4, img_data, 4 * viewer_->width);
    stbi_flip_vertically_on_write(false);
    stbi_write_png(diff_name.c_str(), ref_w, ref_h, 3, diff_data_u8.get(), 3 * ref_w);

    return (psnr >= viewer_->app_params.psnr) ? 0 : -1;
}