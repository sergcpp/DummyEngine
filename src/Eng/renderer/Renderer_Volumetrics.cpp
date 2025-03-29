#include "Renderer.h"

#include <Ren/Context.h>
#include <Sys/AssetFile.h>

#include "../utils/Load.h"
#include "Renderer_Names.h"
#include "executors/ExSkydome.h"

#include "shaders/blit_fxaa_interface.h"
#include "shaders/skydome_interface.h"
#include "shaders/sun_brightness_interface.h"
#include "shaders/vol_interface.h"

void Eng::Renderer::InitSkyResources() {
    if (p_list_->env.env_map_name != "physical_sky") {
        // release resources
        sky_transmittance_lut_ = {};
        sky_multiscatter_lut_ = {};
        sky_moon_tex_ = {};
        sky_weather_tex_ = {};
        sky_cirrus_tex_ = {};
        sky_curl_tex_ = {};
        sky_noise3d_tex_ = {};
    } else {
        if (!sky_transmittance_lut_) {
            const std::vector<Ren::Vec4f> transmittance_lut = Generate_SkyTransmittanceLUT(p_list_->env.atmosphere);
            { // Init transmittance LUT
                Ren::TexParams p;
                p.w = SKY_TRANSMITTANCE_LUT_W;
                p.h = SKY_TRANSMITTANCE_LUT_H;
                p.format = Ren::eTexFormat::RGBA32F;
                p.usage = Ren::Bitmask(Ren::eTexUsage::Transfer) | Ren::eTexUsage::Sampled;
                p.sampling.filter = Ren::eTexFilter::Bilinear;
                p.sampling.wrap = Ren::eTexWrap::ClampToEdge;

                Ren::eTexLoadStatus status;
                sky_transmittance_lut_ = ctx_.LoadTexture(
                    "Sky Transmittance LUT",
                    Ren::Span{(const uint8_t *)transmittance_lut.data(), transmittance_lut.size() * sizeof(Ren::Vec4f)},
                    p, ctx_.default_mem_allocs(), &status);
                assert(status == Ren::eTexLoadStatus::CreatedFromData);
            }
            { // Init multiscatter LUT
                const std::vector<Ren::Vec4f> multiscatter_lut =
                    Generate_SkyMultiscatterLUT(p_list_->env.atmosphere, transmittance_lut);

                Ren::TexParams p;
                p.w = p.h = SKY_MULTISCATTER_LUT_RES;
                p.format = Ren::eTexFormat::RGBA32F;
                p.usage = Ren::Bitmask(Ren::eTexUsage::Transfer) | Ren::eTexUsage::Sampled;
                p.sampling.filter = Ren::eTexFilter::Bilinear;
                p.sampling.wrap = Ren::eTexWrap::ClampToEdge;

                Ren::eTexLoadStatus status;
                sky_multiscatter_lut_ = ctx_.LoadTexture(
                    "Sky Multiscatter LUT",
                    Ren::Span{(const uint8_t *)multiscatter_lut.data(), multiscatter_lut.size() * sizeof(Ren::Vec4f)},
                    p, ctx_.default_mem_allocs(), &status);
                assert(status == Ren::eTexLoadStatus::CreatedFromData);
            }
            { // Init Moon texture
                const std::string_view moon_diff = "assets_pc/textures/internal/moon_diff.dds";

                Ren::TexParams p;
                p.usage = Ren::Bitmask(Ren::eTexUsage::Transfer) | Ren::eTexUsage::Sampled;
                p.sampling.filter = Ren::eTexFilter::Bilinear;
                p.sampling.wrap = Ren::eTexWrap::Repeat;

                const std::vector<uint8_t> data = LoadDDS(moon_diff, &p);
                if (!data.empty()) {
                    Ren::eTexLoadStatus status;
                    sky_moon_tex_ = ctx_.LoadTexture(moon_diff, data, p, ctx_.default_mem_allocs(), &status);
                    assert(status == Ren::eTexLoadStatus::CreatedFromData);
                } else {
                    ctx_.log()->Error("Failed to load %s", moon_diff.data());
                }
            }
            { // Init Weather texture
                const std::string_view weather = "assets_pc/textures/internal/weather.dds";

                Ren::TexParams p;
                p.usage = Ren::Bitmask(Ren::eTexUsage::Transfer) | Ren::eTexUsage::Sampled;
                p.sampling.filter = Ren::eTexFilter::Bilinear;
                p.sampling.wrap = Ren::eTexWrap::Repeat;

                const std::vector<uint8_t> data = LoadDDS(weather, &p);
                if (!data.empty()) {
                    Ren::eTexLoadStatus status;
                    sky_weather_tex_ = ctx_.LoadTexture(weather, data, p, ctx_.default_mem_allocs(), &status);
                    assert(status == Ren::eTexLoadStatus::CreatedFromData);
                } else {
                    ctx_.log()->Error("Failed to load %s", weather.data());
                }
            }
            { // Init Cirrus texture
                const std::string_view cirrus = "assets_pc/textures/internal/cirrus.dds";

                Ren::TexParams p;
                p.usage = Ren::Bitmask(Ren::eTexUsage::Transfer) | Ren::eTexUsage::Sampled;
                p.sampling.filter = Ren::eTexFilter::Bilinear;
                p.sampling.wrap = Ren::eTexWrap::Repeat;

                const std::vector<uint8_t> data = LoadDDS(cirrus, &p);
                if (!data.empty()) {
                    Ren::eTexLoadStatus status;
                    sky_cirrus_tex_ = ctx_.LoadTexture(cirrus, data, p, ctx_.default_mem_allocs(), &status);
                    assert(status == Ren::eTexLoadStatus::CreatedFromData);
                } else {
                    ctx_.log()->Error("Failed to load %s", cirrus.data());
                }
            }
            { // Init Curl texture
                const std::string_view curl = "assets_pc/textures/internal/curl.dds";

                Ren::TexParams p;
                // p.flags = Ren::eTexFlags::SRGB;
                p.usage = Ren::Bitmask(Ren::eTexUsage::Transfer) | Ren::eTexUsage::Sampled;
                p.sampling.filter = Ren::eTexFilter::Bilinear;
                p.sampling.wrap = Ren::eTexWrap::Repeat;

                const std::vector<uint8_t> data = LoadDDS(curl, &p);
                assert(p.format == Ren::eTexFormat::RGBA8);
                p.format = Ren::eTexFormat::RGBA8_srgb;

                if (!data.empty()) {
                    Ren::eTexLoadStatus status;
                    sky_curl_tex_ = ctx_.LoadTexture(curl, data, p, ctx_.default_mem_allocs(), &status);
                    assert(status == Ren::eTexLoadStatus::CreatedFromData);
                } else {
                    ctx_.log()->Error("Failed to load %s", curl.data());
                }
            }
            { // Init 3d noise texture
                const std::string_view noise = "assets_pc/textures/internal/3dnoise.dds";

                Ren::TexParams p;
                p.usage = Ren::Bitmask(Ren::eTexUsage::Sampled) | Ren::eTexUsage::Transfer;
                p.sampling.filter = Ren::eTexFilter::Bilinear;
                p.sampling.wrap = Ren::eTexWrap::Repeat;

                const std::vector<uint8_t> data = LoadDDS(noise, &p);
                if (!data.empty()) {
                    Ren::eTexLoadStatus status;
                    sky_noise3d_tex_ = ctx_.LoadTexture(noise, data, p, ctx_.default_mem_allocs(), &status);
                    assert(status == Ren::eTexLoadStatus::CreatedFromData);
                } else {
                    ctx_.log()->Error("Failed to load %s", noise.data());
                }
            }
        }
    }
}

