#include "test_common.h"

#include <chrono>
#include <future>
#include <istream>

#include <stb/stb_image.h>
#include <stb/stb_image_write.h>
#if defined(USE_VK_RENDER)
#include <Ren/VKCtx.h>
#elif defined(USE_GL_RENDER)
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

namespace RendererInternal {
extern const int TaaSampleCountStatic;
}

namespace {
const int LUT_DIMS = 48;
#include "__agx.inl"

std::string_view g_device_name;
int g_validation_level = 0;
bool g_nohwrt = false;

std::mutex g_stbi_mutex;
} // namespace

enum eImgTest { NoShadow, NoGI, NoGI_RTShadow, NoDiffGI, NoDiffGI_RTShadow, MedDiffGI, Full, Full_Ultra };

void run_image_test(std::string_view test_name, const double min_psnr, const eImgTest img_test = eImgTest::NoShadow) {
    using namespace std::chrono;

    const auto start_time = high_resolution_clock::now();

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
    const std::string ref_name =
        "assets_pc/references/" + std::string(test_name) + "/ref" + test_postfix + ".uncompressed.png";

    int ref_w, ref_h, ref_channels;
    uint8_t *ref_img = stbi_load(ref_name.c_str(), &ref_w, &ref_h, &ref_channels, 4);
    SCOPE_EXIT({ stbi_image_free(ref_img); })

    LogErr log;
    TestContext ren_ctx(ref_w, ref_h, g_device_name, g_validation_level, g_nohwrt, &log);
#if defined(USE_VK_RENDER)
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

    Eng::ShaderLoader shader_loader;
    Eng::Random rand(0);
    Sys::ThreadPool threads(1);
    Eng::Renderer renderer(ren_ctx, shader_loader, rand, threads);

    renderer.settings.enable_bloom = false;
    renderer.settings.enable_shadow_jitter = true;
    renderer.settings.taa_mode = Eng::eTAAMode::Static;
    renderer.settings.pixel_filter = Eng::ePixelFilter::Box;

    if (img_test == eImgTest::NoShadow) {
        renderer.settings.reflections_quality = Eng::eReflectionsQuality::Off;
        renderer.settings.shadows_quality = Eng::eShadowsQuality::Off;
        renderer.settings.gi_quality = Eng::eGIQuality::Off;
        renderer.settings.sky_quality = Eng::eSkyQuality::Medium;
    } else if (img_test == eImgTest::NoGI) {
        renderer.settings.reflections_quality = Eng::eReflectionsQuality::Off;
        renderer.settings.gi_quality = Eng::eGIQuality::Off;
        renderer.settings.sky_quality = Eng::eSkyQuality::Medium;
    } else if (img_test == eImgTest::NoGI_RTShadow) {
        renderer.settings.reflections_quality = Eng::eReflectionsQuality::Off;
        renderer.settings.shadows_quality = Eng::eShadowsQuality::Raytraced;
        renderer.settings.gi_quality = Eng::eGIQuality::Off;
        renderer.settings.sky_quality = Eng::eSkyQuality::Medium;
    } else if (img_test == eImgTest::NoDiffGI) {
        renderer.settings.gi_quality = Eng::eGIQuality::Off;
        renderer.settings.sky_quality = Eng::eSkyQuality::Medium;
    } else if (img_test == eImgTest::NoDiffGI_RTShadow) {
        renderer.settings.shadows_quality = Eng::eShadowsQuality::Raytraced;
        renderer.settings.gi_quality = Eng::eGIQuality::Off;
        renderer.settings.sky_quality = Eng::eSkyQuality::Medium;
    } else if (img_test == eImgTest::MedDiffGI) {
        renderer.settings.gi_quality = Eng::eGIQuality::Medium;
        renderer.settings.sky_quality = Eng::eSkyQuality::Medium;
    } else if (img_test == eImgTest::Full_Ultra) {
        renderer.settings.shadows_quality = Eng::eShadowsQuality::Raytraced;
        renderer.settings.reflections_quality = Eng::eReflectionsQuality::Raytraced_High;
        renderer.settings.gi_quality = Eng::eGIQuality::Ultra;
        renderer.settings.sky_quality = Eng::eSkyQuality::Ultra;
    }

    Eng::path_config_t paths;
    Eng::SceneManager scene_manager(ren_ctx, shader_loader, nullptr, threads, paths);

    using namespace std::placeholders;
    scene_manager.SetPipelineInitializer(std::bind(&Eng::Renderer::InitPipelinesForProgram, &renderer, _1, _2, _3, _4));

    Sys::MultiPoolAllocator<char> alloc(32, 512);
    JsObjectP js_scene(alloc);

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

    Ren::Vec3f view_pos, view_dir;
    float view_fov = 45.0f, gamma = 1.0f, min_exposure = 0.0f, max_exposure = 0.0f;

    if (js_scene.Has("camera")) {
        const JsObjectP &js_cam = js_scene.at("camera").as_obj();
        if (js_cam.Has("view_origin")) {
            const JsArrayP &js_orig = js_cam.at("view_origin").as_arr();
            view_pos[0] = float(js_orig.at(0).as_num().val);
            view_pos[1] = float(js_orig.at(1).as_num().val);
            view_pos[2] = float(js_orig.at(2).as_num().val);
        }

        if (js_cam.Has("view_dir")) {
            const JsArrayP &js_dir = js_cam.at("view_dir").as_arr();
            view_dir[0] = float(js_dir.at(0).as_num().val);
            view_dir[1] = float(js_dir.at(1).as_num().val);
            view_dir[2] = float(js_dir.at(2).as_num().val);
        }

        if (js_cam.Has("fov")) {
            const JsNumber &js_fov = js_cam.at("fov").as_num();
            view_fov = float(js_fov.val);
        }

        if (js_cam.Has("gamma")) {
            const JsNumber &js_gamma = js_cam.at("gamma").as_num();
            gamma = float(js_gamma.val);
        }

        if (js_cam.Has("min_exposure")) {
            const JsNumber &js_min_exposure = js_cam.at("min_exposure").as_num();
            min_exposure = float(js_min_exposure.val);
        }

        if (js_cam.Has("max_exposure")) {
            const JsNumber &js_max_exposure = js_cam.at("max_exposure").as_num();
            max_exposure = float(js_max_exposure.val);
        }

        if (js_cam.Has("view_transform")) {
            const JsStringP &js_view_transform = js_cam.at("view_transform").as_str();
            if (js_view_transform.val == "agx") {
                renderer.settings.tonemap_mode = Eng::eTonemapMode::LUT;
                renderer.SetTonemapLUT(LUT_DIMS, Ren::eTexFormat::RawRGB10_A2,
                                       Ren::Span<const uint8_t>(reinterpret_cast<const uint8_t *>(__agx),
                                                                reinterpret_cast<const uint8_t *>(__agx) +
                                                                    4 * LUT_DIMS * LUT_DIMS * LUT_DIMS));
            }
        }
    }

    scene_manager.LoadScene(js_scene);
    { // test serialization
        JsObjectP js_scene_out(alloc);
        if (js_scene.Has("camera")) {
            js_scene_out["camera"] = js_scene["camera"];
        }
        scene_manager.SaveScene(js_scene_out);
        require(js_scene_out.Equals(js_scene, 0.001));
    }
    scene_manager.SetupView(view_pos, view_pos + view_dir, Ren::Vec3f{0.0f, 1.0f, 0.0f}, view_fov, gamma, min_exposure,
                            max_exposure);

    //
    // Create required staging buffers
    //
    Ren::BufferRef instance_indices_stage_buf = ren_ctx.LoadBuffer(
        "Instance Indices (Upload)", Ren::eBufType::Upload, Eng::InstanceIndicesBufChunkSize * Ren::MaxFramesInFlight);
    Ren::BufferRef skin_transforms_stage_buf = ren_ctx.LoadBuffer(
        "Skin Transforms (Upload)", Ren::eBufType::Upload, Eng::SkinTransformsBufChunkSize * Ren::MaxFramesInFlight);
    Ren::BufferRef shape_keys_stage_buf = ren_ctx.LoadBuffer("Shape Keys (Stage)", Ren::eBufType::Upload,
                                                             Eng::ShapeKeysBufChunkSize * Ren::MaxFramesInFlight);
    Ren::BufferRef cells_stage_buf =
        ren_ctx.LoadBuffer("Cells (Upload)", Ren::eBufType::Upload, Eng::CellsBufChunkSize * Ren::MaxFramesInFlight);
    Ren::BufferRef rt_cells_stage_buf =
        ren_ctx.LoadBuffer("RT Cells (Upload)", Ren::eBufType::Upload, Eng::CellsBufChunkSize * Ren::MaxFramesInFlight);
    Ren::BufferRef items_stage_buf =
        ren_ctx.LoadBuffer("Items (Upload)", Ren::eBufType::Upload, Eng::ItemsBufChunkSize * Ren::MaxFramesInFlight);
    Ren::BufferRef rt_items_stage_buf =
        ren_ctx.LoadBuffer("RT Items (Upload)", Ren::eBufType::Upload, Eng::ItemsBufChunkSize * Ren::MaxFramesInFlight);
    Ren::BufferRef lights_stage_buf =
        ren_ctx.LoadBuffer("Lights (Upload)", Ren::eBufType::Upload, Eng::LightsBufChunkSize * Ren::MaxFramesInFlight);
    Ren::BufferRef decals_stage_buf =
        ren_ctx.LoadBuffer("Decals (Upload)", Ren::eBufType::Upload, Eng::DecalsBufChunkSize * Ren::MaxFramesInFlight);
    Ren::BufferRef rt_geo_instances_stage_buf = ren_ctx.LoadBuffer(
        "RT Geo Instances (Upload)", Ren::eBufType::Upload, Eng::RTGeoInstancesBufChunkSize * Ren::MaxFramesInFlight);
    Ren::BufferRef rt_sh_geo_instances_stage_buf =
        ren_ctx.LoadBuffer("RT Shadow Geo Instances (Upload)", Ren::eBufType::Upload,
                           Eng::RTGeoInstancesBufChunkSize * Ren::MaxFramesInFlight);
    Ren::BufferRef rt_obj_instances_stage_buf, rt_sh_obj_instances_stage_buf, rt_tlas_nodes_stage_buf,
        rt_sh_tlas_nodes_stage_buf;
    if (ren_ctx.capabilities.hwrt) {
        rt_obj_instances_stage_buf = ren_ctx.LoadBuffer("RT Obj Instances (Upload)", Ren::eBufType::Upload,
                                                        Eng::HWRTObjInstancesBufChunkSize * Ren::MaxFramesInFlight);
        rt_sh_obj_instances_stage_buf = ren_ctx.LoadBuffer("RT Shadow Obj Instances (Upload)", Ren::eBufType::Upload,
                                                           Eng::HWRTObjInstancesBufChunkSize * Ren::MaxFramesInFlight);
    } else if (ren_ctx.capabilities.swrt) {
        rt_obj_instances_stage_buf = ren_ctx.LoadBuffer("RT Obj Instances (Upload)", Ren::eBufType::Upload,
                                                        Eng::SWRTObjInstancesBufChunkSize * Ren::MaxFramesInFlight);
        rt_sh_obj_instances_stage_buf = ren_ctx.LoadBuffer("RT Shadow Obj Instances (Upload)", Ren::eBufType::Upload,
                                                           Eng::SWRTObjInstancesBufChunkSize * Ren::MaxFramesInFlight);
        rt_tlas_nodes_stage_buf = ren_ctx.LoadBuffer("SWRT TLAS Nodes (Upload)", Ren::eBufType::Upload,
                                                     Eng::SWRTTLASNodesBufChunkSize * Ren::MaxFramesInFlight);
        rt_sh_tlas_nodes_stage_buf = ren_ctx.LoadBuffer("SWRT Shadow TLAS Nodes (Upload)", Ren::eBufType::Upload,
                                                        Eng::SWRTTLASNodesBufChunkSize * Ren::MaxFramesInFlight);
    }

    Ren::BufferRef shared_data_stage_buf = ren_ctx.LoadBuffer("Shared Data (Upload)", Ren::eBufType::Upload,
                                                              Eng::SharedDataBlockSize * Ren::MaxFramesInFlight);

    //
    // Initialize draw list
    //
    Eng::DrawList draw_list;
    draw_list.Init(shared_data_stage_buf, instance_indices_stage_buf, skin_transforms_stage_buf, shape_keys_stage_buf,
                   cells_stage_buf, rt_cells_stage_buf, items_stage_buf, rt_items_stage_buf, lights_stage_buf,
                   decals_stage_buf, rt_geo_instances_stage_buf, rt_sh_geo_instances_stage_buf,
                   rt_obj_instances_stage_buf, rt_sh_obj_instances_stage_buf, rt_tlas_nodes_stage_buf,
                   rt_sh_tlas_nodes_stage_buf);
    draw_list.render_settings = renderer.settings;

    renderer.PrepareDrawList(scene_manager.scene_data(), scene_manager.main_cam(), scene_manager.ext_cam(), draw_list);
    scene_manager.UpdateTexturePriorities(draw_list.visible_textures, draw_list.desired_textures);

    //
    // Render image
    //

    // NOTE: Temporarily placed here
    std::lock_guard<std::mutex> _(g_stbi_mutex);

    Ren::Tex2DParams params;
    params.w = ref_w;
    params.h = ref_h;
    params.format = Ren::eTexFormat::RawRGBA8888;
    params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
    params.sampling.wrap = Ren::eTexWrap::ClampToEdge;
    params.usage = Ren::eTexUsage::RenderTarget | Ren::eTexUsage::Transfer;
#if defined(USE_GL_RENDER)
    params.flags = Ren::eTexFlagBits::SRGB;
#endif

    Ren::eTexLoadStatus status;
    Ren::Tex2DRef render_result = ren_ctx.LoadTexture2D("Render Result", params, ren_ctx.default_mem_allocs(), &status);

    auto begin_frame = [&ren_ctx]() {
        Ren::ApiContext *api_ctx = ren_ctx.api_ctx();

#if defined(USE_VK_RENDER)
        api_ctx->vkWaitForFences(api_ctx->device, 1, &api_ctx->in_flight_fences[api_ctx->backend_frame], VK_TRUE,
                                 UINT64_MAX);
        api_ctx->vkResetFences(api_ctx->device, 1, &api_ctx->in_flight_fences[api_ctx->backend_frame]);

        Ren::ReadbackTimestampQueries(api_ctx, api_ctx->backend_frame);
        Ren::DestroyDeferredResources(api_ctx, api_ctx->backend_frame);

        const bool reset_result = ren_ctx.default_descr_alloc()->Reset();
        require_fatal(reset_result);

        ///////////////////////////////////////////////////////////////

        VkCommandBufferBeginInfo begin_info = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        api_ctx->vkBeginCommandBuffer(api_ctx->draw_cmd_buf[api_ctx->backend_frame], &begin_info);

        api_ctx->vkCmdResetQueryPool(api_ctx->draw_cmd_buf[api_ctx->backend_frame],
                                     api_ctx->query_pools[api_ctx->backend_frame], 0, Ren::MaxTimestampQueries);
#elif defined(USE_GL_RENDER)
        // Make sure all operations have finished
        api_ctx->in_flight_fences[api_ctx->backend_frame].ClientWaitSync();
        api_ctx->in_flight_fences[api_ctx->backend_frame] = {};

        Ren::ReadbackTimestampQueries(api_ctx, api_ctx->backend_frame);
#endif
    };

    auto end_frame = [&ren_ctx]() {
        Ren::ApiContext *api_ctx = ren_ctx.api_ctx();

#if defined(USE_VK_RENDER)
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

#elif defined(USE_GL_RENDER)
        api_ctx->in_flight_fences[api_ctx->backend_frame] = Ren::MakeFence();
#endif
        api_ctx->backend_frame = (api_ctx->backend_frame + 1) % Ren::MaxFramesInFlight;
    };

    // renderer.settings.taa_mode = Eng::eTAAMode::Dynamic;

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
    // renderer.settings.taa_mode = Eng::eTAAMode::Static;
    // draw_list.render_settings = renderer.settings;
    renderer.reset_accumulation();

    //
    // Warmup (fill diffuse/specular history buffers)
    //
    for (int i = 0; i < 64; ++i) {
        draw_list.Clear();
        renderer.PrepareDrawList(scene_manager.scene_data(), scene_manager.main_cam(), scene_manager.ext_cam(),
                                 draw_list);

        begin_frame();
        renderer.ExecuteDrawList(draw_list, scene_manager.persistent_data(), render_result);
        end_frame();
    }

    draw_list.frame_index = 0;
    renderer.reset_accumulation();

    //
    // Main capture
    //
    for (int i = 0; i < RendererInternal::TaaSampleCountStatic; ++i) {
        draw_list.Clear();
        renderer.PrepareDrawList(scene_manager.scene_data(), scene_manager.main_cam(), scene_manager.ext_cam(),
                                 draw_list);

        begin_frame();
        renderer.ExecuteDrawList(draw_list, scene_manager.persistent_data(), render_result);
        end_frame();
    }

    Ren::BufferRef stage_buf = ren_ctx.LoadBuffer("Temp readback buf", Ren::eBufType::Readback, 4 * ref_w * ref_h);

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
#if defined(USE_GL_RENDER)
        true;
#else
        false;
#endif

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
    const std::string combined_test_name = std::string(test_name) + test_postfix;
    printf("Test %-36s (PSNR: %.2f/%.2f dB, Time: %.2fms)\n", combined_test_name.c_str(), psnr, min_psnr,
           test_duration_ms);
    fflush(stdout);
    require(psnr >= min_psnr);

    const std::string out_name = "assets_pc/references/" + std::string(test_name) + "/out" + test_postfix + ".png";
    const std::string diff_name = "assets_pc/references/" + std::string(test_name) + "/diff" + test_postfix + ".png";

    stbi_flip_vertically_on_write(flip_y);
    stbi_write_png(out_name.c_str(), ref_w, ref_h, 4, img_data, 4 * ref_w);
    stbi_flip_vertically_on_write(false);
    stbi_write_png(diff_name.c_str(), ref_w, ref_h, 3, diff_data_u8.get(), 3 * ref_w);
}

