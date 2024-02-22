#include "test_common.h"

#include <chrono>
#include <future>
#include <istream>

#include <stb/stb_image.h>
#include <stb/stb_image_write.h>
#if defined(USE_GL_RENDER)
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
std::mutex g_stbi_mutex;
}

enum eImgTest { NoShadow, NoGI, NoDiffuseGI, Full };

void run_image_test(const char *test_name, const char *device_name, int validation_level, const double min_psnr,
                    const int pix_thres, const eImgTest img_test = eImgTest::NoShadow) {
    using namespace std::chrono;

    const auto start_time = high_resolution_clock::now();

    const char *test_postfix = "";
    if (img_test == eImgTest::NoShadow) {
        test_postfix = "_noshadow";
    } else if (img_test == eImgTest::NoGI) {
        test_postfix = "_nogi";
    } else if (img_test == eImgTest::NoDiffuseGI) {
        test_postfix = "_nodiffusegi";
    }
    const std::string ref_name =
        std::string("assets_pc/references/") + test_name + "/ref" + test_postfix + ".uncompressed.png";

    int ref_w, ref_h, ref_channels;
    uint8_t *ref_img = stbi_load(ref_name.c_str(), &ref_w, &ref_h, &ref_channels, 4);
    SCOPE_EXIT({ stbi_image_free(ref_img); })

    LogErr log;
    TestContext ren_ctx(ref_w, ref_h, device_name, validation_level, &log);
#if defined(USE_VK_RENDER)
    Ren::ApiContext *api_ctx = ren_ctx.api_ctx();
    require_return(!device_name || Ren::MatchDeviceNames(api_ctx->device_properties.deviceName, device_name));
#endif

    Eng::ShaderLoader shader_loader;
    Eng::Random rand(0);
    Sys::ThreadPool threads(1);
    Eng::Renderer renderer(ren_ctx, shader_loader, rand, threads);

    uint64_t render_flags = Eng::EnableZFill | Eng::EnableCulling | Eng::EnableSSAO | Eng::EnableLightmap |
                            Eng::EnableLights | Eng::EnableDecals | Eng::EnableTaa | Eng::EnableTaaStatic |
                            Eng::EnableTimers | Eng::EnableDOF | Eng::EnableDeferred | Eng::EnableHQ_HDR |
                            Eng::EnableShadows | Eng::EnableSSR | Eng::EnableSSR_HQ | Eng::EnableGI;
    if (img_test == eImgTest::NoShadow) {
        render_flags &= ~Eng::EnableShadows;
        render_flags &= ~Eng::EnableSSR;
        render_flags &= ~Eng::EnableSSR_HQ;
        render_flags &= ~Eng::EnableGI;
    } else if (img_test == eImgTest::NoGI) {
        render_flags &= ~Eng::EnableSSR;
        render_flags &= ~Eng::EnableSSR_HQ;
        render_flags &= ~Eng::EnableGI;
    } else if (img_test == eImgTest::NoDiffuseGI) {
        render_flags &= ~Eng::EnableGI;
    }
    renderer.set_render_flags(render_flags);

    Eng::path_config_t paths;
    Eng::SceneManager scene_manager(ren_ctx, shader_loader, nullptr, threads, paths);

    using namespace std::placeholders;
    scene_manager.SetPipelineInitializer(std::bind(&Eng::Renderer::InitPipelinesForProgram, &renderer, _1, _2, _3, _4));

    Sys::MultiPoolAllocator<char> alloc(32, 512);
    JsObjectP js_scene(alloc);

    { // Load scene data from file
        const std::string scene_name = std::string("assets_pc/scenes/") + test_name + ".json";
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
    float view_fov = 45.0f, max_exposure = 1.0f;

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

        if (js_cam.Has("max_exposure")) {
            const JsNumber &js_max_exposure = js_cam.at("max_exposure").as_num();
            max_exposure = float(js_max_exposure.val);
        }
    }

    if (img_test == eImgTest::NoDiffuseGI) {
        if (js_scene.Has("environment")) {
            JsObjectP &js_env = js_scene.at("environment").as_obj();
            if (js_env.Has("ambient_hack")) {
                JsArrayP &js_ambient_hack = js_env.at("ambient_hack").as_arr();
                js_ambient_hack[0].as_num().val = 0.0;
                js_ambient_hack[1].as_num().val = 0.0;
                js_ambient_hack[2].as_num().val = 0.0;
            }
        }
    }

    scene_manager.LoadScene(js_scene);
    scene_manager.SetupView(view_pos, view_pos + view_dir, Ren::Vec3f{0.0f, 1.0f, 0.0f}, view_fov, false, max_exposure);

    //
    // Create required staging buffers
    //
    Ren::BufferRef instances_stage_buf = ren_ctx.LoadBuffer("Instances (Stage)", Ren::eBufType::Stage,
                                                            Eng::InstanceDataBufChunkSize * Ren::MaxFramesInFlight);
    Ren::BufferRef instance_indices_stage_buf = ren_ctx.LoadBuffer(
        "Instance Indices (Stage)", Ren::eBufType::Stage, Eng::InstanceIndicesBufChunkSize * Ren::MaxFramesInFlight);
    Ren::BufferRef skin_transforms_stage_buf = ren_ctx.LoadBuffer(
        "Skin Transforms (Stage)", Ren::eBufType::Stage, Eng::SkinTransformsBufChunkSize * Ren::MaxFramesInFlight);
    Ren::BufferRef shape_keys_stage_buf = ren_ctx.LoadBuffer("Shape Keys (Stage)", Ren::eBufType::Stage,
                                                             Eng::ShapeKeysBufChunkSize * Ren::MaxFramesInFlight);
    Ren::BufferRef cells_stage_buf =
        ren_ctx.LoadBuffer("Cells (Stage)", Ren::eBufType::Stage, Eng::CellsBufChunkSize * Ren::MaxFramesInFlight);
    Ren::BufferRef rt_cells_stage_buf =
        ren_ctx.LoadBuffer("RT Cells (Stage)", Ren::eBufType::Stage, Eng::CellsBufChunkSize * Ren::MaxFramesInFlight);
    Ren::BufferRef items_stage_buf =
        ren_ctx.LoadBuffer("Items (Stage)", Ren::eBufType::Stage, Eng::ItemsBufChunkSize * Ren::MaxFramesInFlight);
    Ren::BufferRef rt_items_stage_buf =
        ren_ctx.LoadBuffer("RT Items (Stage)", Ren::eBufType::Stage, Eng::ItemsBufChunkSize * Ren::MaxFramesInFlight);
    Ren::BufferRef lights_stage_buf =
        ren_ctx.LoadBuffer("Lights (Stage)", Ren::eBufType::Stage, Eng::LightsBufChunkSize * Ren::MaxFramesInFlight);
    Ren::BufferRef decals_stage_buf =
        ren_ctx.LoadBuffer("Decals (Stage)", Ren::eBufType::Stage, Eng::DecalsBufChunkSize * Ren::MaxFramesInFlight);
    Ren::BufferRef rt_obj_instances_stage_buf, rt_sh_obj_instances_stage_buf, rt_tlas_nodes_stage_buf,
        rt_sh_tlas_nodes_stage_buf;
    if (ren_ctx.capabilities.raytracing) {
        rt_obj_instances_stage_buf = ren_ctx.LoadBuffer("RT Obj Instances (Stage)", Ren::eBufType::Stage,
                                                        Eng::HWRTObjInstancesBufChunkSize * Ren::MaxFramesInFlight);
        rt_sh_obj_instances_stage_buf = ren_ctx.LoadBuffer("RT Shadow Obj Instances (Stage)", Ren::eBufType::Stage,
                                                           Eng::HWRTObjInstancesBufChunkSize * Ren::MaxFramesInFlight);
    } else if (ren_ctx.capabilities.swrt) {
        rt_obj_instances_stage_buf = ren_ctx.LoadBuffer("RT Obj Instances (Stage)", Ren::eBufType::Stage,
                                                        Eng::SWRTObjInstancesBufChunkSize * Ren::MaxFramesInFlight);
        rt_sh_obj_instances_stage_buf = ren_ctx.LoadBuffer("RT Shadow Obj Instances (Stage)", Ren::eBufType::Stage,
                                                           Eng::SWRTObjInstancesBufChunkSize * Ren::MaxFramesInFlight);
        rt_tlas_nodes_stage_buf = ren_ctx.LoadBuffer("SWRT TLAS Nodes (Stage)", Ren::eBufType::Stage,
                                                     Eng::SWRTTLASNodesBufChunkSize * Ren::MaxFramesInFlight);
        rt_sh_tlas_nodes_stage_buf = ren_ctx.LoadBuffer("SWRT Shadow TLAS Nodes (Stage)", Ren::eBufType::Stage,
                                                        Eng::SWRTTLASNodesBufChunkSize * Ren::MaxFramesInFlight);
    }

    Ren::BufferRef shared_data_stage_buf = ren_ctx.LoadBuffer("Shared Data (Stage)", Ren::eBufType::Stage,
                                                              Eng::SharedDataBlockSize * Ren::MaxFramesInFlight);

    //
    // Initialize draw list
    //
    Eng::DrawList draw_list;
    draw_list.Init(shared_data_stage_buf, instances_stage_buf, instance_indices_stage_buf, skin_transforms_stage_buf,
                   shape_keys_stage_buf, cells_stage_buf, rt_cells_stage_buf, items_stage_buf, rt_items_stage_buf,
                   lights_stage_buf, decals_stage_buf, rt_obj_instances_stage_buf, rt_sh_obj_instances_stage_buf,
                   rt_tlas_nodes_stage_buf, rt_sh_tlas_nodes_stage_buf);
    draw_list.render_flags = renderer.render_flags();

    renderer.PrepareDrawList(scene_manager.scene_data(), scene_manager.main_cam(), scene_manager.ext_cam(), draw_list);
    scene_manager.UpdateTexturePriorities(draw_list.visible_textures.data, draw_list.visible_textures.count,
                                          draw_list.desired_textures.data, draw_list.desired_textures.count);

    //
    // Render image
    //

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

    // Make sure all textures are loaded
    bool finished = false;
    while (!finished) {
        begin_frame();
        finished = scene_manager.Serve(1, false);
        end_frame();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    for (int i = 0; i < RendererInternal::TaaSampleCountStatic; ++i) {
        draw_list.Clear();
        renderer.PrepareDrawList(scene_manager.scene_data(), scene_manager.main_cam(), scene_manager.ext_cam(),
                                 draw_list);

        begin_frame();
        renderer.ExecuteDrawList(draw_list, scene_manager.persistent_data(), render_result);
        end_frame();
    }

    Ren::BufferRef stage_buf = ren_ctx.LoadBuffer("Temp stage buf", Ren::eBufType::Stage, 4 * ref_w * ref_h);

    { // Download result
        void *cmd_buf = ren_ctx.BegTempSingleTimeCommands();
        render_result->CopyTextureData(*stage_buf, cmd_buf, 0);
        ren_ctx.EndTempSingleTimeCommands(cmd_buf);
    }

    std::unique_ptr<uint8_t[]> diff_data_u8(new uint8_t[ref_w * ref_h * 3]);
    std::unique_ptr<uint8_t[]> mask_data_u8(new uint8_t[ref_w * ref_h * 3]);
    memset(&mask_data_u8[0], 0, ref_w * ref_h * 3);

    const uint8_t *img_data = stage_buf->Map(Ren::eBufMap::Read);
    SCOPE_EXIT({ stage_buf->Unmap(); })

    double mse = 0.0;

    const int DiffThres = 32;
    int error_pixels = 0;
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

            if (diff_r > DiffThres || diff_g > DiffThres || diff_b > DiffThres) {
                mask_data_u8[3 * (j * ref_w + i) + 0] = 255;
                ++error_pixels;
            }

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

    std::lock_guard<std::mutex> _(g_stbi_mutex);

    printf("Test %s%-12s (PSNR: %.2f/%.2f dB, Fireflies: %i/%i, Time: %.2fms)\n", test_name, test_postfix, psnr,
           min_psnr, error_pixels, pix_thres, test_duration_ms);
    fflush(stdout);
    require(psnr >= min_psnr && error_pixels <= pix_thres);

    stbi_flip_vertically_on_write(flip_y);

    const std::string out_name = std::string("assets_pc/references/") + test_name + "/out" + test_postfix + ".png";
    const std::string diff_name = std::string("assets_pc/references/") + test_name + "/diff" + test_postfix + ".png";
    const std::string mask_name = std::string("assets_pc/references/") + test_name + "/mask" + test_postfix + ".png";

    stbi_write_png(out_name.c_str(), ref_w, ref_h, 4, img_data, 4 * ref_w);
    stbi_flip_vertically_on_write(false);
    stbi_write_png(diff_name.c_str(), ref_w, ref_h, 3, diff_data_u8.get(), 3 * ref_w);
    stbi_write_png(mask_name.c_str(), ref_w, ref_h, 3, mask_data_u8.get(), 3 * ref_w);
}

void test_materials(Sys::ThreadPool &threads, const char *device_name, int vl) {
    { // complex materials
        std::vector<std::future<void>> futures;

        futures.push_back(threads.Enqueue(run_image_test, "complex_mat0", device_name, vl, 34.69, 1320, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat0", device_name, vl, 31.38, 1860, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat0", device_name, vl, 29.62, 2705, NoDiffuseGI));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat0", device_name, vl, 28.71, 4265, Full));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat1", device_name, vl, 34.31, 815, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat1", device_name, vl, 31.65, 1275, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat1", device_name, vl, 30.26, 1935, NoDiffuseGI));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat1", device_name, vl, 29.55, 3005, Full));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat2", device_name, vl, 33.70, 950, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat2", device_name, vl, 31.76, 1470, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat2", device_name, vl, 26.87, 7280, NoDiffuseGI));
        futures.push_back(threads.Enqueue(run_image_test, "complex_mat2", device_name, vl, 21.07, 19080, Full));

        for (auto &f : futures) {
            f.wait();
        }
    }
    puts(" ---------------");
    { // diffuse material
        std::vector<std::future<void>> futures;

        futures.push_back(threads.Enqueue(run_image_test, "diff_mat0", device_name, vl, 38.51, 10, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "diff_mat0", device_name, vl, 31.92, 925, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "diff_mat0", device_name, vl, 33.17, 750, Full));
        futures.push_back(threads.Enqueue(run_image_test, "diff_mat1", device_name, vl, 38.66, 10, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "diff_mat1", device_name, vl, 31.94, 990, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "diff_mat1", device_name, vl, 32.97, 730, Full));
        futures.push_back(threads.Enqueue(run_image_test, "diff_mat2", device_name, vl, 38.02, 10, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "diff_mat2", device_name, vl, 31.43, 1080, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "diff_mat2", device_name, vl, 32.39, 740, Full));
        futures.push_back(threads.Enqueue(run_image_test, "diff_mat3", device_name, vl, 38.37, 10, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "diff_mat3", device_name, vl, 31.46, 1215, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "diff_mat3", device_name, vl, 32.17, 785, Full));
        futures.push_back(threads.Enqueue(run_image_test, "diff_mat4", device_name, vl, 38.35, 10, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "diff_mat4", device_name, vl, 31.74, 1350, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "diff_mat4", device_name, vl, 32.36, 1000, Full));
        futures.push_back(threads.Enqueue(run_image_test, "diff_mat5", device_name, vl, 39.88, 10, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "diff_mat5", device_name, vl, 33.94, 695, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "diff_mat5", device_name, vl, 30.34, 1920, Full));
        futures.push_back(threads.Enqueue(run_image_test, "diff_mat6", device_name, vl, 40.24, 10, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "diff_mat6", device_name, vl, 33.97, 705, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "diff_mat6", device_name, vl, 30.17, 1250, Full));
        futures.push_back(threads.Enqueue(run_image_test, "diff_mat7", device_name, vl, 39.57, 10, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "diff_mat7", device_name, vl, 33.37, 750, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "diff_mat7", device_name, vl, 29.01, 1000, Full));
        futures.push_back(threads.Enqueue(run_image_test, "diff_mat8", device_name, vl, 39.78, 10, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "diff_mat8", device_name, vl, 33.41, 780, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "diff_mat8", device_name, vl, 28.88, 910, Full));
        futures.push_back(threads.Enqueue(run_image_test, "diff_mat9", device_name, vl, 39.60, 10, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "diff_mat9", device_name, vl, 33.74, 870, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "diff_mat9", device_name, vl, 29.45, 1280, Full));

        for (auto &f : futures) {
            f.wait();
        }
    }
    puts(" ---------------");
    { // sheen material
        std::vector<std::future<void>> futures;

        futures.push_back(threads.Enqueue(run_image_test, "sheen_mat0", device_name, vl, 38.95, 10, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "sheen_mat0", device_name, vl, 32.33, 720, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "sheen_mat0", device_name, vl, 30.07, 700, Full));
        futures.push_back(threads.Enqueue(run_image_test, "sheen_mat1", device_name, vl, 30.21, 10, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "sheen_mat1", device_name, vl, 32.16, 720, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "sheen_mat1", device_name, vl, 27.26, 4105, Full));
        futures.push_back(threads.Enqueue(run_image_test, "sheen_mat2", device_name, vl, 38.31, 10, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "sheen_mat2", device_name, vl, 32.05, 720, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "sheen_mat2", device_name, vl, 29.68, 4110, Full));
        futures.push_back(threads.Enqueue(run_image_test, "sheen_mat3", device_name, vl, 37.17, 10, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "sheen_mat3", device_name, vl, 31.75, 720, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "sheen_mat3", device_name, vl, 28.71, 3415, Full));
        futures.push_back(threads.Enqueue(run_image_test, "sheen_mat4", device_name, vl, 39.86, 10, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "sheen_mat4", device_name, vl, 34.30, 460, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "sheen_mat4", device_name, vl, 23.56, 20542, Full));
        futures.push_back(threads.Enqueue(run_image_test, "sheen_mat5", device_name, vl, 38.61, 10, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "sheen_mat5", device_name, vl, 33.91, 460, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "sheen_mat5", device_name, vl, 22.16, 32890, Full));
        futures.push_back(threads.Enqueue(run_image_test, "sheen_mat6", device_name, vl, 39.24, 10, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "sheen_mat6", device_name, vl, 33.93, 460, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "sheen_mat6", device_name, vl, 22.80, 38900, Full));
        futures.push_back(threads.Enqueue(run_image_test, "sheen_mat7", device_name, vl, 37.22, 10, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "sheen_mat7", device_name, vl, 33.22, 460, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "sheen_mat7", device_name, vl, 22.16, 40010, Full));

        for (auto &f : futures) {
            f.wait();
        }
    }
    puts(" ---------------");
    { // specular material
        std::vector<std::future<void>> futures;

        futures.push_back(threads.Enqueue(run_image_test, "spec_mat0", device_name, vl, 33.36, 440, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat0", device_name, vl, 30.88, 1060, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat0", device_name, vl, 25.51, 7935, NoDiffuseGI));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat0", device_name, vl, 18.43, 19850, Full));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat1", device_name, vl, 29.47, 1500, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat1", device_name, vl, 28.41, 2205, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat1", device_name, vl, 22.92, 9285, NoDiffuseGI));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat1", device_name, vl, 17.10, 21230, Full));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat2", device_name, vl, 19.64, 61185, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat2", device_name, vl, 19.44, 62030, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat2", device_name, vl, 20.53, 40240, NoDiffuseGI));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat2", device_name, vl, 18.07, 41635, Full));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat3", device_name, vl, 22.63, 23385, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat3", device_name, vl, 21.65, 27970, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat3", device_name, vl, 21.87, 32315, NoDiffuseGI));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat3", device_name, vl, 20.90, 42285, Full));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat4", device_name, vl, 37.84, 10, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat4", device_name, vl, 31.22, 895, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat4", device_name, vl, 26.73, 7305, NoDiffuseGI));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat4", device_name, vl, 24.42, 15010, Full));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat5", device_name, vl, 34.37, 205, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat5", device_name, vl, 32.44, 645, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat5", device_name, vl, 26.29, 8495, NoDiffuseGI));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat5", device_name, vl, 16.01, 40215, Full));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat6", device_name, vl, 29.30, 3470, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat6", device_name, vl, 28.97, 3780, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat6", device_name, vl, 23.12, 8660, NoDiffuseGI));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat6", device_name, vl, 15.79, 37275, Full));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat7", device_name, vl, 21.03, 56555, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat7", device_name, vl, 21.14, 54710, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat7", device_name, vl, 20.21, 36895, NoDiffuseGI));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat7", device_name, vl, 15.45, 47585, Full));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat8", device_name, vl, 24.15, 18070, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat8", device_name, vl, 23.35, 21230, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat8", device_name, vl, 22.10, 35165, NoDiffuseGI));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat8", device_name, vl, 16.31, 55975, Full));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat9", device_name, vl, 30.57, 1830, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat9", device_name, vl, 28.83, 3220, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat9", device_name, vl, 22.67, 27303, NoDiffuseGI));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat9", device_name, vl, 16.96, 111641, Full));

        for (auto &f : futures) {
            f.wait();
        }
    }
    puts(" ---------------");
    { // metal material
        std::vector<std::future<void>> futures;

        futures.push_back(threads.Enqueue(run_image_test, "metal_mat0", device_name, vl, 31.36, 1555, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat0", device_name, vl, 29.48, 2135, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat0", device_name, vl, 26.95, 10425, NoDiffuseGI));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat0", device_name, vl, 24.56, 14145, Full));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat1", device_name, vl, 30.83, 3820, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat1", device_name, vl, 29.04, 4435, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat1", device_name, vl, 27.21, 4560, NoDiffuseGI));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat1", device_name, vl, 24.71, 13565, Full));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat2", device_name, vl, 23.60, 54435, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat2", device_name, vl, 23.24, 55545, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat2", device_name, vl, 25.22, 16055, NoDiffuseGI));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat2", device_name, vl, 23.60, 54435, Full));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat3", device_name, vl, 26.45, 22140, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat3", device_name, vl, 25.62, 24580, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat3", device_name, vl, 26.84, 19845, NoDiffuseGI));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat3", device_name, vl, 26.45, 22140, Full));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat4", device_name, vl, 35.73, 10, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat4", device_name, vl, 31.37, 770, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat4", device_name, vl, 30.44, 3615, NoDiffuseGI));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat4", device_name, vl, 29.29, 5540, Full));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat5", device_name, vl, 35.51, 225, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat5", device_name, vl, 32.64, 655, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat5", device_name, vl, 28.07, 9405, NoDiffuseGI));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat5", device_name, vl, 20.89, 40970, Full));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat6", device_name, vl, 33.46, 1520, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat6", device_name, vl, 31.66, 2025, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat6", device_name, vl, 26.97, 5200, NoDiffuseGI));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat6", device_name, vl, 20.63, 37850, Full));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat7", device_name, vl, 25.26, 19630, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat7", device_name, vl, 25.23, 16525, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat7", device_name, vl, 24.60, 19565, NoDiffuseGI));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat7", device_name, vl, 20.34, 46490, Full));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat8", device_name, vl, 28.10, 15400, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat8", device_name, vl, 27.61, 17060, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat8", device_name, vl, 26.43, 15250, NoDiffuseGI));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat8", device_name, vl, 21.43, 53830, Full));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat9", device_name, vl, 35.47, 440, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat9", device_name, vl, 32.40, 1255, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat9", device_name, vl, 27.82, 7405, NoDiffuseGI));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat9", device_name, vl, 21.67, 76915, Full));

        for (auto &f : futures) {
            f.wait();
        }
    }
    puts(" ---------------");
    { // plastic material
        std::vector<std::future<void>> futures;

        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat0", device_name, vl, 37.54, 460, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat0", device_name, vl, 31.95, 1290, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat0", device_name, vl, 28.19, 5470, NoDiffuseGI));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat0", device_name, vl, 29.19, 3030, Full));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat1", device_name, vl, 37.74, 10, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat1", device_name, vl, 31.73, 990, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat1", device_name, vl, 30.95, 940, NoDiffuseGI));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat1", device_name, vl, 30.18, 1570, Full));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat2", device_name, vl, 34.65, 625, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat2", device_name, vl, 30.09, 1680, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat2", device_name, vl, 20.43, 1405, NoDiffuseGI));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat2", device_name, vl, 30.93, 1720, Full));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat3", device_name, vl, 37.59, 1160, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat3", device_name, vl, 30.70, 2320, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat3", device_name, vl, 30.90, 2065, NoDiffuseGI));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat3", device_name, vl, 31.29, 1525, Full));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat4", device_name, vl, 37.72, 15, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat4", device_name, vl, 31.24, 1350, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat4", device_name, vl, 30.15, 1850, NoDiffuseGI));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat4", device_name, vl, 31.27, 930, Full));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat5", device_name, vl, 35.82, 185, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat5", device_name, vl, 33.17, 865, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat5", device_name, vl, 28.64, 6230, NoDiffuseGI));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat5", device_name, vl, 26.1, 10750, Full));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat6", device_name, vl, 38.51, 10, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat6", device_name, vl, 33.52, 715, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat6", device_name, vl, 30.79, 1190, NoDiffuseGI));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat6", device_name, vl, 27.38, 7360, Full));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat7", device_name, vl, 34.95, 1165, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat7", device_name, vl, 32.20, 1845, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat7", device_name, vl, 29.65, 1943, NoDiffuseGI));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat7", device_name, vl, 28.16, 5010, Full));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat8", device_name, vl, 35.64, 2145, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat8", device_name, vl, 32.11, 2730, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat8", device_name, vl, 29.40, 3095, NoDiffuseGI));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat8", device_name, vl, 28.63, 2510, Full));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat9", device_name, vl, 38.88, 10, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat9", device_name, vl, 33.09, 895, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat9", device_name, vl, 29.22, 4100, NoDiffuseGI));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat9", device_name, vl, 27.67, 1125, Full));

        for (auto &f : futures) {
            f.wait();
        }
    }
    puts(" ---------------");
    { // tint material
        std::vector<std::future<void>> futures;

        futures.push_back(threads.Enqueue(run_image_test, "tint_mat0", device_name, vl, 40.51, 365, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat0", device_name, vl, 32.60, 1200, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat0", device_name, vl, 27.82, 7285, NoDiffuseGI));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat0", device_name, vl, 28.49, 4300, Full));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat1", device_name, vl, 36.40, 10, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat1", device_name, vl, 31.38, 990, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat1", device_name, vl, 29.45, 1300, NoDiffuseGI));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat1", device_name, vl, 29.74, 1815, Full));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat2", device_name, vl, 27.76, 17680, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat2", device_name, vl, 26.84, 17630, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat2", device_name, vl, 24.43, 53410, NoDiffuseGI));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat2", device_name, vl, 23.78, 71660, Full));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat3", device_name, vl, 34.01, 4250, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat3", device_name, vl, 30.57, 4730, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat3", device_name, vl, 29.09, 4825, NoDiffuseGI));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat3", device_name, vl, 30.70, 3485, Full));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat4", device_name, vl, 35.57, 10, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat4", device_name, vl, 30.55, 1440, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat4", device_name, vl, 28.95, 2710, NoDiffuseGI));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat4", device_name, vl, 30.08, 1250, Full));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat5", device_name, vl, 38.37, 175, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat5", device_name, vl, 33.52, 845, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat5", device_name, vl, 28.02, 7955, NoDiffuseGI));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat5", device_name, vl, 23.50, 33360, Full));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat6", device_name, vl, 36.08, 1135, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat6", device_name, vl, 32.64, 1855, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat6", device_name, vl, 30.08, 2795, NoDiffuseGI));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat6", device_name, vl, 23.54, 33870, Full));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat7", device_name, vl, 27.40, 18035, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat7", device_name, vl, 27.12, 16500, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat7", device_name, vl, 23.51, 62860, NoDiffuseGI));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat7", device_name, vl, 21.02, 84100, Full));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat8", device_name, vl, 32.86, 6205, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat8", device_name, vl, 31.15, 5700, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat8", device_name, vl, 28.71, 7240, NoDiffuseGI));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat8", device_name, vl, 22.72, 35235, Full));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat9", device_name, vl, 35.86, 10, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat9", device_name, vl, 31.96, 965, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat9", device_name, vl, 27.70, 6995, NoDiffuseGI));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat9", device_name, vl, 21.81, 39685, Full));

        for (auto &f : futures) {
            f.wait();
        }
    }
    puts(" ---------------");
    { // clearcoat material
        std::vector<std::future<void>> futures;

        futures.push_back(threads.Enqueue(run_image_test, "coat_mat0", device_name, vl, 37.53, 450, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "coat_mat0", device_name, vl, 32.03, 1100, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "coat_mat1", device_name, vl, 30.31, 1110, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "coat_mat1", device_name, vl, 28.78, 1785, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "coat_mat2", device_name, vl, 31.36, 890, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "coat_mat2", device_name, vl, 29.44, 1555, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "coat_mat3", device_name, vl, 31.04, 1575, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "coat_mat3", device_name, vl, 29.20, 2200, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "coat_mat4", device_name, vl, 29.12, 3295, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "coat_mat4", device_name, vl, 28.01, 3560, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "coat_mat5", device_name, vl, 36.83, 350, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "coat_mat5", device_name, vl, 33.27, 785, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "coat_mat6", device_name, vl, 28.75, 2755, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "coat_mat6", device_name, vl, 28.24, 3210, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "coat_mat7", device_name, vl, 30.87, 1950, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "coat_mat7", device_name, vl, 29.96, 2345, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "coat_mat8", device_name, vl, 30.22, 2985, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "coat_mat8", device_name, vl, 29.63, 3235, NoGI));
        futures.push_back(threads.Enqueue(run_image_test, "coat_mat9", device_name, vl, 28.11, 5005, NoShadow));
        futures.push_back(threads.Enqueue(run_image_test, "coat_mat9", device_name, vl, 27.86, 4720, NoGI));

        for (auto &f : futures) {
            f.wait();
        }
    }
}