void Eng::Renderer::AddSkydomePass(const CommonBuffers &common_buffers, FrameTextures &frame_textures) {
    using Stg = Ren::eStage;
    using Trg = Ren::eBindTarget;

    // TODO: Remove this condition
    if (!p_list_->env.env_map) {
        return;
    }

    if (p_list_->env.env_map_name == "physical_sky") {
        auto &skydome_cube = fg_builder_.AddNode("SKYDOME CUBE");

        auto *data = skydome_cube.AllocNodeData<ExSkydomeCube::Args>();

        data->shared_data = skydome_cube.AddUniformBufferInput(common_buffers.shared_data,
                                                               Ren::Bitmask{Stg::VertexShader} | Stg::FragmentShader);

        data->transmittance_lut = skydome_cube.AddTextureInput(sky_transmittance_lut_, Stg::FragmentShader);
        data->multiscatter_lut = skydome_cube.AddTextureInput(sky_multiscatter_lut_, Stg::FragmentShader);
        data->moon_tex = skydome_cube.AddTextureInput(sky_moon_tex_, Stg::FragmentShader);
        data->weather_tex = skydome_cube.AddTextureInput(sky_weather_tex_, Stg::FragmentShader);
        data->cirrus_tex = skydome_cube.AddTextureInput(sky_cirrus_tex_, Stg::FragmentShader);
        data->curl_tex = skydome_cube.AddTextureInput(sky_curl_tex_, Stg::FragmentShader);
        data->noise3d_tex = skydome_cube.AddTextureInput(sky_noise3d_tex_, Stg::FragmentShader);
        frame_textures.envmap = data->color_tex = skydome_cube.AddColorOutput(p_list_->env.env_map);

        skydome_cube.make_executor<ExSkydomeCube>(prim_draw_, &view_state_, data);
    }

    FgResRef sky_temp;
    { // Main pass
        auto &skymap = fg_builder_.AddNode("SKYDOME");

        auto *data = skymap.AllocNodeData<ExSkydomeScreen::Args>();
        data->shared_data = skymap.AddUniformBufferInput(common_buffers.shared_data,
                                                         Ren::Bitmask{Stg::VertexShader} | Stg::FragmentShader);
        if (p_list_->env.env_map_name != "physical_sky") {
            frame_textures.envmap = data->env_tex = skymap.AddTextureInput(p_list_->env.env_map, Stg::FragmentShader);
            frame_textures.color = data->color_tex = skymap.AddColorOutput(MAIN_COLOR_TEX, frame_textures.color_params);
            frame_textures.depth = data->depth_tex = skymap.AddDepthOutput(MAIN_DEPTH_TEX, frame_textures.depth_params);
        } else {
            data->sky_quality = settings.sky_quality;
            frame_textures.envmap = data->env_tex = skymap.AddTextureInput(frame_textures.envmap, Stg::FragmentShader);
            data->phys.transmittance_lut = skymap.AddTextureInput(sky_transmittance_lut_, Stg::FragmentShader);
            data->phys.multiscatter_lut = skymap.AddTextureInput(sky_multiscatter_lut_, Stg::FragmentShader);
            data->phys.moon_tex = skymap.AddTextureInput(sky_moon_tex_, Stg::FragmentShader);
            data->phys.weather_tex = skymap.AddTextureInput(sky_weather_tex_, Stg::FragmentShader);
            data->phys.cirrus_tex = skymap.AddTextureInput(sky_cirrus_tex_, Stg::FragmentShader);
            data->phys.curl_tex = skymap.AddTextureInput(sky_curl_tex_, Stg::FragmentShader);
            data->phys.noise3d_tex = skymap.AddTextureInput(sky_noise3d_tex_, Stg::FragmentShader);

            if (settings.sky_quality == eSkyQuality::High) {
                frame_textures.depth = data->depth_tex =
                    skymap.AddTextureInput(frame_textures.depth, Stg::FragmentShader);

                Ren::TexParams params;
                params.w = (view_state_.scr_res[0] + 3) / 4;
                params.h = (view_state_.scr_res[1] + 3) / 4;
                params.format = Ren::eTexFormat::RGBA16F;
                params.sampling.filter = Ren::eTexFilter::Bilinear;
                params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

                sky_temp = data->color_tex = skymap.AddColorOutput("SKY TEMP", params);
            } else {
                frame_textures.color = data->color_tex =
                    skymap.AddColorOutput(MAIN_COLOR_TEX, frame_textures.color_params);
                frame_textures.depth = data->depth_tex =
                    skymap.AddDepthOutput(MAIN_DEPTH_TEX, frame_textures.depth_params);
            }
        }

        skymap.make_executor<ExSkydomeScreen>(prim_draw_, &view_state_, data);
    }

    if (p_list_->env.env_map_name != "physical_sky") {
        return;
    }

    if (settings.sky_quality == eSkyQuality::High) {
        FgResRef sky_upsampled;
        { // prepare upsampled texture
            auto &sky_upsample = fg_builder_.AddNode("SKY UPSAMPLE");

            struct PassData {
                FgResRef shared_data;
                FgResRef env_map;
                FgResRef sky_temp_tex;
                FgResRef sky_hist_tex;
                FgResRef output_tex;
            };

            auto *data = sky_upsample.AllocNodeData<PassData>();
            data->shared_data = sky_upsample.AddUniformBufferInput(common_buffers.shared_data, Stg::ComputeShader);
            frame_textures.envmap = data->env_map =
                sky_upsample.AddTextureInput(frame_textures.envmap, Stg::ComputeShader);
            data->sky_temp_tex = sky_upsample.AddTextureInput(sky_temp, Stg::ComputeShader);

            Ren::TexParams params;
            params.w = view_state_.scr_res[0];
            params.h = view_state_.scr_res[1];
            params.format = Ren::eTexFormat::RGBA16F;
            params.sampling.filter = Ren::eTexFilter::Bilinear;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

            sky_upsampled = data->output_tex =
                sky_upsample.AddStorageImageOutput("SKY HIST", params, Stg::ComputeShader);
            data->sky_hist_tex = sky_upsample.AddHistoryTextureInput(sky_upsampled, Stg::ComputeShader);

            sky_upsample.set_execute_cb([data, this](FgBuilder &builder) {
                FgAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(data->shared_data);
                FgAllocTex &env_map_tex = builder.GetReadTexture(data->env_map);
                FgAllocTex &sky_temp_tex = builder.GetReadTexture(data->sky_temp_tex);
                FgAllocTex &sky_hist_tex = builder.GetReadTexture(data->sky_hist_tex);
                FgAllocTex &output_tex = builder.GetWriteTexture(data->output_tex);

                const Ren::Binding bindings[] = {{Trg::UBuf, BIND_UB_SHARED_DATA_BUF, *unif_sh_data_buf.ref},
                                                 {Trg::TexSampled, Skydome::ENV_TEX_SLOT, *env_map_tex.ref},
                                                 {Trg::TexSampled, Skydome::SKY_TEX_SLOT, *sky_temp_tex.ref},
                                                 {Trg::TexSampled, Skydome::SKY_HIST_TEX_SLOT, *sky_hist_tex.ref},
                                                 {Trg::ImageRW, Skydome::OUT_IMG_SLOT, *output_tex.ref}};

                const Ren::Vec3u grp_count = Ren::Vec3u{
                    (view_state_.act_res[0] + Skydome::LOCAL_GROUP_SIZE_X - 1u) / Skydome::LOCAL_GROUP_SIZE_X,
                    (view_state_.act_res[1] + Skydome::LOCAL_GROUP_SIZE_Y - 1u) / Skydome::LOCAL_GROUP_SIZE_Y, 1u};

                Skydome::Params2 uniform_params;
                uniform_params.img_size = view_state_.scr_res;
                uniform_params.sample_coord = ExSkydomeScreen::sample_pos(view_state_.frame_index);
                uniform_params.hist_weight = (view_state_.pre_exposure / view_state_.prev_pre_exposure);

                DispatchCompute(*pi_sky_upsample_, grp_count, bindings, &uniform_params, sizeof(uniform_params),
                                builder.ctx().default_descr_alloc(), builder.ctx().log());
            });
        }
        { // blit result
            auto &sky_blit = fg_builder_.AddNode("SKY BLIT");

            struct PassData {
                FgResRef depth_tex;
                FgResRef sky_tex;
                FgResRef output_tex;
            };

            auto *data = sky_blit.AllocNodeData<PassData>();
            frame_textures.depth = data->depth_tex = sky_blit.AddDepthOutput(frame_textures.depth);
            data->sky_tex = sky_blit.AddTextureInput(sky_upsampled, Stg::FragmentShader);

            frame_textures.color = data->output_tex =
                sky_blit.AddColorOutput(MAIN_COLOR_TEX, frame_textures.color_params);

            sky_blit.set_execute_cb([data, this](FgBuilder &builder) {
                FgAllocTex &sky_tex = builder.GetReadTexture(data->sky_tex);
                FgAllocTex &depth_tex = builder.GetWriteTexture(data->depth_tex);
                FgAllocTex &output_tex = builder.GetWriteTexture(data->output_tex);

                Ren::RastState rast_state;
                rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);
                rast_state.depth.write_enabled = false;
                rast_state.depth.test_enabled = true;
                rast_state.depth.compare_op = uint8_t(Ren::eCompareOp::Equal);

                rast_state.viewport[2] = view_state_.act_res[0];
                rast_state.viewport[3] = view_state_.act_res[1];

                const Ren::Binding bindings[] = {{Trg::TexSampled, FXAA::INPUT_TEX_SLOT, *sky_tex.ref}};

                FXAA::Params uniform_params;
                uniform_params.transform = Ren::Vec4f{0.0f, 0.0f, 1.0f, 1.0f};
                uniform_params.inv_resolution =
                    1.0f / Ren::Vec2f{float(view_state_.act_res[0]), float(view_state_.act_res[1])};

                const Ren::RenderTarget depth_target = {depth_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store};
                const Ren::RenderTarget render_targets[] = {{output_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store}};

                prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, blit_fxaa_prog_, depth_target, render_targets, rast_state,
                                    builder.rast_state(), bindings, &uniform_params, sizeof(FXAA::Params), 0);
            });
        }
    }
}