void test_materials(Sys::ThreadPool &threads, const bool full, std::string_view device_name, const int vl,
                    const bool nohwrt) {
    g_device_name = device_name;
    g_validation_level = vl;
    g_nohwrt = nohwrt;

    { // complex materials
        std::vector<std::future<void>> futures;

        futures.push_back(threads.Enqueue(run_image_test, "visibility_flags", 24.60, Full));
        futures.push_back(threads.Enqueue(run_image_test, "visibility_flags", 24.90, Full_Ultra));
        futures.push_back(threads.Enqueue(run_image_test, "two_sided_mat", 35.95, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "two_sided_mat", 29.00, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "two_sided_mat", 28.50, NoDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "two_sided_mat", 27.80, MedDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "two_sided_mat", 27.15, Full));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat0", 34.40, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat0", 32.10, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat0", 27.80, NoDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat0", 26.80, MedDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat0", 25.65, Full));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat0", 25.95, Full_Ultra));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat1", 35.20, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat1", 32.75, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat1", 30.20, NoDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat1", 29.75, MedDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat1", 28.15, Full));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat1", 28.80, Full_Ultra));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat2", 33.90, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat2", 31.90, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat2", 26.95, NoDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat2", 25.05, MedDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat2", 24.80, Full));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat2", 27.45, Full_Ultra));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat2_spot_light", 34.55, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat2_spot_light", 35.85, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat2_spot_light", 29.95, NoDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat2_spot_light", 26.70, MedDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat2_spot_light", 26.55, Full));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat2_spot_light", 27.80, Full_Ultra));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat2_sun_light", 33.50, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat2_sun_light", 29.20, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat2_sun_light", 33.40, NoGI_RTShadow));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat2_sun_light", 21.30, Full));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat2_sun_light", 22.85, Full_Ultra));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat2_moon_light", 21.75, MedDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat2_moon_light", 19.50, Full));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat2_moon_light", 19.80, Full_Ultra));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat2_hdri_light", 20.50, MedDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat2_hdri_light", 22.30, Full));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat2_hdri_light", 23.55, Full_Ultra));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat2_portal_hdri", 28.65, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat2_portal_hdri", 24.95, NoDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat2_portal_hdri", 21.85, MedDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat2_portal_hdri", 23.90, Full));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat2_portal_hdri", 24.55, Full_Ultra));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat2_portal_sky", 18.75, MedDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat2_portal_sky", 18.35, Full));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat2_portal_sky", 18.45, Full_Ultra));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat2_mesh_lights", 20.85, MedDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat2_mesh_lights", 21.20, Full));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat2_mesh_lights", 22.30, Full_Ultra));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat3", 24.25, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat3", 21.60, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat3", 22.90, NoDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat3", 22.00, MedDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat3", 22.20, Full));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat3", 22.65, Full_Ultra));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat3_sun_light", 21.80, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat3_sun_light", 17.65, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat3_sun_light", 22.70, NoGI_RTShadow));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat3_sun_light", 18.70, Full));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat3_sun_light", 21.90, Full_Ultra));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat3_mesh_lights", 17.05, MedDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat3_mesh_lights", 20.70, Full));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat3_mesh_lights", 20.70, Full_Ultra));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat4", 19.75, Full));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat4", 19.75, Full_Ultra));
        futures.push_back(threads.Enqueue(run_image_test, "emit_mat0", 23.34, Full));
        futures.push_back(threads.Enqueue(run_image_test, "emit_mat0", 23.30, Full_Ultra));
        futures.push_back(threads.Enqueue(run_image_test, "emit_mat1", 22.09, Full));
        futures.push_back(threads.Enqueue(run_image_test, "emit_mat1", 22.38, Full_Ultra));

        for (auto &f : futures) {
            f.wait();
        }
    }

    if (!full) {
        return;
    }

    puts(" ---------------");
    /*if (g_tests_success)*/ { // diffuse material
        std::vector<std::future<void>> futures;

        futures.push_back(threads.Enqueue(run_image_test, "diff_mat0", 47.20, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "diff_mat0", 34.65, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "diff_mat0", 32.10, MedDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "diff_mat0", 28.30, Full));
        futures.push_back(threads.Enqueue(run_image_test, "diff_mat1", 46.70, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "diff_mat1", 34.15, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "diff_mat1", 30.60, MedDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "diff_mat1", 26.45, Full));
        futures.push_back(threads.Enqueue(run_image_test, "diff_mat2", 45.40, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "diff_mat2", 34.40, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "diff_mat2", 31.90, MedDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "diff_mat2", 28.25, Full));
        futures.push_back(threads.Enqueue(run_image_test, "diff_mat3", 46.60, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "diff_mat3", 35.95, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "diff_mat3", 26.35, MedDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "diff_mat3", 23.60, Full));
        futures.push_back(threads.Enqueue(run_image_test, "diff_mat4", 47.35, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "diff_mat4", 35.30, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "diff_mat4", 25.60, MedDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "diff_mat4", 21.65, Full));
        futures.push_back(threads.Enqueue(run_image_test, "diff_mat5", 44.70, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "diff_mat5", 35.40, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "diff_mat5", 26.35, MedDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "diff_mat5", 23.45, Full));

        for (auto &f : futures) {
            f.wait();
        }
    }
    puts(" ---------------");
    /*if (g_tests_success)*/ { // sheen material
        std::vector<std::future<void>> futures;

        futures.push_back(threads.Enqueue(run_image_test, "sheen_mat0", 46.70, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "sheen_mat0", 35.10, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "sheen_mat0", 30.55, MedDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "sheen_mat0", 29.10, Full));
        futures.push_back(threads.Enqueue(run_image_test, "sheen_mat1", 43.95, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "sheen_mat1", 34.85, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "sheen_mat1", 27.35, MedDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "sheen_mat1", 26.75, Full));
        futures.push_back(threads.Enqueue(run_image_test, "sheen_mat2", 45.40, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "sheen_mat2", 34.85, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "sheen_mat2", 29.70, MedDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "sheen_mat2", 27.65, Full));
        futures.push_back(threads.Enqueue(run_image_test, "sheen_mat3", 42.15, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "sheen_mat3", 34.45, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "sheen_mat3", 28.70, MedDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "sheen_mat3", 26.35, Full));
        futures.push_back(threads.Enqueue(run_image_test, "sheen_mat4", 45.30, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "sheen_mat4", 36.60, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "sheen_mat4", 26.50, MedDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "sheen_mat4", 21.80, Full));
        futures.push_back(threads.Enqueue(run_image_test, "sheen_mat5", 42.35, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "sheen_mat5", 36.10, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "sheen_mat5", 24.15, MedDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "sheen_mat5", 20.80, Full));
        futures.push_back(threads.Enqueue(run_image_test, "sheen_mat6", 44.50, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "sheen_mat6", 36.15, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "sheen_mat6", 26.45, MedDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "sheen_mat6", 20.50, Full));
        futures.push_back(threads.Enqueue(run_image_test, "sheen_mat7", 40.10, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "sheen_mat7", 35.25, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "sheen_mat7", 26.20, MedDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "sheen_mat7", 19.80, Full));

        for (auto &f : futures) {
            f.wait();
        }
    }
    puts(" ---------------");
    /*if (g_tests_success)*/ { // specular material
        std::vector<std::future<void>> futures;

        futures.push_back(threads.Enqueue(run_image_test, "spec_mat0", 40.10, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat0", 33.85, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat0", 26.80, NoDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat0", 24.20, MedDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat0", 24.00, Full));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat0", 27.30, Full_Ultra));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat1", 19.60, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat1", 19.40, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat1", 21.90, NoDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat1", 18.45, MedDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat1", 18.35, Full));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat1", 19.00, Full_Ultra));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat2", 44.85, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat2", 34.00, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat2", 28.30, NoDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat2", 26.25, MedDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat2", 25.80, Full));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat2", 25.45, Full_Ultra));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat3", 36.40, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat3", 33.60, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat3", 28.85, NoDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat3", 21.75, MedDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat3", 19.70, Full));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat3", 22.50, Full_Ultra));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat4", 21.00, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat4", 21.10, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat4", 22.05, NoDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat4", 15.60, MedDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat4", 14.90, Full));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat4", 14.70, Full_Ultra));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat5", 32.15, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat5", 30.30, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat5", 27.95, NoDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat5", 20.15, MedDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat5", 18.75, Full));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat5", 18.50, Full_Ultra));

        for (auto &f : futures) {
            f.wait();
        }
    }
    puts(" ---------------");
    /*if (g_tests_success)*/ { // metal material
        std::vector<std::future<void>> futures;

        futures.push_back(threads.Enqueue(run_image_test, "metal_mat0", 32.65, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat0", 30.75, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat0", 28.85, NoDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat0", 26.05, MedDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat0", 25.85, Full));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat0", 28.10, Full_Ultra));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat1", 23.60, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat1", 23.20, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat1", 26.65, NoDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat1", 25.00, MedDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat1", 24.60, Full));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat1", 25.10, Full_Ultra));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat2", 37.00, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat2", 32.80, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat2", 32.60, NoDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat2", 30.95, MedDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat2", 29.75, Full));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat2", 29.85, Full_Ultra));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat3", 38.05, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat3", 34.35, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat3", 30.20, NoDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat3", 22.90, MedDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat3", 20.65, Full));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat3", 22.05, Full_Ultra));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat4", 25.25, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat4", 25.20, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat4", 26.50, NoDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat4", 20.50, MedDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat4", 18.75, Full));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat4", 18.55, Full_Ultra));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat5", 37.30, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat5", 33.80, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat5", 32.05, NoDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat5", 25.55, MedDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat5", 21.60, Full));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat5", 21.65, Full_Ultra));

        for (auto &f : futures) {
            f.wait();
        }
    }
    puts(" ---------------");
    /*if (g_tests_success)*/ { // plastic material
        std::vector<std::future<void>> futures;

        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat0", 43.70, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat0", 34.35, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat0", 32.00, NoDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat0", 29.30, MedDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat0", 26.80, Full));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat0", 28.05, Full_Ultra));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat1", 36.20, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat1", 32.75, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat1", 28.00, NoDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat1", 24.45, MedDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat1", 23.40, Full));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat1", 23.65, Full_Ultra));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat2", 41.80, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat2", 33.50, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat2", 32.65, NoDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat2", 30.25, MedDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat2", 26.45, Full));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat2", 27.55, Full_Ultra));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat3", 38.45, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat3", 34.40, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat3", 31.45, NoDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat3", 24.90, MedDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat3", 22.00, Full));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat3", 23.45, Full_Ultra));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat4", 35.75, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat4", 33.35, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat4", 28.30, NoDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat4", 22.10, MedDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat4", 20.35, Full));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat4", 20.90, Full_Ultra));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat5", 42.35, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat5", 34.70, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat5", 30.40, NoDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat5", 25.65, MedDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat5", 21.60, Full));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat5", 23.05, Full_Ultra));

        for (auto &f : futures) {
            f.wait();
        }
    }
    puts(" ---------------");
    /*if (g_tests_success)*/ { // tint material
        std::vector<std::future<void>> futures;

        futures.push_back(threads.Enqueue(run_image_test, "tint_mat0", 43.25, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat0", 34.30, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat0", 33.45, NoDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat0", 29.10, MedDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat0", 26.35, Full));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat1", 28.65, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat1", 27.65, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat1", 24.40, NoDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat1", 22.15, MedDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat1", 20.80, Full));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat2", 40.45, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat2", 33.15, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat2", 31.45, NoDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat2", 28.90, MedDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat2", 25.40, Full));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat3", 43.95, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat3", 35.65, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat3", 34.05, NoDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat3", 26.90, MedDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat3", 20.20, Full));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat4", 28.00, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat4", 27.80, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat4", 23.50, NoDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat4", 22.20, MedDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat4", 17.30, Full));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat5", 39.85, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat5", 34.15, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat5", 29.25, NoDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat5", 25.45, MedDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat5", 17.90, Full));

        for (auto &f : futures) {
            f.wait();
        }
    }
    puts(" ---------------");
    /*if (g_tests_success)*/ { // clearcoat material
        std::vector<std::future<void>> futures;

        futures.push_back(threads.Enqueue(run_image_test, "coat_mat0", 41.10, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "coat_mat0", 34.45, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "coat_mat1", 32.05, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "coat_mat1", 30.70, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "coat_mat2", 28.90, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "coat_mat2", 28.50, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "coat_mat3", 38.75, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "coat_mat3", 35.10, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "coat_mat4", 30.85, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "coat_mat4", 30.55, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "coat_mat5", 28.00, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "coat_mat5", 27.80, NoGI));

        for (auto &f : futures) {
            f.wait();
        }
    }
    puts(" ---------------");
    /*if (g_tests_success)*/ { // alpha material
        std::vector<std::future<void>> futures;

        futures.push_back(threads.Enqueue(run_image_test, "alpha_mat0", 37.20, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "alpha_mat0", 25.00, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "alpha_mat0", 22.70, NoDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "alpha_mat0", 22.05, MedDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "alpha_mat0", 22.55, Full));
        futures.push_back(threads.Enqueue(run_image_test, "alpha_mat0", 22.35, Full_Ultra));
        futures.push_back(threads.Enqueue(run_image_test, "alpha_mat1", 38.40, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "alpha_mat1", 24.40, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "alpha_mat1", 23.40, NoDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "alpha_mat1", 22.70, MedDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "alpha_mat1", 23.30, Full));
        futures.push_back(threads.Enqueue(run_image_test, "alpha_mat1", 23.15, Full_Ultra));
        futures.push_back(threads.Enqueue(run_image_test, "alpha_mat2", 41.30, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "alpha_mat2", 27.45, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "alpha_mat2", 26.80, NoDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "alpha_mat2", 26.20, MedDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "alpha_mat2", 27.10, Full));
        futures.push_back(threads.Enqueue(run_image_test, "alpha_mat2", 27.00, Full_Ultra));
        futures.push_back(threads.Enqueue(run_image_test, "alpha_mat3", 47.05, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "alpha_mat3", 36.70, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "alpha_mat3", 36.70, NoDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "alpha_mat3", 33.75, MedDiffGI));
        futures.push_back(threads.Enqueue(run_image_test, "alpha_mat3", 29.55, Full));
        futures.push_back(threads.Enqueue(run_image_test, "alpha_mat3", 30.25, Full_Ultra));

        for (auto &f : futures) {
            f.wait();
        }
    }
}
