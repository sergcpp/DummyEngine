#include "test_scene.h"

#include <chrono>

#include <stb/stb_image.h>
#include <stb/stb_image_write.h>
#if defined(REN_VK_BACKEND)
#include <Ren/VKCtx.h>
#elif defined(REN_GL_BACKEND)
#include <Ren/GLCtx.h>
#endif
#include <Snd/Context.h>
#include <Sys/AssetFile.h>
#include <Sys/Json.h>
#include <Sys/MemBuf.h>
#include <Sys/ScopeExit.h>
#include <Sys/ThreadPool.h>

#include "../renderer/Renderer.h"
#include "../scene/SceneManager.h"
#include "../utils/Random.h"
#include "../utils/ShaderLoader.h"

#include "test_common.h"

namespace RendererInternal {
extern const int TaaSampleCountStatic;
}

namespace {
const int LUT_DIMS = 48;
#include "__agx.inl"

std::mutex g_stbi_mutex;
} // namespace

extern std::string_view g_device_name;
extern int g_validation_level;
extern bool g_nohwrt, g_nosubgroup;

void run_image_test(Sys::ThreadPool &threads, std::string_view test_name, Ren::Span<const double> min_psnr,
                    const eImgTest img_test) {
    using namespace std::chrono;
    using namespace Eng;

    auto start_time = high_resolution_clock::now();

    const char *test_postfix = "";
    if (img_test == eImgTest::NoShadow) {
        test_postfix = "_noshadow";
    } else if (img_test == eImgTest::NoGI) {
        test_postfix = "_nogi";
    } else if (img_test == eImgTest::NoGI_RTShadow) {
        test_postfix = "_nogirt";
    } else if (img_test == eImgTest::NoDiffGI) {
        test_postfix = "_nodiffgi";
    } else if (img_test == eImgTest::NoDiffGI_RTShadow) {
        test_postfix = "_nodiffgirt";
    } else if (img_test == eImgTest::MedDiffGI) {
        test_postfix = "_meddiffgi";
    } else if (img_test == eImgTest::Full_Ultra) {
        test_postfix = "_ultra";
    }

    int ref_w = -1, ref_h = -1;
    { // Determine render resolution
        std::string ref_name =
            "assets_pc/references/" + std::string(test_name) + "/ref" + test_postfix + ".uncompressed.png";

        int ref_channels;
        uint8_t *ref_img = stbi_load(ref_name.c_str(), &ref_w, &ref_h, &ref_channels, 4);
        if (!ref_img) {
            ref_name = "assets_pc/references/" + std::string(test_name) + "/ref_0" + test_postfix + ".uncompressed.png";
            ref_img = stbi_load(ref_name.c_str(), &ref_w, &ref_h, &ref_channels, 4);
        }
        require_return(ref_img != nullptr);
        SCOPE_EXIT({ stbi_image_free(ref_img); })
    }
    require_return(ref_w != -1 && ref_h != -1);

    LogErr log;
    TestContext ren_ctx(ref_w, ref_h, g_device_name, g_validation_level, g_nohwrt, g_nosubgroup, &log);
#if defined(REN_VK_BACKEND)
    Ren::ApiContext *api_ctx = ren_ctx.api_ctx();
    require_return(g_device_name.empty() ||
                   Ren::MatchDeviceNames(api_ctx->device_properties.deviceName, g_device_name.data()));
#endif

    const bool skip_test = img_test != eImgTest::NoShadow && img_test != eImgTest::NoGI && !ren_ctx.capabilities.hwrt &&
                           !ren_ctx.capabilities.swrt;
    if (skip_test) {
        const std::string combined_test_name = std::string(test_name) + test_postfix;
        printf("Test %-36s ...skipped\n", combined_test_name.c_str());
        return;
    }

    ShaderLoader shader_loader(ren_ctx);
    Random rand(0);
    auto renderer = std::make_unique<Renderer>(ren_ctx, shader_loader, rand, threads);
    renderer->InitPipelines();

    renderer->settings.enable_motion_blur = false;
    renderer->settings.enable_bloom = false;
    renderer->settings.enable_shadow_jitter = true;
    renderer->settings.enable_aberration = false;
    renderer->settings.enable_purkinje = false;
    renderer->settings.enable_sharpen = false;
    renderer->settings.taa_mode = eTAAMode::Static;
    renderer->settings.pixel_filter = ePixelFilter::Box;
    renderer->settings.gi_cache_update_mode = eGICacheUpdateMode::Full;

    if (img_test == eImgTest::NoShadow) {
        renderer->settings.reflections_quality = eReflectionsQuality::Off;
        renderer->settings.shadows_quality = eShadowsQuality::Off;
        renderer->settings.gi_quality = eGIQuality::Off;
        renderer->settings.sky_quality = eSkyQuality::Medium;
    } else if (img_test == eImgTest::NoGI) {
        renderer->settings.reflections_quality = eReflectionsQuality::Off;
        renderer->settings.gi_quality = eGIQuality::Off;
        renderer->settings.sky_quality = eSkyQuality::Medium;
    } else if (img_test == eImgTest::NoGI_RTShadow) {
        renderer->settings.reflections_quality = eReflectionsQuality::Off;
        renderer->settings.shadows_quality = eShadowsQuality::Raytraced;
        renderer->settings.gi_quality = eGIQuality::Off;
        renderer->settings.sky_quality = eSkyQuality::Medium;
    } else if (img_test == eImgTest::NoDiffGI) {
        renderer->settings.gi_quality = eGIQuality::Off;
        renderer->settings.sky_quality = eSkyQuality::Medium;
    } else if (img_test == eImgTest::NoDiffGI_RTShadow) {
        renderer->settings.shadows_quality = eShadowsQuality::Raytraced;
        renderer->settings.gi_quality = eGIQuality::Off;
        renderer->settings.sky_quality = eSkyQuality::Medium;
    } else if (img_test == eImgTest::MedDiffGI) {
        renderer->settings.gi_quality = eGIQuality::Medium;
        renderer->settings.sky_quality = eSkyQuality::Medium;
    } else if (img_test == eImgTest::Full_Ultra) {
        renderer->settings.shadows_quality = eShadowsQuality::Raytraced;
        renderer->settings.reflections_quality = eReflectionsQuality::Raytraced_High;
        renderer->settings.ssao_quality = eSSAOQuality::Ultra;
        renderer->settings.gi_quality = eGIQuality::Ultra;
        renderer->settings.sky_quality = eSkyQuality::Ultra;
        renderer->settings.transparency_quality = eTransparencyQuality::Ultra;
        renderer->settings.vol_quality = eVolQuality::Ultra;
    }

    shader_loader.LoadPipelineCache("assets_pc/");

    path_config_t paths;
    SceneManager scene_manager(ren_ctx, shader_loader, nullptr, threads, paths);

    using namespace std::placeholders;
    scene_manager.SetPipelineInitializer(std::bind(&Renderer::InitPipelinesForProgram, renderer.get(), _1, _2, _3, _4));

    Sys::MultiPoolAllocator<char> alloc(32, 512);
    Sys::JsObjectP js_scene(alloc);

    { // Load scene data from file
        const std::string scene_name = "assets_pc/scenes/" + std::string(test_name) + ".json";
        Sys::AssetFile in_scene(scene_name);
        require_return(bool(in_scene));

        const size_t scene_size = in_scene.size();

        std::unique_ptr<uint8_t[]> scene_data(new uint8_t[scene_size]);
        in_scene.Read((char *)&scene_data[0], scene_size);

        Sys::MemBuf mem(&scene_data[0], scene_size);
        std::istream in_stream(&mem);

        require_return(js_scene.Read(in_stream));
    }

    Ren::Vec3d view_pos, view_dir;
    float view_fov = 45.0f, gamma = 1.0f, min_exposure = 0.0f, max_exposure = 0.0f;

    struct cam_frame_t {
        Ren::Vec3d pos, dir;
    };
    std::vector<cam_frame_t> cam_frames;

    if (const size_t camera_ndx = js_scene.IndexOf("camera"); camera_ndx < js_scene.Size()) {
        const Sys::JsObjectP &js_cam = js_scene[camera_ndx].second.as_obj();
        if (const size_t view_origin_ndx = js_cam.IndexOf("view_origin"); view_origin_ndx < js_cam.Size()) {
            const Sys::JsArrayP &js_orig = js_cam[view_origin_ndx].second.as_arr();
            view_pos[0] = js_orig.at(0).as_num().val;
            view_pos[1] = js_orig.at(1).as_num().val;
            view_pos[2] = js_orig.at(2).as_num().val;
        }
        if (const size_t view_dir_ndx = js_cam.IndexOf("view_dir"); view_dir_ndx < js_cam.Size()) {
            const Sys::JsArrayP &js_dir = js_cam[view_dir_ndx].second.as_arr();
            view_dir[0] = js_dir.at(0).as_num().val;
            view_dir[1] = js_dir.at(1).as_num().val;
            view_dir[2] = js_dir.at(2).as_num().val;
        }
        if (const size_t fov_ndx = js_cam.IndexOf("fov"); fov_ndx < js_cam.Size()) {
            const Sys::JsNumber &js_fov = js_cam[fov_ndx].second.as_num();
            view_fov = float(js_fov.val);
        }
        if (const size_t gamma_ndx = js_cam.IndexOf("gamma"); gamma_ndx < js_cam.Size()) {
            const Sys::JsNumber &js_gamma = js_cam[gamma_ndx].second.as_num();
            gamma = float(js_gamma.val);
        }
        if (const size_t min_exposure_ndx = js_cam.IndexOf("min_exposure"); min_exposure_ndx < js_cam.Size()) {
            const Sys::JsNumber &js_min_exposure = js_cam[min_exposure_ndx].second.as_num();
            min_exposure = float(js_min_exposure.val);
        }
        if (const size_t max_exposure_ndx = js_cam.IndexOf("max_exposure"); max_exposure_ndx < js_cam.Size()) {
            const Sys::JsNumber &js_max_exposure = js_cam[max_exposure_ndx].second.as_num();
            max_exposure = float(js_max_exposure.val);
        }
        if (const size_t view_transform_ndx = js_cam.IndexOf("view_transform"); view_transform_ndx < js_cam.Size()) {
            const Sys::JsStringP &js_view_transform = js_cam[view_transform_ndx].second.as_str();
            if (js_view_transform.val == "agx") {
                renderer->settings.tonemap_mode = eTonemapMode::LUT;
                renderer->SetTonemapLUT(LUT_DIMS, Ren::eTexFormat::RGB10_A2,
                                        Ren::Span<const uint8_t>(reinterpret_cast<const uint8_t *>(__agx),
                                                                 reinterpret_cast<const uint8_t *>(__agx) +
                                                                     4 * LUT_DIMS * LUT_DIMS * LUT_DIMS));
            }
        }

        if (const size_t paths_ndx = js_cam.IndexOf("paths"); paths_ndx < js_cam.Size()) {
            const Sys::JsArrayP &js_frames = js_cam[paths_ndx].second.as_arr().at(0).as_arr();
            for (const Sys::JsElementP &el : js_frames.elements) {
                const Sys::JsObjectP &js_frame = el.as_obj();

                const Sys::JsArrayP &js_frame_pos = js_frame.at("pos").as_arr();
                const Sys::JsArrayP &js_frame_rot = js_frame.at("rot").as_arr();

                auto rx = float(js_frame_rot.at(0).as_num().val);
                auto ry = float(js_frame_rot.at(1).as_num().val);
                auto rz = float(js_frame_rot.at(2).as_num().val);

                rx *= Ren::Pi<float>() / 180;
                ry *= Ren::Pi<float>() / 180;
                rz *= Ren::Pi<float>() / 180;

                Ren::Mat4f transform;
                transform = Rotate(transform, float(rz), Ren::Vec3f{0, 0, 1});
                transform = Rotate(transform, float(rx), Ren::Vec3f{1, 0, 0});
                transform = Rotate(transform, float(ry), Ren::Vec3f{0, 1, 0});

                auto view_vec = Ren::Vec4f{0, -1, 0, 0};
                view_vec = transform * view_vec;

                cam_frame_t &frame = cam_frames.emplace_back();
                frame.pos[0] = js_frame_pos.at(0).as_num().val;
                frame.pos[1] = js_frame_pos.at(1).as_num().val;
                frame.pos[2] = js_frame_pos.at(2).as_num().val;
                frame.dir = Ren::Vec3d(view_vec);
            }
        }
    }

    if (!cam_frames.empty()) {
        require_return(min_psnr.size() == cam_frames.size());
        // Switch to dynamic mode
        renderer->settings.taa_mode = eTAAMode::Dynamic;
        renderer->settings.enable_shadow_jitter = false;
        // Use the first frame for warmup
        view_pos = cam_frames[0].pos;
        view_dir = cam_frames[0].dir;
    }

    scene_manager.LoadScene(js_scene);
    { // test serialization
        Sys::JsObjectP js_scene_out(alloc);
        if (js_scene.Has("camera")) {
            js_scene_out["camera"] = js_scene["camera"];
        }
        scene_manager.SaveScene(js_scene_out);
        require(js_scene_out.Equals(js_scene, 0.001));
    }
    scene_manager.SetupView(view_pos, view_pos + view_dir, Ren::Vec3f{0.0f, 1.0f, 0.0f}, view_fov, Ren::Vec2f{0.0f},
                            gamma, min_exposure, max_exposure);

    //
    // Create required staging buffers
    //
    Ren::BufRef instance_indices_stage_buf = ren_ctx.LoadBuffer("Instance Indices (Upload)", Ren::eBufType::Upload,
                                                                InstanceIndicesBufChunkSize * Ren::MaxFramesInFlight);
    Ren::BufRef skin_transforms_stage_buf = ren_ctx.LoadBuffer("Skin Transforms (Upload)", Ren::eBufType::Upload,
                                                               SkinTransformsBufChunkSize * Ren::MaxFramesInFlight);
    Ren::BufRef shape_keys_stage_buf =
        ren_ctx.LoadBuffer("Shape Keys (Stage)", Ren::eBufType::Upload, ShapeKeysBufChunkSize * Ren::MaxFramesInFlight);
    Ren::BufRef cells_stage_buf =
        ren_ctx.LoadBuffer("Cells (Upload)", Ren::eBufType::Upload, CellsBufChunkSize * Ren::MaxFramesInFlight);
    Ren::BufRef rt_cells_stage_buf =
        ren_ctx.LoadBuffer("RT Cells (Upload)", Ren::eBufType::Upload, CellsBufChunkSize * Ren::MaxFramesInFlight);
    Ren::BufRef items_stage_buf =
        ren_ctx.LoadBuffer("Items (Upload)", Ren::eBufType::Upload, ItemsBufChunkSize * Ren::MaxFramesInFlight);
    Ren::BufRef rt_items_stage_buf =
        ren_ctx.LoadBuffer("RT Items (Upload)", Ren::eBufType::Upload, ItemsBufChunkSize * Ren::MaxFramesInFlight);
    Ren::BufRef lights_stage_buf =
        ren_ctx.LoadBuffer("Lights (Upload)", Ren::eBufType::Upload, LightsBufChunkSize * Ren::MaxFramesInFlight);
    Ren::BufRef decals_stage_buf =
        ren_ctx.LoadBuffer("Decals (Upload)", Ren::eBufType::Upload, DecalsBufChunkSize * Ren::MaxFramesInFlight);
    Ren::BufRef rt_geo_instances_stage_buf = ren_ctx.LoadBuffer("RT Geo Instances (Upload)", Ren::eBufType::Upload,
                                                                RTGeoInstancesBufChunkSize * Ren::MaxFramesInFlight);
    Ren::BufRef rt_sh_geo_instances_stage_buf = ren_ctx.LoadBuffer(
        "RT Shadow Geo Instances (Upload)", Ren::eBufType::Upload, RTGeoInstancesBufChunkSize * Ren::MaxFramesInFlight);
    Ren::BufRef rt_vol_geo_instances_stage_buf = ren_ctx.LoadBuffer(
        "RT Volume Geo Instances (Upload)", Ren::eBufType::Upload, RTGeoInstancesBufChunkSize * Ren::MaxFramesInFlight);
    Ren::BufRef rt_obj_instances_stage_buf, rt_sh_obj_instances_stage_buf, rt_vol_obj_instances_stage_buf,
        rt_tlas_nodes_stage_buf, rt_sh_tlas_nodes_stage_buf, rt_vol_tlas_nodes_stage_buf;
    if (ren_ctx.capabilities.hwrt) {
        rt_obj_instances_stage_buf = ren_ctx.LoadBuffer("RT Obj Instances (Upload)", Ren::eBufType::Upload,
                                                        HWRTObjInstancesBufChunkSize * Ren::MaxFramesInFlight);
        rt_sh_obj_instances_stage_buf = ren_ctx.LoadBuffer("RT Shadow Obj Instances (Upload)", Ren::eBufType::Upload,
                                                           HWRTObjInstancesBufChunkSize * Ren::MaxFramesInFlight);
        rt_vol_obj_instances_stage_buf = ren_ctx.LoadBuffer("RT Volume Obj Instances (Upload)", Ren::eBufType::Upload,
                                                            HWRTObjInstancesBufChunkSize * Ren::MaxFramesInFlight);
    } else if (ren_ctx.capabilities.swrt) {
        rt_obj_instances_stage_buf = ren_ctx.LoadBuffer("RT Obj Instances (Upload)", Ren::eBufType::Upload,
                                                        SWRTObjInstancesBufChunkSize * Ren::MaxFramesInFlight);
        rt_sh_obj_instances_stage_buf = ren_ctx.LoadBuffer("RT Shadow Obj Instances (Upload)", Ren::eBufType::Upload,
                                                           SWRTObjInstancesBufChunkSize * Ren::MaxFramesInFlight);
        rt_vol_obj_instances_stage_buf = ren_ctx.LoadBuffer("RT Volume Obj Instances (Upload)", Ren::eBufType::Upload,
                                                            SWRTObjInstancesBufChunkSize * Ren::MaxFramesInFlight);
        rt_tlas_nodes_stage_buf = ren_ctx.LoadBuffer("SWRT TLAS Nodes (Upload)", Ren::eBufType::Upload,
                                                     SWRTTLASNodesBufChunkSize * Ren::MaxFramesInFlight);
        rt_sh_tlas_nodes_stage_buf = ren_ctx.LoadBuffer("SWRT Shadow TLAS Nodes (Upload)", Ren::eBufType::Upload,
                                                        SWRTTLASNodesBufChunkSize * Ren::MaxFramesInFlight);
        rt_vol_tlas_nodes_stage_buf = ren_ctx.LoadBuffer("SWRT Volume TLAS Nodes (Upload)", Ren::eBufType::Upload,
                                                         SWRTTLASNodesBufChunkSize * Ren::MaxFramesInFlight);
    }

    Ren::BufRef shared_data_stage_buf =
        ren_ctx.LoadBuffer("Shared Data (Upload)", Ren::eBufType::Upload, SharedDataBlockSize * Ren::MaxFramesInFlight);

    //
    // Initialize draw list
    //
    DrawList draw_list;
    draw_list.Init(shared_data_stage_buf, instance_indices_stage_buf, skin_transforms_stage_buf, shape_keys_stage_buf,
                   cells_stage_buf, rt_cells_stage_buf, items_stage_buf, rt_items_stage_buf, lights_stage_buf,
                   decals_stage_buf, rt_geo_instances_stage_buf, rt_sh_geo_instances_stage_buf,
                   rt_vol_geo_instances_stage_buf, rt_obj_instances_stage_buf, rt_sh_obj_instances_stage_buf,
                   rt_vol_obj_instances_stage_buf, rt_tlas_nodes_stage_buf, rt_sh_tlas_nodes_stage_buf,
                   rt_vol_tlas_nodes_stage_buf);
    draw_list.render_settings = renderer->settings;

    renderer->PrepareDrawList(scene_manager.scene_data(), scene_manager.main_cam(), scene_manager.ext_cam(), draw_list);
    scene_manager.UpdateTexturePriorities(draw_list.visible_textures, draw_list.desired_textures);

    //
    // Render image
    //

    // NOTE: Temporarily placed here
    std::lock_guard<std::mutex> _(g_stbi_mutex);

    Ren::TexParams params;
    params.w = ref_w;
    params.h = ref_h;
#if defined(REN_GL_BACKEND)
    params.format = Ren::eTexFormat::RGBA8_srgb;
#else
    params.format = Ren::eTexFormat::RGBA8;
#endif
    params.sampling.filter = Ren::eTexFilter::Bilinear;
    params.sampling.wrap = Ren::eTexWrap::ClampToEdge;
    params.usage = Ren::Bitmask(Ren::eTexUsage::RenderTarget) | Ren::eTexUsage::Transfer;

    Ren::eTexLoadStatus status;
    Ren::TexRef render_result = ren_ctx.LoadTexture("Render Result", params, ren_ctx.default_mem_allocs(), &status);

    auto begin_frame = [&ren_ctx]() {
        Ren::ApiContext *api_ctx = ren_ctx.api_ctx();

#if defined(REN_VK_BACKEND)
        api_ctx->vkWaitForFences(api_ctx->device, 1, &api_ctx->in_flight_fences[api_ctx->backend_frame], VK_TRUE,
                                 UINT64_MAX);
        api_ctx->vkResetFences(api_ctx->device, 1, &api_ctx->in_flight_fences[api_ctx->backend_frame]);

        ReadbackTimestampQueries(api_ctx, api_ctx->backend_frame);
        DestroyDeferredResources(api_ctx, api_ctx->backend_frame);

        const bool reset_result = ren_ctx.default_descr_alloc()->Reset();
        require_fatal(reset_result);

        ///////////////////////////////////////////////////////////////

        VkCommandBufferBeginInfo begin_info = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        api_ctx->vkBeginCommandBuffer(api_ctx->draw_cmd_buf[api_ctx->backend_frame], &begin_info);
        api_ctx->curr_cmd_buf = api_ctx->draw_cmd_buf[api_ctx->backend_frame];

        api_ctx->vkCmdResetQueryPool(api_ctx->draw_cmd_buf[api_ctx->backend_frame],
                                     api_ctx->query_pools[api_ctx->backend_frame], 0, Ren::MaxTimestampQueries);
#elif defined(REN_GL_BACKEND)
        // Make sure all operations have finished
        api_ctx->in_flight_fences[api_ctx->backend_frame].ClientWaitSync();
        api_ctx->in_flight_fences[api_ctx->backend_frame] = {};

        ReadbackTimestampQueries(api_ctx, api_ctx->backend_frame);
#endif
    };

    auto end_frame = [&ren_ctx]() {
        Ren::ApiContext *api_ctx = ren_ctx.api_ctx();

#if defined(REN_VK_BACKEND)
        api_ctx->vkEndCommandBuffer(api_ctx->draw_cmd_buf[api_ctx->backend_frame]);

        const int prev_frame = (api_ctx->backend_frame + Ren::MaxFramesInFlight - 1) % Ren::MaxFramesInFlight;

        //////////////////////////////////////////////////////////////////////////////////////////////////

        VkSubmitInfo submit_info = {VK_STRUCTURE_TYPE_SUBMIT_INFO};

        const VkSemaphore wait_semaphores[] = {api_ctx->render_finished_semaphores[prev_frame]};
        const VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_ALL_COMMANDS_BIT};

        if (api_ctx->render_finished_semaphore_is_set[prev_frame]) {
            submit_info.waitSemaphoreCount = 1;
            submit_info.pWaitSemaphores = wait_semaphores;
            submit_info.pWaitDstStageMask = wait_stages;
        }

        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &api_ctx->draw_cmd_buf[api_ctx->backend_frame];

        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores = &api_ctx->render_finished_semaphores[api_ctx->backend_frame];

        VkResult res = api_ctx->vkQueueSubmit(api_ctx->graphics_queue, 1, &submit_info,
                                              api_ctx->in_flight_fences[api_ctx->backend_frame]);
        require_fatal(res == VK_SUCCESS);

        api_ctx->render_finished_semaphore_is_set[api_ctx->backend_frame] = true;
        api_ctx->render_finished_semaphore_is_set[prev_frame] = false;

#elif defined(REN_GL_BACKEND)
        api_ctx->in_flight_fences[api_ctx->backend_frame] = Ren::MakeFence();
#endif
        api_ctx->backend_frame = (api_ctx->backend_frame + 1) % Ren::MaxFramesInFlight;
    };

    auto validate_frame = [&](int frame_index = -1) {
        Ren::BufRef stage_buf = ren_ctx.LoadBuffer("Temp readback buf", Ren::eBufType::Readback, 4 * ref_w * ref_h);

        { // Download result
            Ren::CommandBuffer cmd_buf = ren_ctx.BegTempSingleTimeCommands();
            render_result->CopyTextureData(*stage_buf, cmd_buf, 0, 4 * ref_w * ref_h);
            ren_ctx.InsertReadbackMemoryBarrier(cmd_buf);
            ren_ctx.EndTempSingleTimeCommands(cmd_buf);
        }

        std::unique_ptr<uint8_t[]> diff_data_u8(new uint8_t[ref_w * ref_h * 3]);

        const uint8_t *img_data = stage_buf->Map();
        SCOPE_EXIT({ stage_buf->Unmap(); })

        double mse = 0.0;

        const bool flip_y =
#if defined(REN_GL_BACKEND)
            true;
#else
            false;
#endif

        const std::string index_str = (frame_index != -1 ? "_" + std::to_string(frame_index) : "");
        const std::string ref_name =
            "assets_pc/references/" + std::string(test_name) + "/ref" + index_str + test_postfix + ".uncompressed.png";

        int _ref_w, _ref_h, _ref_channels;
        uint8_t *ref_img = stbi_load(ref_name.c_str(), &_ref_w, &_ref_h, &_ref_channels, 4);
        SCOPE_EXIT({ stbi_image_free(ref_img); })

        assert(_ref_w == ref_w);
        assert(_ref_h == ref_h);

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

        const double test_duration_ms = duration<double>(high_resolution_clock::now() - start_time).count() * 1000.0;

        // std::lock_guard<std::mutex> _(g_stbi_mutex);
        const std::string combined_test_name = std::string(test_name) + index_str + test_postfix;
        printf("Test %-36s (PSNR: %.2f/%.2f dB, Time: %.2fms)\n", combined_test_name.c_str(), psnr,
               min_psnr[std::max(frame_index, 0)], test_duration_ms);
        fflush(stdout);
        require(psnr >= min_psnr[std::max(frame_index, 0)]);

        const std::string out_name =
            "assets_pc/references/" + std::string(test_name) + "/out" + index_str + test_postfix + ".png";
        const std::string diff_name =
            "assets_pc/references/" + std::string(test_name) + "/diff" + index_str + test_postfix + ".png";

        stbi_flip_vertically_on_write(flip_y);
        stbi_write_png(out_name.c_str(), ref_w, ref_h, 4, img_data, 4 * ref_w);
        stbi_flip_vertically_on_write(false);
        stbi_write_png(diff_name.c_str(), ref_w, ref_h, 3, diff_data_u8.get(), 3 * ref_w);
    };

    // Make sure all textures are loaded
    bool finished = false;
    for (int i = 0; i < 0 || !finished; ++i) {
        /*draw_list.Clear();
        renderer.PrepareDrawList(scene_manager.scene_data(), scene_manager.main_cam(), scene_manager.ext_cam(),
                                 draw_list);*/

        begin_frame();
        finished = scene_manager.Serve(1);
        // renderer.ExecuteDrawList(draw_list, scene_manager.persistent_data(), render_result);
        end_frame();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    draw_list.frame_index = 0;
    // renderer->settings.taa_mode = eTAAMode::Static;
    // draw_list.render_settings = renderer->settings;
    renderer->reset_accumulation();

    //
    // Warmup (fill diffuse/specular history buffers)
    //
    for (int i = 0; i < 64; ++i) {
        draw_list.Clear();
        renderer->PrepareDrawList(scene_manager.scene_data(), scene_manager.main_cam(), scene_manager.ext_cam(),
                                  draw_list);

        begin_frame();
        renderer->ExecuteDrawList(draw_list, scene_manager.persistent_data(), render_result);
        end_frame();
    }

    draw_list.frame_index = 0;
    renderer->reset_accumulation();

    //
    // Main capture
    //
    if (cam_frames.empty()) {
        // Static accumulation
        for (int i = 0; i < RendererInternal::TaaSampleCountStatic; ++i) {
            draw_list.Clear();
            renderer->PrepareDrawList(scene_manager.scene_data(), scene_manager.main_cam(), scene_manager.ext_cam(),
                                      draw_list);

            begin_frame();
            renderer->ExecuteDrawList(draw_list, scene_manager.persistent_data(), render_result);
            end_frame();
        }
        validate_frame();
    } else {
        // Dynamic accumulation
        for (int i = 0; i < int(cam_frames.size()); ++i) {
            const auto &frame = cam_frames[i];
            scene_manager.SetupView(frame.pos, frame.pos + frame.dir, Ren::Vec3f{0.0f, 1.0f, 0.0f}, view_fov,
                                    Ren::Vec2f{0.0f}, gamma, min_exposure, max_exposure);

            draw_list.Clear();
            renderer->PrepareDrawList(scene_manager.scene_data(), scene_manager.main_cam(), scene_manager.ext_cam(),
                                      draw_list);

            begin_frame();
            renderer->ExecuteDrawList(draw_list, scene_manager.persistent_data(), render_result);
            end_frame();

            validate_frame(i);
            start_time = high_resolution_clock::now();
        }
    }

    shader_loader.WritePipelineCache("assets_pc/");
}

void test_empty_scene(Sys::ThreadPool &threads) { run_image_test(threads, "empty", INFINITY, Full); }