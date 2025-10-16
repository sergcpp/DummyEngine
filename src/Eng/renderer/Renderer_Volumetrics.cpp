#include "Renderer.h"

#include <Ren/Context.h>
#include <Sys/AssetFile.h>

#include "../utils/Load.h"
#include "Renderer_Names.h"
#include "executors/ExSkydome.h"
#include "executors/ExVolVoxelize.h"

#include "shaders/blit_fxaa_interface.h"
#include "shaders/blit_vol_compose_interface.h"
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
                    assert(p.format == Ren::eTexFormat::BC1_srgb);
                    p.format = Ren::eTexFormat::BC1;
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

                std::vector<uint8_t> data = LoadDDS(noise, &p);
                if (!data.empty()) {
                    assert(p.format == Ren::eTexFormat::BC4);
                    if (!ctx_.capabilities.bc4_3d_texture_format) {
                        // Decompress texture
                        std::vector<uint8_t> decompressed_data(p.w * p.h * p.d);
                        for (int z = 0; z < int(p.d); ++z) {
                            const uint8_t *compressed_block = &data[z * p.w * p.h / 2];
                            uint8_t *out_data = &decompressed_data[z * p.w * p.h];
                            for (int y = 0; y < int(p.h); y += 4) {
                                for (int x = 0; x < int(p.w); x += 4) {
                                    uint8_t decode_data[8];
                                    decode_data[0] = compressed_block[0];
                                    decode_data[1] = compressed_block[1];

                                    // 6-step intermediate values
                                    decode_data[2] = (6 * decode_data[0] + 1 * decode_data[1]) / 7;
                                    decode_data[3] = (5 * decode_data[0] + 2 * decode_data[1]) / 7;
                                    decode_data[4] = (4 * decode_data[0] + 3 * decode_data[1]) / 7;
                                    decode_data[5] = (3 * decode_data[0] + 4 * decode_data[1]) / 7;
                                    decode_data[6] = (2 * decode_data[0] + 5 * decode_data[1]) / 7;
                                    decode_data[7] = (1 * decode_data[0] + 6 * decode_data[1]) / 7;

                                    int next_bit = 8 * 2;
                                    for (int i = 0; i < 16; ++i) {
                                        int idx = 0, bit;
                                        bit = (compressed_block[next_bit >> 3] >> (next_bit & 7)) & 1;
                                        idx += bit << 0;
                                        ++next_bit;
                                        bit = (compressed_block[next_bit >> 3] >> (next_bit & 7)) & 1;
                                        idx += bit << 1;
                                        ++next_bit;
                                        bit = (compressed_block[next_bit >> 3] >> (next_bit & 7)) & 1;
                                        idx += bit << 2;
                                        ++next_bit;

                                        out_data[(y + (i / 4)) * int(p.w) + x + (i % 4)] = decode_data[idx & 7];
                                    }

                                    compressed_block += 8;
                                }
                            }
                        }
                        p.format = Ren::eTexFormat::R8;
                        data = std::move(decompressed_data);
                    }

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
                params.w = (view_state_.ren_res[0] + 3) / 4;
                params.h = (view_state_.ren_res[1] + 3) / 4;
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
            params.w = view_state_.ren_res[0];
            params.h = view_state_.ren_res[1];
            params.format = Ren::eTexFormat::RGBA16F;
            params.sampling.filter = Ren::eTexFilter::Bilinear;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

            sky_upsampled = data->output_tex =
                sky_upsample.AddStorageImageOutput("SKY HIST", params, Stg::ComputeShader);
            data->sky_hist_tex = sky_upsample.AddHistoryTextureInput(sky_upsampled, Stg::ComputeShader);

            sky_upsample.set_execute_cb([data, this](FgContext &fg) {
                FgAllocBuf &unif_sh_data_buf = fg.AccessROBuffer(data->shared_data);
                FgAllocTex &env_map_tex = fg.AccessROTexture(data->env_map);
                FgAllocTex &sky_temp_tex = fg.AccessROTexture(data->sky_temp_tex);
                FgAllocTex &sky_hist_tex = fg.AccessROTexture(data->sky_hist_tex);
                FgAllocTex &output_tex = fg.AccessRWTexture(data->output_tex);

                const Ren::Binding bindings[] = {{Trg::UBuf, BIND_UB_SHARED_DATA_BUF, *unif_sh_data_buf.ref},
                                                 {Trg::TexSampled, Skydome::ENV_TEX_SLOT, *env_map_tex.ref},
                                                 {Trg::TexSampled, Skydome::SKY_TEX_SLOT, *sky_temp_tex.ref},
                                                 {Trg::TexSampled, Skydome::SKY_HIST_TEX_SLOT, *sky_hist_tex.ref},
                                                 {Trg::ImageRW, Skydome::OUT_IMG_SLOT, *output_tex.ref}};

                const Ren::Vec3u grp_count =
                    Ren::Vec3u{(view_state_.ren_res[0] + Skydome::GRP_SIZE_X - 1u) / Skydome::GRP_SIZE_X,
                               (view_state_.ren_res[1] + Skydome::GRP_SIZE_Y - 1u) / Skydome::GRP_SIZE_Y, 1u};

                Skydome::Params2 uniform_params;
                uniform_params.img_size = view_state_.ren_res;
                uniform_params.texel_size = 1.0f / Ren::Vec2f{view_state_.ren_res};
                uniform_params.sample_coord = ExSkydomeScreen::sample_pos(view_state_.frame_index);
                uniform_params.hist_weight = (view_state_.pre_exposure / view_state_.prev_pre_exposure);

                DispatchCompute(*pi_sky_upsample_, grp_count, bindings, &uniform_params, sizeof(uniform_params),
                                fg.descr_alloc(), fg.log());
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

            sky_blit.set_execute_cb([data, this](FgContext &fg) {
                FgAllocTex &sky_tex = fg.AccessROTexture(data->sky_tex);
                FgAllocTex &depth_tex = fg.AccessRWTexture(data->depth_tex);
                FgAllocTex &output_tex = fg.AccessRWTexture(data->output_tex);

                Ren::RastState rast_state;
                rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);
                rast_state.depth.write_enabled = false;
                rast_state.depth.test_enabled = true;
                rast_state.depth.compare_op = uint8_t(Ren::eCompareOp::Equal);

                rast_state.viewport[2] = view_state_.ren_res[0];
                rast_state.viewport[3] = view_state_.ren_res[1];

                const Ren::Binding bindings[] = {{Trg::TexSampled, FXAA::INPUT_TEX_SLOT, *sky_tex.ref}};

                FXAA::Params uniform_params;
                uniform_params.transform = Ren::Vec4f{0.0f, 0.0f, 1.0f, 1.0f};
                uniform_params.inv_resolution = 1.0f / Ren::Vec2f{view_state_.ren_res};

                const Ren::RenderTarget depth_target = {depth_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store};
                const Ren::RenderTarget render_targets[] = {{output_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store}};

                prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, blit_fxaa_prog_, depth_target, render_targets, rast_state,
                                    fg.rast_state(), bindings, &uniform_params, sizeof(FXAA::Params), 0);
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

        sun_color.set_execute_cb([data, this](FgContext &fg) {
            FgAllocBuf &unif_sh_data_buf = fg.AccessROBuffer(data->shared_data);
            FgAllocTex &transmittance_lut = fg.AccessROTexture(data->transmittance_lut);
            FgAllocTex &multiscatter_lut = fg.AccessROTexture(data->multiscatter_lut);
            FgAllocTex &moon_tex = fg.AccessROTexture(data->moon_tex);
            FgAllocTex &weather_tex = fg.AccessROTexture(data->weather_tex);
            FgAllocTex &cirrus_tex = fg.AccessROTexture(data->cirrus_tex);
            FgAllocTex &noise3d_tex = fg.AccessROTexture(data->noise3d_tex);
            FgAllocBuf &output_buf = fg.AccessRWBuffer(data->output_buf);

            const Ren::Binding bindings[] = {
                {Trg::UBuf, BIND_UB_SHARED_DATA_BUF, *unif_sh_data_buf.ref},
                {Trg::TexSampled, SunBrightness::TRANSMITTANCE_LUT_SLOT, *transmittance_lut.ref},
                {Trg::TexSampled, SunBrightness::MULTISCATTER_LUT_SLOT, *multiscatter_lut.ref},
                {Trg::TexSampled, SunBrightness::MOON_TEX_SLOT, *moon_tex.ref},
                {Trg::TexSampled, SunBrightness::WEATHER_TEX_SLOT, *weather_tex.ref},
                {Trg::TexSampled, SunBrightness::CIRRUS_TEX_SLOT, *cirrus_tex.ref},
                {Trg::TexSampled, SunBrightness::NOISE3D_TEX_SLOT, *noise3d_tex.ref},
                {Trg::SBufRW, SunBrightness::OUT_BUF_SLOT, *output_buf.ref}};

            DispatchCompute(*pi_sun_brightness_, Ren::Vec3u{1u, 1u, 1u}, bindings, nullptr, 0, fg.descr_alloc(),
                            fg.log());
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

        sun_color.set_execute_cb([data](FgContext &fg) {
            FgAllocBuf &sample_buf = fg.AccessROBuffer(data->sample_buf);
            FgAllocBuf &unif_sh_data_buf = fg.AccessRWBuffer(data->shared_data);

            CopyBufferToBuffer(*sample_buf.ref, 0, *unif_sh_data_buf.ref, offsetof(shared_data_t, sun_col),
                               3 * sizeof(float), fg.cmd_buf());
            CopyBufferToBuffer(*sample_buf.ref, 4 * sizeof(float), *unif_sh_data_buf.ref,
                               offsetof(shared_data_t, sun_col_point_sh), 3 * sizeof(float), fg.cmd_buf());
        });
    }
}

void Eng::Renderer::AddVolumetricPasses(const CommonBuffers &common_buffers, const PersistentGpuData &persistent_data,
                                        const AccelerationStructureData &acc_struct_data, FgResRef rt_geo_instances_res,
                                        FgResRef rt_obj_instances_res, FrameTextures &frame_textures) {
    if (!ctx_.capabilities.hwrt && !ctx_.capabilities.swrt) {
        return;
    }

    using Stg = Ren::eStage;
    using Trg = Ren::eBindTarget;

    const int TileSize = (p_list_->render_settings.vol_quality == Eng::eVolQuality::Ultra) ? 8 : 12;
    const bool AllCascades = (p_list_->render_settings.vol_quality == Eng::eVolQuality::Ultra);

    FgResRef fr_emission_tex, fr_scatter_tex;
    { // Voxelize
        auto &voxelize = fg_builder_.AddNode("VOL VOXELIZE");

        auto *data = voxelize.AllocNodeData<ExVolVoxelize::Args>();
        data->shared_data = voxelize.AddUniformBufferInput(common_buffers.shared_data, Stg::ComputeShader);
        data->stbn_tex = voxelize.AddTextureInput(stbn_1D_64spp_, Stg::ComputeShader);

        data->geo_data = voxelize.AddStorageReadonlyInput(rt_geo_instances_res, Stg::ComputeShader);
        data->materials = voxelize.AddStorageReadonlyInput(persistent_data.materials_buf, Stg::ComputeShader);

        data->tlas_buf =
            voxelize.AddStorageReadonlyInput(persistent_data.rt_tlas_buf[int(eTLASIndex::Volume)], Stg::ComputeShader);
        data->tlas = acc_struct_data.rt_tlases[int(eTLASIndex::Volume)];

        if (!ctx_.capabilities.hwrt) {
            data->swrt.root_node = persistent_data.swrt.rt_root_node;
            data->swrt.rt_blas_buf =
                voxelize.AddStorageReadonlyInput(persistent_data.swrt.rt_blas_buf, Stg::ComputeShader);
            data->swrt.prim_ndx_buf =
                voxelize.AddStorageReadonlyInput(persistent_data.swrt.rt_prim_indices_buf, Stg::ComputeShader);
            data->swrt.mesh_instances_buf = voxelize.AddStorageReadonlyInput(rt_obj_instances_res, Stg::ComputeShader);
            data->swrt.vtx_buf1 = voxelize.AddStorageReadonlyInput(persistent_data.vertex_buf1, Stg::ComputeShader);
            data->swrt.ndx_buf = voxelize.AddStorageReadonlyInput(persistent_data.indices_buf, Stg::ComputeShader);
        }

        { // Emission/Density and Scatter/Absorption textures
            Ren::TexParams p;
            p.w = (view_state_.ren_res[0] + TileSize - 1) / TileSize;
            p.h = (view_state_.ren_res[1] + TileSize - 1) / TileSize;
            p.d = 144;
            p.format = Ren::eTexFormat::RGBA16F;
            p.sampling.filter = Ren::eTexFilter::Bilinear;
            p.sampling.wrap = Ren::eTexWrap::ClampToEdge;

            fr_emission_tex = data->out_emission_tex =
                voxelize.AddStorageImageOutput("Vol Emission/Density", p, Stg::ComputeShader);
            fr_scatter_tex = data->out_scatter_tex =
                voxelize.AddStorageImageOutput("Vol Scatter/Absorption", p, Stg::ComputeShader);
        }

        voxelize.make_executor<ExVolVoxelize>(&p_list_, &view_state_, data);
    }
    { // Scatter
        auto &scatter = fg_builder_.AddNode("VOL SCATTER");

        struct PassData {
            FgResRef shared_data;
            FgResRef stbn_tex;
            FgResRef shadow_depth_tex, shadow_color_tex;
            FgResRef fr_emission_tex, fr_scatter_tex;
            FgResRef cells_buf, items_buf, lights_buf, decals_buf;
            FgResRef envmap_tex;
            FgResRef irradiance_tex, distance_tex, offset_tex;
            FgResRef out_emission_tex, out_scatter_tex;
        };

        auto *data = scatter.AllocNodeData<PassData>();
        data->shared_data = scatter.AddUniformBufferInput(common_buffers.shared_data, Stg::ComputeShader);
        data->stbn_tex = scatter.AddTextureInput(stbn_1D_64spp_, Stg::ComputeShader);
        data->shadow_depth_tex = scatter.AddTextureInput(frame_textures.shadow_depth, Stg::ComputeShader);
        data->shadow_color_tex = scatter.AddTextureInput(frame_textures.shadow_color, Stg::ComputeShader);

        fr_emission_tex = data->out_emission_tex = scatter.AddStorageImageOutput(fr_emission_tex, Stg::ComputeShader);
        fr_scatter_tex = data->out_scatter_tex = scatter.AddStorageImageOutput(fr_scatter_tex, Stg::ComputeShader);

        data->fr_emission_tex = scatter.AddHistoryTextureInput(fr_emission_tex, Stg::ComputeShader);
        data->fr_scatter_tex = scatter.AddHistoryTextureInput(fr_scatter_tex, Stg::ComputeShader);

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

        scatter.set_execute_cb([data, this, AllCascades](FgContext &fg) {
            FgAllocBuf &unif_sh_data_buf = fg.AccessROBuffer(data->shared_data);

            FgAllocTex &stbn_tex = fg.AccessROTexture(data->stbn_tex);
            FgAllocTex &shad_depth_tex = fg.AccessROTexture(data->shadow_depth_tex);
            FgAllocTex &shad_color_tex = fg.AccessROTexture(data->shadow_color_tex);

            FgAllocTex &fr_emission_tex = fg.AccessROTexture(data->fr_emission_tex);
            FgAllocTex &fr_scatter_tex = fg.AccessROTexture(data->fr_scatter_tex);

            FgAllocBuf &cells_buf = fg.AccessROBuffer(data->cells_buf);
            FgAllocBuf &items_buf = fg.AccessROBuffer(data->items_buf);
            FgAllocBuf &lights_buf = fg.AccessROBuffer(data->lights_buf);
            FgAllocBuf &decals_buf = fg.AccessROBuffer(data->decals_buf);
            FgAllocTex &envmap_tex = fg.AccessROTexture(data->envmap_tex);

            FgAllocTex *irr_tex = nullptr, *dist_tex = nullptr, *off_tex = nullptr;
            if (data->irradiance_tex) {
                irr_tex = &fg.AccessROTexture(data->irradiance_tex);
                dist_tex = &fg.AccessROTexture(data->distance_tex);
                off_tex = &fg.AccessROTexture(data->offset_tex);
            }

            FgAllocTex &out_emission_tex = fg.AccessRWTexture(data->out_emission_tex);
            FgAllocTex &out_scatter_tex = fg.AccessRWTexture(data->out_scatter_tex);

            if (view_state_.skip_volumetrics) {
                return;
            }

            Ren::SmallVector<Ren::Binding, 16> bindings = {
                {Trg::UBuf, BIND_UB_SHARED_DATA_BUF, *unif_sh_data_buf.ref},
                {Trg::TexSampled, Fog::STBN_TEX_SLOT, *stbn_tex.ref},
                {Trg::TexSampled, Fog::SHADOW_DEPTH_TEX_SLOT, *shad_depth_tex.ref},
                {Trg::TexSampled, Fog::SHADOW_COLOR_TEX_SLOT, *shad_color_tex.ref},
                {Trg::TexSampled, Fog::FR_EMISSION_TEX_SLOT, *fr_emission_tex.ref},
                {Trg::TexSampled, Fog::FR_SCATTER_TEX_SLOT, *fr_scatter_tex.ref},
                {Trg::UTBuf, Fog::CELLS_BUF_SLOT, *cells_buf.ref},
                {Trg::UTBuf, Fog::ITEMS_BUF_SLOT, *items_buf.ref},
                {Trg::UTBuf, Fog::LIGHT_BUF_SLOT, *lights_buf.ref},
                {Trg::UTBuf, Fog::DECAL_BUF_SLOT, *decals_buf.ref},
                {Trg::TexSampled, Fog::ENVMAP_TEX_SLOT, *envmap_tex.ref},
                {Trg::ImageRW, Fog::OUT_FR_EMISSION_IMG_SLOT, *out_emission_tex.ref},
                {Trg::ImageRW, Fog::OUT_FR_SCATTER_IMG_SLOT, *out_scatter_tex.ref}};
            if (irr_tex) {
                bindings.emplace_back(Ren::eBindTarget::TexSampled, Fog::IRRADIANCE_TEX_SLOT, *irr_tex->ref);
                bindings.emplace_back(Ren::eBindTarget::TexSampled, Fog::DISTANCE_TEX_SLOT, *dist_tex->ref);
                bindings.emplace_back(Ren::eBindTarget::TexSampled, Fog::OFFSET_TEX_SLOT, *off_tex->ref);
            }

            const auto froxel_res = Ren::Vec4i{out_emission_tex.ref->params.w, out_emission_tex.ref->params.h,
                                               out_emission_tex.ref->params.d, 0};

            const Ren::Vec3u grp_count =
                Ren::Vec3u{(froxel_res[0] + Fog::GRP_SIZE_X - 1u) / Fog::GRP_SIZE_X,
                           (froxel_res[1] + Fog::GRP_SIZE_Y - 1u) / Fog::GRP_SIZE_Y, froxel_res[2]};

            Fog::Params uniform_params;
            uniform_params.froxel_res = froxel_res;
            uniform_params.scatter_color =
                Ren::Saturate(Ren::Vec4f{p_list_->env.fog.scatter_color, p_list_->env.fog.absorption});
            uniform_params.emission_color =
                Ren::Vec4f{p_list_->env.fog.emission_color, std::max(p_list_->env.fog.density, 0.0f)};
            uniform_params.anisotropy = Ren::Clamp(p_list_->env.fog.anisotropy, -0.99f, 0.99f);
            uniform_params.bbox_min = Ren::Vec4f{p_list_->env.fog.bbox_min};
            uniform_params.bbox_max = Ren::Vec4f{p_list_->env.fog.bbox_max};
            uniform_params.frame_index = view_state_.frame_index;
            uniform_params.hist_weight = (view_state_.pre_exposure / view_state_.prev_pre_exposure);

            DispatchCompute(*pi_vol_scatter_[AllCascades][irr_tex != nullptr], grp_count, bindings, &uniform_params,
                            sizeof(uniform_params), fg.descr_alloc(), fg.log());
        });
    }
    FgResRef froxel_tex;
    { // Ray march
        auto &ray_march = fg_builder_.AddNode("VOL RAY MARCH");

        struct PassData {
            FgResRef shared_data;
            FgResRef fr_emission_tex;
            FgResRef fr_scatter_tex;
            FgResRef output_tex;
        };

        auto *data = ray_march.AllocNodeData<PassData>();
        data->shared_data = ray_march.AddUniformBufferInput(common_buffers.shared_data, Stg::ComputeShader);
        data->fr_emission_tex = ray_march.AddTextureInput(fr_emission_tex, Stg::ComputeShader);
        data->fr_scatter_tex = ray_march.AddTextureInput(fr_scatter_tex, Stg::ComputeShader);

        { //
            Ren::TexParams p;
            p.w = (view_state_.ren_res[0] + TileSize - 1) / TileSize;
            p.h = (view_state_.ren_res[1] + TileSize - 1) / TileSize;
            p.d = 144;
            p.format = Ren::eTexFormat::RGBA16F;
            p.sampling.filter = Ren::eTexFilter::Bilinear;
            p.sampling.wrap = Ren::eTexWrap::ClampToEdge;

            froxel_tex = data->output_tex =
                ray_march.AddStorageImageOutput("Vol Scattering Final", p, Stg::ComputeShader);
        }

        ray_march.set_execute_cb([data, this](FgContext &fg) {
            FgAllocBuf &unif_sh_data_buf = fg.AccessROBuffer(data->shared_data);
            FgAllocTex &fr_emission_tex = fg.AccessROTexture(data->fr_emission_tex);
            FgAllocTex &fr_scatter_tex = fg.AccessROTexture(data->fr_scatter_tex);

            FgAllocTex &output_tex = fg.AccessRWTexture(data->output_tex);

            if (view_state_.skip_volumetrics) {
                return;
            }

            const Ren::Binding bindings[] = {{Trg::UBuf, BIND_UB_SHARED_DATA_BUF, *unif_sh_data_buf.ref},
                                             {Trg::TexSampled, Fog::FR_EMISSION_TEX_SLOT, *fr_emission_tex.ref},
                                             {Trg::TexSampled, Fog::FR_SCATTER_TEX_SLOT, *fr_scatter_tex.ref},
                                             {Trg::ImageRW, Fog::OUT_FR_FINAL_IMG_SLOT, *output_tex.ref}};

            const auto froxel_res =
                Ren::Vec4i{output_tex.ref->params.w, output_tex.ref->params.h, output_tex.ref->params.d, 0};

            const Ren::Vec3u grp_count = Ren::Vec3u{(froxel_res[0] + Fog::GRP_SIZE_X - 1u) / Fog::GRP_SIZE_X,
                                                    (froxel_res[1] + Fog::GRP_SIZE_Y - 1u) / Fog::GRP_SIZE_Y, 1};

            Fog::Params uniform_params;
            uniform_params.froxel_res = froxel_res;
            uniform_params.scatter_color =
                Ren::Saturate(Ren::Vec4f{p_list_->env.fog.scatter_color, p_list_->env.fog.absorption});

            DispatchCompute(*pi_vol_ray_march_, grp_count, bindings, &uniform_params, sizeof(uniform_params),
                            fg.descr_alloc(), fg.log());
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
        data->shared_data = apply.AddUniformBufferInput(common_buffers.shared_data, Stg::FragmentShader);
        data->depth_tex = apply.AddTextureInput(frame_textures.depth, Stg::FragmentShader);
        data->froxel_tex = apply.AddTextureInput(froxel_tex, Stg::FragmentShader);

        frame_textures.color = data->output_tex = apply.AddColorOutput(frame_textures.color);

        apply.set_execute_cb([data, this](FgContext &fg) {
            FgAllocBuf &unif_sh_data_buf = fg.AccessROBuffer(data->shared_data);

            FgAllocTex &depth_tex = fg.AccessROTexture(data->depth_tex);
            FgAllocTex &froxel_tex = fg.AccessROTexture(data->froxel_tex);
            FgAllocTex &output_tex = fg.AccessRWTexture(data->output_tex);

            if (view_state_.skip_volumetrics) {
                return;
            }

            Ren::RastState rast_state;
            rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);
            rast_state.depth.write_enabled = false;
            rast_state.depth.test_enabled = false;
            rast_state.blend.enabled = true;
            rast_state.blend.src_color = uint8_t(Ren::eBlendFactor::One);
            rast_state.blend.src_alpha = uint8_t(Ren::eBlendFactor::Zero);
            rast_state.blend.dst_color = uint8_t(Ren::eBlendFactor::SrcAlpha);
            rast_state.blend.dst_alpha = uint8_t(Ren::eBlendFactor::One);

            rast_state.viewport[2] = view_state_.ren_res[0];
            rast_state.viewport[3] = view_state_.ren_res[1];

            const Ren::Binding bindings[] = {{Trg::UBuf, BIND_UB_SHARED_DATA_BUF, *unif_sh_data_buf.ref},
                                             {Trg::TexSampled, VolCompose::DEPTH_TEX_SLOT, {*depth_tex.ref, 1}},
                                             {Trg::TexSampled, VolCompose::FROXELS_TEX_SLOT, *froxel_tex.ref}};

            const auto img_res = Ren::Vec2i{depth_tex.ref->params.w, depth_tex.ref->params.h};
            const auto froxel_res =
                Ren::Vec4i{froxel_tex.ref->params.w, froxel_tex.ref->params.h, froxel_tex.ref->params.d, 0};

            const Ren::RenderTarget render_targets[] = {{output_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store}};

            VolCompose::Params uniform_params;
            uniform_params.transform = Ren::Vec4f{0.0f, 0.0f, 1.0f, 1.0f};
            uniform_params.froxel_res = Ren::Vec4f(froxel_res);

            prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, blit_vol_compose_prog_, {}, render_targets, rast_state,
                                fg.rast_state(), bindings, &uniform_params, sizeof(uniform_params), 0);
        });
    }
}