void Eng::Renderer::AddSunColorUpdatePass(CommonBuffers &common_buffers) {
    using Stg = Ren::eStage;
    using Trg = Ren::eBindTarget;

    if (p_list_->env.env_map_name != "physical_sky") {
        return;
    }

    FgResRef output;
    { // Sample sun color
        auto &sun_color = fg_builder_.AddNode("SUN COLOR SAMPLE");

        struct PassData {
            FgResRef shared_data;
            FgResRef transmittance_lut;
            FgResRef multiscatter_lut;
            FgResRef moon_tex;
            FgResRef weather_tex;
            FgResRef cirrus_tex;
            FgResRef noise3d_tex;
            FgResRef output_buf;
        };

        auto *data = sun_color.AllocNodeData<PassData>();

        data->shared_data = sun_color.AddUniformBufferInput(common_buffers.shared_data, Stg::ComputeShader);
        data->transmittance_lut = sun_color.AddTextureInput(sky_transmittance_lut_, Stg::ComputeShader);
        data->multiscatter_lut = sun_color.AddTextureInput(sky_multiscatter_lut_, Stg::ComputeShader);
        data->moon_tex = sun_color.AddTextureInput(sky_moon_tex_, Stg::ComputeShader);
        data->weather_tex = sun_color.AddTextureInput(sky_weather_tex_, Stg::ComputeShader);
        data->cirrus_tex = sun_color.AddTextureInput(sky_cirrus_tex_, Stg::ComputeShader);
        data->noise3d_tex = sun_color.AddTextureInput(sky_noise3d_tex_, Stg::ComputeShader);

        FgBufDesc desc = {};
        desc.type = Ren::eBufType::Storage;
        desc.size = 8 * sizeof(float);
        output = data->output_buf = sun_color.AddStorageOutput("Sun Brightness Result", desc, Stg::ComputeShader);

        sun_color.set_execute_cb([data, this](FgBuilder &builder) {
            FgAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(data->shared_data);
            FgAllocTex &transmittance_lut = builder.GetReadTexture(data->transmittance_lut);
            FgAllocTex &multiscatter_lut = builder.GetReadTexture(data->multiscatter_lut);
            FgAllocTex &moon_tex = builder.GetReadTexture(data->moon_tex);
            FgAllocTex &weather_tex = builder.GetReadTexture(data->weather_tex);
            FgAllocTex &cirrus_tex = builder.GetReadTexture(data->cirrus_tex);
            FgAllocTex &noise3d_tex = builder.GetReadTexture(data->noise3d_tex);
            FgAllocBuf &output_buf = builder.GetWriteBuffer(data->output_buf);

            const Ren::Binding bindings[] = {
                {Trg::UBuf, BIND_UB_SHARED_DATA_BUF, *unif_sh_data_buf.ref},
                {Trg::TexSampled, SunBrightness::TRANSMITTANCE_LUT_SLOT, *transmittance_lut.ref},
                {Trg::TexSampled, SunBrightness::MULTISCATTER_LUT_SLOT, *multiscatter_lut.ref},
                {Trg::TexSampled, SunBrightness::MOON_TEX_SLOT, *moon_tex.ref},
                {Trg::TexSampled, SunBrightness::WEATHER_TEX_SLOT, *weather_tex.ref},
                {Trg::TexSampled, SunBrightness::CIRRUS_TEX_SLOT, *cirrus_tex.ref},
                {Trg::TexSampled, SunBrightness::NOISE3D_TEX_SLOT, *noise3d_tex.ref},
                {Trg::SBufRW, SunBrightness::OUT_BUF_SLOT, *output_buf.ref}};

            DispatchCompute(*pi_sun_brightness_, Ren::Vec3u{1u, 1u, 1u}, bindings, nullptr, 0,
                            builder.ctx().default_descr_alloc(), builder.ctx().log());
        });
    }
    { // Update sun color
        auto &sun_color = fg_builder_.AddNode("SUN COLOR UPDATE");

        struct PassData {
            FgResRef sample_buf;
            FgResRef shared_data;
        };

        auto *data = sun_color.AllocNodeData<PassData>();

        data->sample_buf = sun_color.AddTransferInput(output);
        common_buffers.shared_data = data->shared_data = sun_color.AddTransferOutput(common_buffers.shared_data);

        sun_color.set_execute_cb([data](FgBuilder &builder) {
            FgAllocBuf &sample_buf = builder.GetReadBuffer(data->sample_buf);
            FgAllocBuf &unif_sh_data_buf = builder.GetWriteBuffer(data->shared_data);

            CopyBufferToBuffer(*sample_buf.ref, 0, *unif_sh_data_buf.ref, offsetof(shared_data_t, sun_col),
                               3 * sizeof(float), builder.ctx().current_cmd_buf());
            CopyBufferToBuffer(*sample_buf.ref, 4 * sizeof(float), *unif_sh_data_buf.ref,
                               offsetof(shared_data_t, sun_col_point_sh), 3 * sizeof(float),
                               builder.ctx().current_cmd_buf());
        });
    }
}

