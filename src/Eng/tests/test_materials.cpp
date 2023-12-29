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

#include "../Random.h"
#include "../renderer/Renderer.h"
#include "../scene/SceneManager.h"
#include "../utils/ShaderLoader.h"

namespace RendererInternal {
extern const int TaaSampleCountStatic;
}

namespace {
std::mutex g_stbi_mutex;
}

void run_image_test(const char *test_name, const char *device_name, int validation_level, const double min_psnr,
                    const int pix_thres) {
    using namespace std::chrono;

    const auto start_time = high_resolution_clock::now();
    const std::string ref_name = std::string("assets/references/") + test_name + "/ref_noshadow.uncompressed.png";

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
    Sys::ThreadPool threads(4);
    Eng::Renderer renderer(ren_ctx, shader_loader, rand, threads);
    renderer.set_render_flags(Eng::EnableZFill | Eng::EnableCulling | Eng::EnableSSAO | Eng::EnableLightmap |
                              Eng::EnableLights | Eng::EnableDecals | Eng::EnableShadows | Eng::EnableTaa |
                              Eng::EnableTaaStatic | Eng::EnableTimers | Eng::EnableDOF | Eng::EnableDeferred |
                              Eng::EnableHQ_HDR);

    Eng::path_config_t paths;
    Eng::SceneManager scene_manager(ren_ctx, shader_loader, nullptr, threads, paths);

    using namespace std::placeholders;
    scene_manager.SetPipelineInitializer(std::bind(&Eng::Renderer::InitPipelinesForProgram, &renderer, _1, _2, _3, _4));

    JsObjectP js_scene(scene_manager.mp_alloc());

    { // Load scene data from file
        const std::string scene_name = std::string("assets/scenes/") + test_name + ".json";
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
    Ren::BufferRef items_stage_buf =
        ren_ctx.LoadBuffer("Items (Stage)", Ren::eBufType::Stage, Eng::ItemsBufChunkSize * Ren::MaxFramesInFlight);
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
                   shape_keys_stage_buf, cells_stage_buf, items_stage_buf, lights_stage_buf, decals_stage_buf,
                   rt_obj_instances_stage_buf, rt_sh_obj_instances_stage_buf, rt_tlas_nodes_stage_buf,
                   rt_sh_tlas_nodes_stage_buf);
    draw_list.render_flags = renderer.render_flags();

    renderer.PrepareDrawList(scene_manager.scene_data(), scene_manager.main_cam(), draw_list);
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
    for (int i = 0; i < 99; ++i) {
        begin_frame();
        scene_manager.Serve(10);
        end_frame();
    }

    for (int i = 0; i < RendererInternal::TaaSampleCountStatic; ++i) {
        draw_list.Clear();
        renderer.PrepareDrawList(scene_manager.scene_data(), scene_manager.main_cam(), draw_list);

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

    const uint8_t *img_data = stage_buf->Map(Ren::BufMapRead);
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

    printf("Test %s (PSNR: %.2f/%.2f dB, Fireflies: %i/%i, Time: %.2fms)\n", test_name, psnr, min_psnr, error_pixels,
           pix_thres, test_duration_ms);
    require(psnr >= min_psnr && error_pixels <= pix_thres);

    std::lock_guard<std::mutex> _(g_stbi_mutex);

    stbi_flip_vertically_on_write(flip_y);

    const std::string out_name = std::string("assets_pc/references/") + test_name + "/out.png";
    stbi_write_png(out_name.c_str(), ref_w, ref_h, 4, img_data, 4 * ref_w);

    stbi_flip_vertically_on_write(false);

    const std::string diff_name = std::string("assets_pc/references/") + test_name + "/diff.png";
    stbi_write_png(diff_name.c_str(), ref_w, ref_h, 3, diff_data_u8.get(), 3 * ref_w);

    const std::string mask_name = std::string("assets_pc/references/") + test_name + "/mask.png";
    stbi_write_png(mask_name.c_str(), ref_w, ref_h, 3, mask_data_u8.get(), 3 * ref_w);
}

void test_materials(Sys::ThreadPool &threads, const char *device_name, int vl) {
    { // diffuse material
        std::vector<std::future<void>> futures;

        futures.push_back(threads.Enqueue(run_image_test, "diff_mat0", device_name, vl, 40.87, 2));
        futures.push_back(threads.Enqueue(run_image_test, "diff_mat1", device_name, vl, 41.08, 2));
        futures.push_back(threads.Enqueue(run_image_test, "diff_mat2", device_name, vl, 40.47, 2));
        futures.push_back(threads.Enqueue(run_image_test, "diff_mat3", device_name, vl, 40.70, 2));
        futures.push_back(threads.Enqueue(run_image_test, "diff_mat4", device_name, vl, 40.57, 4));
        futures.push_back(threads.Enqueue(run_image_test, "diff_mat5", device_name, vl, 42.31, 1));
        futures.push_back(threads.Enqueue(run_image_test, "diff_mat6", device_name, vl, 42.84, 1));
        futures.push_back(threads.Enqueue(run_image_test, "diff_mat7", device_name, vl, 42.27, 1));
        futures.push_back(threads.Enqueue(run_image_test, "diff_mat8", device_name, vl, 42.39, 1));
        futures.push_back(threads.Enqueue(run_image_test, "diff_mat9", device_name, vl, 41.76, 1));

        for (auto &f : futures) {
            f.wait();
        }
    }
    puts(" ---------------");
    { // sheen material
        std::vector<std::future<void>> futures;

        futures.push_back(threads.Enqueue(run_image_test, "sheen_mat0", device_name, vl, 41.07, 2));
        futures.push_back(threads.Enqueue(run_image_test, "sheen_mat1", device_name, vl, 39.98, 2));
        futures.push_back(threads.Enqueue(run_image_test, "sheen_mat2", device_name, vl, 40.40, 2));
        futures.push_back(threads.Enqueue(run_image_test, "sheen_mat3", device_name, vl, 38.90, 2));
        futures.push_back(threads.Enqueue(run_image_test, "sheen_mat4", device_name, vl, 41.85, 1));
        futures.push_back(threads.Enqueue(run_image_test, "sheen_mat5", device_name, vl, 40.08, 1));
        futures.push_back(threads.Enqueue(run_image_test, "sheen_mat6", device_name, vl, 41.22, 1));
        futures.push_back(threads.Enqueue(run_image_test, "sheen_mat7", device_name, vl, 38.65, 1));

        for (auto &f : futures) {
            f.wait();
        }
    }
    puts(" ---------------");
    { // specular material
        std::vector<std::future<void>> futures;

        futures.push_back(threads.Enqueue(run_image_test, "spec_mat0", device_name, vl, 37.09, 350));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat1", device_name, vl, 29.58, 1367));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat2", device_name, vl, 19.71, 60386));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat3", device_name, vl, 22.81, 22731));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat4", device_name, vl, 40.23, 2));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat5", device_name, vl, 35.74, 163));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat6", device_name, vl, 29.52, 3036));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat7", device_name, vl, 21.12, 55928));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat8", device_name, vl, 24.33, 17635));
        futures.push_back(threads.Enqueue(run_image_test, "spec_mat9", device_name, vl, 31.21, 1661));

        for (auto &f : futures) {
            f.wait();
        }
    }
    puts(" ---------------");
    { // metal material
        std::vector<std::future<void>> futures;

        futures.push_back(threads.Enqueue(run_image_test, "metal_mat0", device_name, vl, 32.31, 1482));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat1", device_name, vl, 31.10, 3798));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat2", device_name, vl, 23.69, 53829));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat3", device_name, vl, 26.55, 21852));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat4", device_name, vl, 36.56, 4));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat5", device_name, vl, 37.24, 166));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat6", device_name, vl, 33.90, 1517));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat7", device_name, vl, 25.38, 19304));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat8", device_name, vl, 28.10, 15170));
        futures.push_back(threads.Enqueue(run_image_test, "metal_mat9", device_name, vl, 36.33, 400));

        for (auto &f : futures) {
            f.wait();
        }
    }
    puts(" ---------------");
    { // plastic material
        std::vector<std::future<void>> futures;

        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat0", device_name, vl, 39.24, 285));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat1", device_name, vl, 39.49, 2));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat2", device_name, vl, 35.38, 614));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat3", device_name, vl, 36.58, 1105));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat4", device_name, vl, 39.60, 5));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat5", device_name, vl, 37.41, 143));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat6", device_name, vl, 39.97, 1));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat7", device_name, vl, 35.45, 1162));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat8", device_name, vl, 36.29, 2076));
        futures.push_back(threads.Enqueue(run_image_test, "plastic_mat9", device_name, vl, 40.78, 1));

        for (auto &f : futures) {
            f.wait();
        }
    }
    puts(" ---------------");
    { // tint material
        std::vector<std::future<void>> futures;

        futures.push_back(threads.Enqueue(run_image_test, "tint_mat0", device_name, vl, 39.63, 223));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat1", device_name, vl, 37.64, 2));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat2", device_name, vl, 28.09, 17015));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat3", device_name, vl, 33.67, 4152));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat4", device_name, vl, 37.19, 3));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat5", device_name, vl, 41.00, 105));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat6", device_name, vl, 36.88, 1133));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat7", device_name, vl, 27.64, 17402));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat8", device_name, vl, 32.83, 6089));
        futures.push_back(threads.Enqueue(run_image_test, "tint_mat9", device_name, vl, 37.38, 1));

        for (auto &f : futures) {
            f.wait();
        }
    }
    puts(" ---------------");
    { // clearcoat material
        std::vector<std::future<void>> futures;

        futures.push_back(threads.Enqueue(run_image_test, "coat_mat0", device_name, vl, 38.88, 347));
        futures.push_back(threads.Enqueue(run_image_test, "coat_mat1", device_name, vl, 30.54, 1094));
        futures.push_back(threads.Enqueue(run_image_test, "coat_mat2", device_name, vl, 31.63, 863));
        futures.push_back(threads.Enqueue(run_image_test, "coat_mat3", device_name, vl, 31.25, 1515));
        futures.push_back(threads.Enqueue(run_image_test, "coat_mat4", device_name, vl, 29.28, 3187));
        futures.push_back(threads.Enqueue(run_image_test, "coat_mat5", device_name, vl, 38.16, 291));
        futures.push_back(threads.Enqueue(run_image_test, "coat_mat6", device_name, vl, 28.87, 2671));
        futures.push_back(threads.Enqueue(run_image_test, "coat_mat7", device_name, vl, 31.04, 1924));
        futures.push_back(threads.Enqueue(run_image_test, "coat_mat8", device_name, vl, 30.33, 2887));
        futures.push_back(threads.Enqueue(run_image_test, "coat_mat9", device_name, vl, 28.37, 4826));

        for (auto &f : futures) {
            f.wait();
        }
    }
}