void Eng::Renderer::AddVolumetricPasses(const CommonBuffers &common_buffers, FrameTextures &frame_textures) {
    using Stg = Ren::eStage;
    using Trg = Ren::eBindTarget;

    const int TileSize = (p_list_->render_settings.vol_quality == Eng::eVolQuality::Ultra) ? 8 : 12;
    const bool AllCascades = (p_list_->render_settings.vol_quality == Eng::eVolQuality::Ultra);

    FgResRef froxel_tex;
    { // Scatter
        auto &scatter = fg_builder_.AddNode("VOL SCATTER");

        struct PassData {
            FgResRef shared_data;
            FgResRef random_seq;
            FgResRef shadow_depth_tex, shadow_color_tex;
            FgResRef froxels_hist_tex;
            FgResRef cells_buf, items_buf, lights_buf, decals_buf;
            FgResRef envmap_tex;
            FgResRef irradiance_tex, distance_tex, offset_tex;
            FgResRef output_tex;
        };

        auto *data = scatter.AllocNodeData<PassData>();
        data->shared_data = scatter.AddUniformBufferInput(common_buffers.shared_data, Stg::ComputeShader);
        data->random_seq = scatter.AddStorageReadonlyInput(pmj_samples_buf_, Stg::ComputeShader);
        data->shadow_depth_tex = scatter.AddTextureInput(frame_textures.shadow_depth, Stg::ComputeShader);
        data->shadow_color_tex = scatter.AddTextureInput(frame_textures.shadow_color, Stg::ComputeShader);

        { //
            Ren::TexParams p;
            p.w = (view_state_.act_res[0] + TileSize - 1) / TileSize;
            p.h = (view_state_.act_res[1] + TileSize - 1) / TileSize;
            p.d = 144;
            p.format = Ren::eTexFormat::RGBA16F;
            p.sampling.filter = Ren::eTexFilter::Bilinear;
            p.sampling.wrap = Ren::eTexWrap::ClampToEdge;

            froxel_tex = data->output_tex = scatter.AddStorageImageOutput("Vol Scattering", p, Stg::ComputeShader);
        }

        data->froxels_hist_tex = scatter.AddHistoryTextureInput(froxel_tex, Stg::ComputeShader);

        data->cells_buf = scatter.AddStorageReadonlyInput(common_buffers.cells, Stg::ComputeShader);
        data->items_buf = scatter.AddStorageReadonlyInput(common_buffers.items, Stg::ComputeShader);
        data->lights_buf = scatter.AddStorageReadonlyInput(common_buffers.lights, Stg::ComputeShader);
        data->decals_buf = scatter.AddStorageReadonlyInput(common_buffers.decals, Stg::ComputeShader);
        data->envmap_tex = scatter.AddTextureInput(frame_textures.envmap, Stg::ComputeShader);

        if (settings.gi_quality != eGIQuality::Off) {
            data->irradiance_tex = scatter.AddTextureInput(frame_textures.gi_cache_irradiance, Stg::ComputeShader);
            data->distance_tex = scatter.AddTextureInput(frame_textures.gi_cache_distance, Stg::ComputeShader);
            data->offset_tex = scatter.AddTextureInput(frame_textures.gi_cache_offset, Stg::ComputeShader);
        }

        scatter.set_execute_cb([data, this, AllCascades](FgBuilder &builder) {
            FgAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(data->shared_data);
            FgAllocBuf &random_seq_buf = builder.GetReadBuffer(data->random_seq);

            FgAllocTex &shad_depth_tex = builder.GetReadTexture(data->shadow_depth_tex);
            FgAllocTex &shad_color_tex = builder.GetReadTexture(data->shadow_color_tex);

            FgAllocTex &froxels_hist_tex = builder.GetReadTexture(data->froxels_hist_tex);

            FgAllocBuf &cells_buf = builder.GetReadBuffer(data->cells_buf);
            FgAllocBuf &items_buf = builder.GetReadBuffer(data->items_buf);
            FgAllocBuf &lights_buf = builder.GetReadBuffer(data->lights_buf);
            FgAllocBuf &decals_buf = builder.GetReadBuffer(data->decals_buf);
            FgAllocTex &envmap_tex = builder.GetReadTexture(data->envmap_tex);

            FgAllocTex *irr_tex = nullptr, *dist_tex = nullptr, *off_tex = nullptr;
            if (data->irradiance_tex) {
                irr_tex = &builder.GetReadTexture(data->irradiance_tex);
                dist_tex = &builder.GetReadTexture(data->distance_tex);
                off_tex = &builder.GetReadTexture(data->offset_tex);
            }

            FgAllocTex &output_tex = builder.GetWriteTexture(data->output_tex);

            if (p_list_->env.fog.density == 0.0f) {
                return;
            }

            Ren::SmallVector<Ren::Binding, 16> bindings = {
                {Trg::UBuf, BIND_UB_SHARED_DATA_BUF, *unif_sh_data_buf.ref},
                {Trg::UTBuf, Fog::RANDOM_SEQ_BUF_SLOT, *random_seq_buf.ref},
                {Trg::TexSampled, Fog::SHADOW_DEPTH_TEX_SLOT, *shad_depth_tex.ref},
                {Trg::TexSampled, Fog::SHADOW_COLOR_TEX_SLOT, *shad_color_tex.ref},
                {Trg::TexSampled, Fog::FROXELS_TEX_SLOT, *froxels_hist_tex.ref},
                {Trg::UTBuf, Fog::CELLS_BUF_SLOT, *cells_buf.ref},
                {Trg::UTBuf, Fog::ITEMS_BUF_SLOT, *items_buf.ref},
                {Trg::UTBuf, Fog::LIGHT_BUF_SLOT, *lights_buf.ref},
                {Trg::UTBuf, Fog::DECAL_BUF_SLOT, *decals_buf.ref},
                {Trg::TexSampled, Fog::ENVMAP_TEX_SLOT, *envmap_tex.ref},
                {Trg::ImageRW, Fog::OUT_FROXELS_IMG_SLOT, *output_tex.ref}};
            if (irr_tex) {
                bindings.emplace_back(Ren::eBindTarget::TexSampled, Fog::IRRADIANCE_TEX_SLOT, *irr_tex->ref);
                bindings.emplace_back(Ren::eBindTarget::TexSampled, Fog::DISTANCE_TEX_SLOT, *dist_tex->ref);
                bindings.emplace_back(Ren::eBindTarget::TexSampled, Fog::OFFSET_TEX_SLOT, *off_tex->ref);
            }

            const auto froxel_res =
                Ren::Vec4i{output_tex.ref->params.w, output_tex.ref->params.h, output_tex.ref->params.d, 0};

            const Ren::Vec3u grp_count =
                Ren::Vec3u{(froxel_res[0] + Fog::LOCAL_GROUP_SIZE_X - 1u) / Fog::LOCAL_GROUP_SIZE_X,
                           (froxel_res[1] + Fog::LOCAL_GROUP_SIZE_Y - 1u) / Fog::LOCAL_GROUP_SIZE_Y, froxel_res[2]};

            Fog::Params uniform_params;
            uniform_params.froxel_res = froxel_res;
            uniform_params.color = Ren::Saturate(Ren::Vec4f{p_list_->env.fog.color});
            uniform_params.density = std::max(p_list_->env.fog.density, 0.0f);
            uniform_params.anisotropy = Ren::Clamp(p_list_->env.fog.anisotropy, -0.99f, 0.99f);
            uniform_params.bbox_min = Ren::Vec4f{p_list_->env.fog.bbox_min};
            uniform_params.bbox_max = Ren::Vec4f{p_list_->env.fog.bbox_max};
            uniform_params.frame_index = view_state_.frame_index;
            uniform_params.hist_weight = (view_state_.pre_exposure / view_state_.prev_pre_exposure);

            DispatchCompute(*pi_vol_scatter_[AllCascades][irr_tex != nullptr], grp_count, bindings, &uniform_params,
                            sizeof(uniform_params), builder.ctx().default_descr_alloc(), builder.ctx().log());
        });
    }
    { // Ray march
        auto &ray_march = fg_builder_.AddNode("VOL RAY MARCH");

        struct PassData {
            FgResRef shared_data;
            FgResRef froxel_tex;
            FgResRef output_tex;
        };

        auto *data = ray_march.AllocNodeData<PassData>();
        data->shared_data = ray_march.AddUniformBufferInput(common_buffers.shared_data, Stg::ComputeShader);
        data->froxel_tex = ray_march.AddTextureInput(froxel_tex, Stg::ComputeShader);

        { //
            Ren::TexParams p;
            p.w = (view_state_.act_res[0] + TileSize - 1) / TileSize;
            p.h = (view_state_.act_res[1] + TileSize - 1) / TileSize;
            p.d = 144;
            p.format = Ren::eTexFormat::RGBA16F;
            p.sampling.filter = Ren::eTexFilter::Bilinear;
            p.sampling.wrap = Ren::eTexWrap::ClampToEdge;

            froxel_tex = data->output_tex =
                ray_march.AddStorageImageOutput("Vol Scattering Final", p, Stg::ComputeShader);
        }

        ray_march.set_execute_cb([data, this](FgBuilder &builder) {
            FgAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(data->shared_data);
            FgAllocTex &froxel_tex = builder.GetReadTexture(data->froxel_tex);

            FgAllocTex &output_tex = builder.GetWriteTexture(data->output_tex);

            if (p_list_->env.fog.density == 0.0f) {
                return;
            }

            const Ren::Binding bindings[] = {{Trg::UBuf, BIND_UB_SHARED_DATA_BUF, *unif_sh_data_buf.ref},
                                             {Trg::TexSampled, Fog::FROXELS_TEX_SLOT, *froxel_tex.ref},
                                             {Trg::ImageRW, Fog::OUT_FROXELS_IMG_SLOT, *output_tex.ref}};

            const auto froxel_res =
                Ren::Vec4i{output_tex.ref->params.w, output_tex.ref->params.h, output_tex.ref->params.d, 0};

            const Ren::Vec3u grp_count =
                Ren::Vec3u{(froxel_res[0] + Fog::LOCAL_GROUP_SIZE_X - 1u) / Fog::LOCAL_GROUP_SIZE_X,
                           (froxel_res[1] + Fog::LOCAL_GROUP_SIZE_Y - 1u) / Fog::LOCAL_GROUP_SIZE_Y, 1};

            Fog::Params uniform_params;
            uniform_params.froxel_res = froxel_res;

            DispatchCompute(*pi_vol_ray_march_, grp_count, bindings, &uniform_params, sizeof(uniform_params),
                            builder.ctx().default_descr_alloc(), builder.ctx().log());
        });
    }
    { // Apply
        auto &apply = fg_builder_.AddNode("VOL APPLY");

        struct PassData {
            FgResRef shared_data;
            FgResRef depth_tex;
            FgResRef froxel_tex;
            FgResRef output_tex;
        };

        auto *data = apply.AllocNodeData<PassData>();
        data->shared_data = apply.AddUniformBufferInput(common_buffers.shared_data, Stg::ComputeShader);
        data->depth_tex = apply.AddTextureInput(frame_textures.depth, Stg::ComputeShader);
        data->froxel_tex = apply.AddTextureInput(froxel_tex, Stg::ComputeShader);

        frame_textures.color = data->output_tex = apply.AddStorageImageOutput(frame_textures.color, Stg::ComputeShader);

        apply.set_execute_cb([data, this](FgBuilder &builder) {
            FgAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(data->shared_data);

            FgAllocTex &depth_tex = builder.GetReadTexture(data->depth_tex);
            FgAllocTex &froxel_tex = builder.GetReadTexture(data->froxel_tex);
            FgAllocTex &output_tex = builder.GetWriteTexture(data->output_tex);

            if (p_list_->env.fog.density == 0.0f) {
                return;
            }

            const Ren::Binding bindings[] = {{Trg::UBuf, BIND_UB_SHARED_DATA_BUF, *unif_sh_data_buf.ref},
                                             {Trg::TexSampled, Fog::DEPTH_TEX_SLOT, {*depth_tex.ref, 1}},
                                             {Trg::TexSampled, Fog::FROXELS_TEX_SLOT, *froxel_tex.ref},
                                             {Trg::ImageRW, Fog::INOUT_COLOR_IMG_SLOT, *output_tex.ref}};

            const auto img_res = Ren::Vec2i{depth_tex.ref->params.w, depth_tex.ref->params.h};
            const auto froxel_res =
                Ren::Vec4i{froxel_tex.ref->params.w, froxel_tex.ref->params.h, froxel_tex.ref->params.d, 0};

            const Ren::Vec3u grp_count =
                Ren::Vec3u{(img_res[0] + Fog::LOCAL_GROUP_SIZE_X - 1u) / Fog::LOCAL_GROUP_SIZE_X,
                           (img_res[1] + Fog::LOCAL_GROUP_SIZE_Y - 1u) / Fog::LOCAL_GROUP_SIZE_Y, 1};

            Fog::Params uniform_params;
            uniform_params.froxel_res = froxel_res;
            uniform_params.img_res = img_res;

            DispatchCompute(*pi_vol_apply_, grp_count, bindings, &uniform_params, sizeof(uniform_params),
                            builder.ctx().default_descr_alloc(), builder.ctx().log());
        });
    }
}
