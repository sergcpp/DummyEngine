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
        sky_moon_ = {};
        sky_weather_ = {};
        sky_cirrus_ = {};
        sky_curl_ = {};
        sky_noise3d_ = {};
    } else {
        if (!sky_transmittance_lut_) {
            const std::vector<Ren::Vec4f> transmittance_lut = Generate_SkyTransmittanceLUT(p_list_->env.atmosphere);
            { // Init transmittance LUT
                Ren::ImgParams p;
                p.w = SKY_TRANSMITTANCE_LUT_W;
                p.h = SKY_TRANSMITTANCE_LUT_H;
                p.format = Ren::eFormat::RGBA32F;
                p.usage = Ren::Bitmask(Ren::eImgUsage::Transfer) | Ren::eImgUsage::Sampled;
                p.sampling.filter = Ren::eFilter::Bilinear;
                p.sampling.wrap = Ren::eWrap::ClampToEdge;

                sky_transmittance_lut_ = ctx_.CreateImage(
                    Ren::String{"Sky Transmittance LUT"},
                    Ren::Span{(const uint8_t *)transmittance_lut.data(), transmittance_lut.size() * sizeof(Ren::Vec4f)},
                    p, ctx_.default_mem_allocs());
                assert(sky_transmittance_lut_);
            }
            { // Init multiscatter LUT
                const std::vector<Ren::Vec4f> multiscatter_lut =
                    Generate_SkyMultiscatterLUT(p_list_->env.atmosphere, transmittance_lut);

                Ren::ImgParams p;
                p.w = p.h = SKY_MULTISCATTER_LUT_RES;
                p.format = Ren::eFormat::RGBA32F;
                p.usage = Ren::Bitmask(Ren::eImgUsage::Transfer) | Ren::eImgUsage::Sampled;
                p.sampling.filter = Ren::eFilter::Bilinear;
                p.sampling.wrap = Ren::eWrap::ClampToEdge;

                sky_multiscatter_lut_ = ctx_.CreateImage(
                    Ren::String{"Sky Multiscatter LUT"},
                    Ren::Span{(const uint8_t *)multiscatter_lut.data(), multiscatter_lut.size() * sizeof(Ren::Vec4f)},
                    p, ctx_.default_mem_allocs());
                assert(sky_multiscatter_lut_);
            }
            { // Init Moon texture
                const std::string_view moon_diff = "assets_pc/textures/internal/moon_diff.dds";

                Ren::ImgParams p;
                p.usage = Ren::Bitmask(Ren::eImgUsage::Transfer) | Ren::eImgUsage::Sampled;
                p.sampling.filter = Ren::eFilter::Bilinear;
                p.sampling.wrap = Ren::eWrap::Repeat;

                const std::vector<uint8_t> data = LoadDDS(moon_diff, &p);
                if (!data.empty()) {
                    sky_moon_ = ctx_.CreateImage(Ren::String{moon_diff}, data, p, ctx_.default_mem_allocs());
                    assert(sky_moon_);
                } else {
                    ctx_.log()->Error("Failed to load %s", moon_diff.data());
                }
            }
            { // Init Weather texture
                const std::string_view weather = "assets_pc/textures/internal/weather.dds";

                Ren::ImgParams p;
                p.usage = Ren::Bitmask(Ren::eImgUsage::Transfer) | Ren::eImgUsage::Sampled;
                p.sampling.filter = Ren::eFilter::Bilinear;
                p.sampling.wrap = Ren::eWrap::Repeat;

                const std::vector<uint8_t> data = LoadDDS(weather, &p);
                if (!data.empty()) {
                    assert(p.format == Ren::eFormat::BC1_srgb);
                    p.format = Ren::eFormat::BC1;
                    sky_weather_ = ctx_.CreateImage(Ren::String{weather}, data, p, ctx_.default_mem_allocs());
                    assert(sky_weather_);
                } else {
                    ctx_.log()->Error("Failed to load %s", weather.data());
                }
            }
            { // Init Cirrus texture
                const std::string_view cirrus = "assets_pc/textures/internal/cirrus.dds";

                Ren::ImgParams p;
                p.usage = Ren::Bitmask(Ren::eImgUsage::Transfer) | Ren::eImgUsage::Sampled;
                p.sampling.filter = Ren::eFilter::Bilinear;
                p.sampling.wrap = Ren::eWrap::Repeat;

                const std::vector<uint8_t> data = LoadDDS(cirrus, &p);
                if (!data.empty()) {
                    sky_cirrus_ = ctx_.CreateImage(Ren::String{cirrus}, data, p, ctx_.default_mem_allocs());
                    assert(sky_cirrus_);
                } else {
                    ctx_.log()->Error("Failed to load %s", cirrus.data());
                }
            }
            { // Init Curl texture
                const std::string_view curl = "assets_pc/textures/internal/curl.dds";

                Ren::ImgParams p;
                // p.flags = Ren::eImgFlags::SRGB;
                p.usage = Ren::Bitmask(Ren::eImgUsage::Transfer) | Ren::eImgUsage::Sampled;
                p.sampling.filter = Ren::eFilter::Bilinear;
                p.sampling.wrap = Ren::eWrap::Repeat;

                const std::vector<uint8_t> data = LoadDDS(curl, &p);
                assert(p.format == Ren::eFormat::RGBA8);
                p.format = Ren::eFormat::RGBA8_srgb;

                if (!data.empty()) {
                    sky_curl_ = ctx_.CreateImage(Ren::String{curl}, data, p, ctx_.default_mem_allocs());
                    assert(sky_curl_);
                } else {
                    ctx_.log()->Error("Failed to load %s", curl.data());
                }
            }
            { // Init 3d noise texture
                const std::string_view noise = "assets_pc/textures/internal/3dnoise.dds";

                Ren::ImgParams p;
                p.usage = Ren::Bitmask(Ren::eImgUsage::Sampled) | Ren::eImgUsage::Transfer;
                p.sampling.filter = Ren::eFilter::Bilinear;
                p.sampling.wrap = Ren::eWrap::Repeat;

                std::vector<uint8_t> data = LoadDDS(noise, &p);
                if (!data.empty()) {
                    assert(p.format == Ren::eFormat::BC4);
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
                        p.format = Ren::eFormat::R8;
                        data = std::move(decompressed_data);
                    }

                    sky_noise3d_ = ctx_.CreateImage(Ren::String{noise}, data, p, ctx_.default_mem_allocs());
                    assert(sky_noise3d_);
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

        auto *data = fg_builder_.AllocTempData<ExSkydomeCube::Args>();

        data->shared_data = skydome_cube.AddUniformBufferInput(common_buffers.shared_data,
                                                               Ren::Bitmask{Stg::VertexShader} | Stg::FragmentShader);

        data->transmittance_lut =
            skydome_cube.AddTextureInput(frame_textures.sky_transmittance_lut, Stg::FragmentShader);
        data->multiscatter_lut = skydome_cube.AddTextureInput(frame_textures.sky_multiscatter_lut, Stg::FragmentShader);
        data->moon = skydome_cube.AddTextureInput(frame_textures.sky_moon, Stg::FragmentShader);
        data->weather = skydome_cube.AddTextureInput(frame_textures.sky_weather, Stg::FragmentShader);
        data->cirrus = skydome_cube.AddTextureInput(frame_textures.sky_cirrus, Stg::FragmentShader);
        data->curl = skydome_cube.AddTextureInput(frame_textures.sky_curl, Stg::FragmentShader);
        data->noise3d = skydome_cube.AddTextureInput(frame_textures.sky_noise3d, Stg::FragmentShader);
        frame_textures.envmap = data->color = skydome_cube.AddColorOutput(frame_textures.envmap);

        skydome_cube.make_executor<ExSkydomeCube>(prim_draw_, &view_state_, data);
    }

    FgImgRWHandle sky_temp;
    { // Main pass
        auto &skymap = fg_builder_.AddNode("SKYDOME");

        auto *data = fg_builder_.AllocTempData<ExSkydomeScreen::Args>();
        data->shared_data = skymap.AddUniformBufferInput(common_buffers.shared_data,
                                                         Ren::Bitmask{Stg::VertexShader} | Stg::FragmentShader);
        if (p_list_->env.env_map_name != "physical_sky") {
            data->env = skymap.AddTextureInput(frame_textures.envmap, Stg::FragmentShader);
            frame_textures.color = data->color = skymap.AddColorOutput(MAIN_COLOR_TEX, frame_textures.color_desc);
            frame_textures.depth = data->depth_rw = skymap.AddDepthOutput(MAIN_DEPTH_TEX, frame_textures.depth_desc);
        } else {
            data->sky_quality = settings.sky_quality;
            data->env = skymap.AddTextureInput(frame_textures.envmap, Stg::FragmentShader);
            data->phys.transmittance_lut =
                skymap.AddTextureInput(frame_textures.sky_transmittance_lut, Stg::FragmentShader);
            data->phys.multiscatter_lut =
                skymap.AddTextureInput(frame_textures.sky_multiscatter_lut, Stg::FragmentShader);
            data->phys.moon = skymap.AddTextureInput(frame_textures.sky_moon, Stg::FragmentShader);
            data->phys.weather = skymap.AddTextureInput(frame_textures.sky_weather, Stg::FragmentShader);
            data->phys.cirrus = skymap.AddTextureInput(frame_textures.sky_cirrus, Stg::FragmentShader);
            data->phys.curl = skymap.AddTextureInput(frame_textures.sky_curl, Stg::FragmentShader);
            data->phys.noise3d = skymap.AddTextureInput(frame_textures.sky_noise3d, Stg::FragmentShader);

            if (settings.sky_quality == eSkyQuality::High) {
                data->depth_ro = skymap.AddTextureInput(frame_textures.depth, Stg::FragmentShader);

                FgImgDesc desc;
                desc.w = (view_state_.ren_res[0] + 3) / 4;
                desc.h = (view_state_.ren_res[1] + 3) / 4;
                desc.format = Ren::eFormat::RGBA16F;
                desc.sampling.filter = Ren::eFilter::Bilinear;
                desc.sampling.wrap = Ren::eWrap::ClampToEdge;

                sky_temp = data->color = skymap.AddColorOutput("SKY TEMP", desc);
            } else {
                frame_textures.color = data->color = skymap.AddColorOutput(MAIN_COLOR_TEX, frame_textures.color_desc);
                frame_textures.depth = data->depth_rw =
                    skymap.AddDepthOutput(MAIN_DEPTH_TEX, frame_textures.depth_desc);
            }
        }

        skymap.make_executor<ExSkydomeScreen>(prim_draw_, &view_state_, data);
    }

    if (p_list_->env.env_map_name != "physical_sky") {
        return;
    }

    if (settings.sky_quality == eSkyQuality::High) {
        FgImgRWHandle sky_upsampled;
        { // prepare upsampled texture
            auto &sky_upsample = fg_builder_.AddNode("SKY UPSAMPLE");

            struct PassData {
                FgBufROHandle shared_data;
                FgImgROHandle env_map;
                FgImgROHandle sky_temp;
                FgImgROHandle sky_hist;
                FgImgRWHandle output;
            };

            auto *data = fg_builder_.AllocTempData<PassData>();
            data->shared_data = sky_upsample.AddUniformBufferInput(common_buffers.shared_data, Stg::ComputeShader);
            data->env_map = sky_upsample.AddTextureInput(frame_textures.envmap, Stg::ComputeShader);
            data->sky_temp = sky_upsample.AddTextureInput(sky_temp, Stg::ComputeShader);

            { //
                FgImgDesc desc;
                desc.w = view_state_.ren_res[0];
                desc.h = view_state_.ren_res[1];
                desc.format = Ren::eFormat::RGBA16F;
                desc.sampling.filter = Ren::eFilter::Bilinear;
                desc.sampling.wrap = Ren::eWrap::ClampToEdge;

                sky_upsampled = data->output = sky_upsample.AddStorageImageOutput("SKY HIST", desc, Stg::ComputeShader);
            }
            data->sky_hist = sky_upsample.AddHistoryTextureInput(sky_upsampled, Stg::ComputeShader);

            sky_upsample.set_execute_cb([data, this](const FgContext &fg) {
                const Ren::BufferROHandle unif_sh_data = fg.AccessROBuffer(data->shared_data);
                const Ren::ImageROHandle env_map = fg.AccessROImage(data->env_map);
                const Ren::ImageROHandle sky_temp = fg.AccessROImage(data->sky_temp);
                const Ren::ImageROHandle sky_hist = fg.AccessROImage(data->sky_hist);

                const Ren::ImageRWHandle output = fg.AccessRWImage(data->output);

                const Ren::Binding bindings[] = {{Trg::UBuf, BIND_UB_SHARED_DATA_BUF, unif_sh_data},
                                                 {Trg::TexSampled, Skydome::ENV_TEX_SLOT, env_map},
                                                 {Trg::TexSampled, Skydome::SKY_TEX_SLOT, sky_temp},
                                                 {Trg::TexSampled, Skydome::SKY_HIST_TEX_SLOT, sky_hist},
                                                 {Trg::ImageRW, Skydome::OUT_IMG_SLOT, output}};

                const Ren::Vec3u grp_count =
                    Ren::Vec3u{(view_state_.ren_res[0] + Skydome::GRP_SIZE_X - 1u) / Skydome::GRP_SIZE_X,
                               (view_state_.ren_res[1] + Skydome::GRP_SIZE_Y - 1u) / Skydome::GRP_SIZE_Y, 1u};

                Skydome::Params2 uniform_params;
                uniform_params.img_size = view_state_.ren_res;
                uniform_params.texel_size = 1.0f / Ren::Vec2f{view_state_.ren_res};
                uniform_params.sample_coord = ExSkydomeScreen::sample_pos(view_state_.frame_index);
                uniform_params.hist_weight = (view_state_.pre_exposure / view_state_.prev_pre_exposure);

                DispatchCompute(fg.cmd_buf(), pi_sky_upsample_, fg.storages(), grp_count, bindings, &uniform_params,
                                sizeof(uniform_params), fg.descr_alloc(), fg.log());
            });
        }
        { // blit result
            auto &sky_blit = fg_builder_.AddNode("SKY BLIT");

            struct PassData {
                FgImgROHandle sky;
                FgImgRWHandle depth;
                FgImgRWHandle output;
            };

            auto *data = fg_builder_.AllocTempData<PassData>();
            frame_textures.depth = data->depth = sky_blit.AddDepthOutput(frame_textures.depth);
            data->sky = sky_blit.AddTextureInput(sky_upsampled, Stg::FragmentShader);

            frame_textures.color = data->output = sky_blit.AddColorOutput(MAIN_COLOR_TEX, frame_textures.color_desc);

            sky_blit.set_execute_cb([data, this](const FgContext &fg) {
                const Ren::ImageROHandle sky = fg.AccessROImage(data->sky);
                const Ren::ImageRWHandle depth = fg.AccessRWImage(data->depth);
                const Ren::ImageRWHandle output = fg.AccessRWImage(data->output);

                Ren::RastState rast_state;
                rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);
                rast_state.depth.write_enabled = false;
                rast_state.depth.test_enabled = true;
                rast_state.depth.compare_op = uint8_t(Ren::eCompareOp::Equal);

                rast_state.viewport[2] = view_state_.ren_res[0];
                rast_state.viewport[3] = view_state_.ren_res[1];

                const Ren::Binding bindings[] = {{Trg::TexSampled, FXAA::INPUT_TEX_SLOT, sky}};

                FXAA::Params uniform_params;
                uniform_params.transform = Ren::Vec4f{0.0f, 0.0f, 1.0f, 1.0f};
                uniform_params.inv_resolution = 1.0f / Ren::Vec2f{view_state_.ren_res};

                const Ren::RenderTarget depth_target = {depth, Ren::eLoadOp::Load, Ren::eStoreOp::Store};
                const Ren::RenderTarget render_targets[] = {{output, Ren::eLoadOp::Load, Ren::eStoreOp::Store}};

                prim_draw_.DrawPrim(fg.cmd_buf(), PrimDraw::ePrim::Quad, blit_fxaa_prog_, depth_target, render_targets,
                                    rast_state, fg.rast_state(), bindings, &uniform_params, sizeof(FXAA::Params), 0,
                                    fg.framebuffers());
            });
        }
    }
}

void Eng::Renderer::AddSunColorUpdatePass(CommonBuffers &common_buffers, const FrameTextures &frame_textures) {
    using Stg = Ren::eStage;
    using Trg = Ren::eBindTarget;

    if (p_list_->env.env_map_name != "physical_sky") {
        return;
    }

    FgBufRWHandle output;

    { // Sample sun color
        auto &sun_color = fg_builder_.AddNode("SUN COLOR SAMPLE");

        struct PassData {
            FgBufROHandle shared_data;
            FgImgROHandle transmittance_lut;
            FgImgROHandle multiscatter_lut;
            FgImgROHandle moon;
            FgImgROHandle weather;
            FgImgROHandle cirrus;
            FgImgROHandle noise3d;
            FgBufRWHandle output;
        };

        auto *data = fg_builder_.AllocTempData<PassData>();

        data->shared_data = sun_color.AddUniformBufferInput(common_buffers.shared_data, Stg::ComputeShader);
        data->transmittance_lut = sun_color.AddTextureInput(frame_textures.sky_transmittance_lut, Stg::ComputeShader);
        data->multiscatter_lut = sun_color.AddTextureInput(frame_textures.sky_multiscatter_lut, Stg::ComputeShader);
        data->moon = sun_color.AddTextureInput(frame_textures.sky_moon, Stg::ComputeShader);
        data->weather = sun_color.AddTextureInput(frame_textures.sky_weather, Stg::ComputeShader);
        data->cirrus = sun_color.AddTextureInput(frame_textures.sky_cirrus, Stg::ComputeShader);
        data->noise3d = sun_color.AddTextureInput(frame_textures.sky_noise3d, Stg::ComputeShader);

        FgBufDesc desc = {};
        desc.type = Ren::eBufType::Storage;
        desc.size = 8 * sizeof(float);
        output = data->output = sun_color.AddStorageOutput("Sun Brightness Result", desc, Stg::ComputeShader);

        sun_color.set_execute_cb([data, this](const FgContext &fg) {
            const Ren::BufferROHandle unif_sh_data = fg.AccessROBuffer(data->shared_data);
            const Ren::ImageROHandle transmittance_lut = fg.AccessROImage(data->transmittance_lut);
            const Ren::ImageROHandle multiscatter_lut = fg.AccessROImage(data->multiscatter_lut);
            const Ren::ImageROHandle moon = fg.AccessROImage(data->moon);
            const Ren::ImageROHandle weather = fg.AccessROImage(data->weather);
            const Ren::ImageROHandle cirrus = fg.AccessROImage(data->cirrus);
            const Ren::ImageROHandle noise3d = fg.AccessROImage(data->noise3d);
            const Ren::BufferHandle output = fg.AccessRWBuffer(data->output);

            const Ren::Binding bindings[] = {
                {Trg::UBuf, BIND_UB_SHARED_DATA_BUF, unif_sh_data},
                {Trg::TexSampled, SunBrightness::TRANSMITTANCE_LUT_SLOT, transmittance_lut},
                {Trg::TexSampled, SunBrightness::MULTISCATTER_LUT_SLOT, multiscatter_lut},
                {Trg::TexSampled, SunBrightness::MOON_TEX_SLOT, moon},
                {Trg::TexSampled, SunBrightness::WEATHER_TEX_SLOT, weather},
                {Trg::TexSampled, SunBrightness::CIRRUS_TEX_SLOT, cirrus},
                {Trg::TexSampled, SunBrightness::NOISE3D_TEX_SLOT, noise3d},
                {Trg::SBufRW, SunBrightness::OUT_BUF_SLOT, output}};

            DispatchCompute(fg.cmd_buf(), pi_sun_brightness_, fg.storages(), Ren::Vec3u{1u, 1u, 1u}, bindings, nullptr,
                            0, fg.descr_alloc(), fg.log());
        });
    }
    { // Update sun color
        auto &sun_color = fg_builder_.AddNode("SUN COLOR UPDATE");

        struct PassData {
            FgBufROHandle samples;
            FgBufRWHandle shared_data;
        };

        auto *data = fg_builder_.AllocTempData<PassData>();

        data->samples = sun_color.AddTransferInput(output);
        common_buffers.shared_data = data->shared_data = sun_color.AddTransferOutput(common_buffers.shared_data);

        sun_color.set_execute_cb([data](const FgContext &fg) {
            const Ren::BufferROHandle samples = fg.AccessROBuffer(data->samples);
            const Ren::BufferHandle unif_sh_data = fg.AccessRWBuffer(data->shared_data);

            CopyBufferToBuffer(fg.ren_ctx().api(), fg.storages(), samples, 0, unif_sh_data,
                               offsetof(shared_data_t, sun_col), 3 * sizeof(float), fg.cmd_buf());
            CopyBufferToBuffer(fg.ren_ctx().api(), fg.storages(), samples, 4 * sizeof(float), unif_sh_data,
                               offsetof(shared_data_t, sun_col_point_sh), 3 * sizeof(float), fg.cmd_buf());
        });
    }
}

void Eng::Renderer::AddVolumetricPasses(const CommonBuffers &common_buffers, const AccelerationStructures &acc_structs,
                                        const FgBufROHandle rt_geo_instances_res,
                                        const FgBufROHandle rt_obj_instances_res, FrameTextures &frame_textures) {
    if (!ctx_.capabilities.hwrt && !ctx_.capabilities.swrt) {
        return;
    }

    using Stg = Ren::eStage;
    using Trg = Ren::eBindTarget;

    const int TileSize = (p_list_->render_settings.vol_quality == Eng::eVolQuality::Ultra) ? 8 : 12;
    const bool AllCascades = (p_list_->render_settings.vol_quality == Eng::eVolQuality::Ultra);

    FgImgRWHandle fr_emission, fr_scatter;
    { // Voxelize
        auto &voxelize = fg_builder_.AddNode("VOL VOXELIZE");

        auto *data = fg_builder_.AllocTempData<ExVolVoxelize::Args>();
        data->shared_data = voxelize.AddUniformBufferInput(common_buffers.shared_data, Stg::ComputeShader);
        data->stbn = voxelize.AddTextureInput(frame_textures.stbn_1D_64spp, Stg::ComputeShader);

        data->geo_data = voxelize.AddStorageReadonlyInput(rt_geo_instances_res, Stg::ComputeShader);
        data->materials = voxelize.AddStorageReadonlyInput(common_buffers.materials, Stg::ComputeShader);

        data->tlas_buf =
            voxelize.AddStorageReadonlyInput(acc_structs.rt_tlas_buf[int(eTLASIndex::Volume)], Stg::ComputeShader);
        data->tlas = acc_structs.rt_tlases[int(eTLASIndex::Volume)];

        if (!ctx_.capabilities.hwrt) {
            data->swrt.root_node = acc_structs.swrt.rt_root_node;
            data->swrt.rt_blas_buf = voxelize.AddStorageReadonlyInput(acc_structs.swrt.rt_blas_buf, Stg::ComputeShader);
            data->swrt.prim_ndx =
                voxelize.AddStorageReadonlyInput(acc_structs.swrt.rt_prim_indices, Stg::ComputeShader);
            data->swrt.mesh_instances = voxelize.AddStorageReadonlyInput(rt_obj_instances_res, Stg::ComputeShader);
            data->swrt.vtx_buf1 = voxelize.AddStorageReadonlyInput(common_buffers.vertex_buf1, Stg::ComputeShader);
            data->swrt.ndx_buf = voxelize.AddStorageReadonlyInput(common_buffers.indices_buf, Stg::ComputeShader);
        }

        { // Emission/Density and Scatter/Absorption textures
            FgImgDesc desc;
            desc.w = (view_state_.ren_res[0] + TileSize - 1) / TileSize;
            desc.h = (view_state_.ren_res[1] + TileSize - 1) / TileSize;
            desc.d = 144;
            desc.format = Ren::eFormat::RGBA16F;
            desc.sampling.filter = Ren::eFilter::Bilinear;
            desc.sampling.wrap = Ren::eWrap::ClampToEdge;

            fr_emission = data->out_emission =
                voxelize.AddStorageImageOutput("Vol Emission/Density", desc, Stg::ComputeShader);
            fr_scatter = data->out_scatter =
                voxelize.AddStorageImageOutput("Vol Scatter/Absorption", desc, Stg::ComputeShader);
        }

        voxelize.make_executor<ExVolVoxelize>(&p_list_, &view_state_, data);
    }
    { // Scatter
        auto &scatter = fg_builder_.AddNode("VOL SCATTER");

        struct PassData {
            FgBufROHandle shared_data;
            FgImgROHandle stbn;
            FgImgROHandle shadow_depth, shadow_color;
            FgImgROHandle fr_emission, fr_scatter;
            FgBufROHandle cells, items, lights, decals;
            FgImgROHandle envmap;
            FgImgROHandle irradiance, distance, offset;
            FgImgRWHandle out_emission, out_scatter;
        };

        auto *data = fg_builder_.AllocTempData<PassData>();
        data->shared_data = scatter.AddUniformBufferInput(common_buffers.shared_data, Stg::ComputeShader);
        data->stbn = scatter.AddTextureInput(frame_textures.stbn_1D_64spp, Stg::ComputeShader);
        data->shadow_depth = scatter.AddTextureInput(frame_textures.shadow_depth, Stg::ComputeShader);
        data->shadow_color = scatter.AddTextureInput(frame_textures.shadow_color, Stg::ComputeShader);

        fr_emission = data->out_emission = scatter.AddStorageImageOutput(fr_emission, Stg::ComputeShader);
        fr_scatter = data->out_scatter = scatter.AddStorageImageOutput(fr_scatter, Stg::ComputeShader);

        data->fr_emission = scatter.AddHistoryTextureInput(fr_emission, Stg::ComputeShader);
        data->fr_scatter = scatter.AddHistoryTextureInput(fr_scatter, Stg::ComputeShader);

        data->cells = scatter.AddStorageReadonlyInput(common_buffers.cells, Stg::ComputeShader);
        data->items = scatter.AddStorageReadonlyInput(common_buffers.items, Stg::ComputeShader);
        data->lights = scatter.AddStorageReadonlyInput(common_buffers.lights, Stg::ComputeShader);
        data->decals = scatter.AddStorageReadonlyInput(common_buffers.decals, Stg::ComputeShader);
        data->envmap = scatter.AddTextureInput(frame_textures.envmap, Stg::ComputeShader);

        if (settings.gi_quality != eGIQuality::Off) {
            data->irradiance = scatter.AddTextureInput(frame_textures.gi_cache_irradiance, Stg::ComputeShader);
            data->distance = scatter.AddTextureInput(frame_textures.gi_cache_distance, Stg::ComputeShader);
            data->offset = scatter.AddTextureInput(frame_textures.gi_cache_offset, Stg::ComputeShader);
        }

        scatter.set_execute_cb([data, this, AllCascades](const FgContext &fg) {
            const Ren::BufferROHandle unif_sh_data = fg.AccessROBuffer(data->shared_data);

            const Ren::ImageROHandle stbn = fg.AccessROImage(data->stbn);
            const Ren::ImageROHandle shadow_depth = fg.AccessROImage(data->shadow_depth);
            const Ren::ImageROHandle shadow_color = fg.AccessROImage(data->shadow_color);

            const Ren::ImageROHandle fr_emission = fg.AccessROImage(data->fr_emission);
            const Ren::ImageROHandle fr_scatter = fg.AccessROImage(data->fr_scatter);

            const Ren::BufferROHandle cells = fg.AccessROBuffer(data->cells);
            const Ren::BufferROHandle items = fg.AccessROBuffer(data->items);
            const Ren::BufferROHandle lights = fg.AccessROBuffer(data->lights);
            const Ren::BufferROHandle decals = fg.AccessROBuffer(data->decals);
            const Ren::ImageROHandle envmap = fg.AccessROImage(data->envmap);

            Ren::ImageROHandle irr, dist, off;
            if (data->irradiance) {
                irr = fg.AccessROImage(data->irradiance);
                dist = fg.AccessROImage(data->distance);
                off = fg.AccessROImage(data->offset);
            }

            const Ren::ImageRWHandle out_emission = fg.AccessRWImage(data->out_emission);
            const Ren::ImageRWHandle out_scatter = fg.AccessRWImage(data->out_scatter);

            if (view_state_.skip_volumetrics) {
                return;
            }

            Ren::SmallVector<Ren::Binding, 16> bindings = {{Trg::UBuf, BIND_UB_SHARED_DATA_BUF, unif_sh_data},
                                                           {Trg::TexSampled, Fog::STBN_TEX_SLOT, stbn},
                                                           {Trg::TexSampled, Fog::SHADOW_DEPTH_TEX_SLOT, shadow_depth},
                                                           {Trg::TexSampled, Fog::SHADOW_COLOR_TEX_SLOT, shadow_color},
                                                           {Trg::TexSampled, Fog::FR_EMISSION_TEX_SLOT, fr_emission},
                                                           {Trg::TexSampled, Fog::FR_SCATTER_TEX_SLOT, fr_scatter},
                                                           {Trg::UTBuf, Fog::CELLS_BUF_SLOT, cells},
                                                           {Trg::UTBuf, Fog::ITEMS_BUF_SLOT, items},
                                                           {Trg::UTBuf, Fog::LIGHT_BUF_SLOT, lights},
                                                           {Trg::UTBuf, Fog::DECAL_BUF_SLOT, decals},
                                                           {Trg::TexSampled, Fog::ENVMAP_TEX_SLOT, envmap},
                                                           {Trg::ImageRW, Fog::OUT_FR_EMISSION_IMG_SLOT, out_emission},
                                                           {Trg::ImageRW, Fog::OUT_FR_SCATTER_IMG_SLOT, out_scatter}};
            if (irr) {
                bindings.emplace_back(Ren::eBindTarget::TexSampled, Fog::IRRADIANCE_TEX_SLOT, irr);
                bindings.emplace_back(Ren::eBindTarget::TexSampled, Fog::DISTANCE_TEX_SLOT, dist);
                bindings.emplace_back(Ren::eBindTarget::TexSampled, Fog::OFFSET_TEX_SLOT, off);
            }

            const Ren::ImageCold &img_cold = fg.storages().images[out_emission].second;
            const auto froxel_res = Ren::Vec4u{img_cold.params.w, img_cold.params.h, img_cold.params.d, 0};

            const Ren::Vec3u grp_count = Ren::Vec3u(Ren::DivCeil(froxel_res[0], Fog::GRP_SIZE_3D_X) + 1,
                                                    Ren::DivCeil(froxel_res[1], Fog::GRP_SIZE_3D_Y) + 1,
                                                    Ren::DivCeil(froxel_res[2], Fog::GRP_SIZE_3D_Z) + 1);

            Fog::Params uniform_params;
            uniform_params.froxel_res = froxel_res;
            uniform_params.thread_offset = (view_state_.frame_index % 4) - 3;
            uniform_params.scatter_color =
                Ren::Saturate(Ren::Vec4f{p_list_->env.fog.scatter_color, p_list_->env.fog.absorption});
            uniform_params.emission_color =
                Ren::Vec4f{p_list_->env.fog.emission_color, std::max(p_list_->env.fog.density, 0.0f)};
            uniform_params.anisotropy = Ren::Clamp(p_list_->env.fog.anisotropy, -0.99f, 0.99f);
            uniform_params.bbox_min = Ren::Vec4f{p_list_->env.fog.bbox_min};
            uniform_params.bbox_max = Ren::Vec4f{p_list_->env.fog.bbox_max};
            uniform_params.frame_index = view_state_.frame_index;
            uniform_params.hist_weight = (view_state_.pre_exposure / view_state_.prev_pre_exposure);

            DispatchCompute(fg.cmd_buf(), pi_vol_scatter_[AllCascades][bool(irr)], fg.storages(), grp_count, bindings,
                            &uniform_params, sizeof(uniform_params), fg.descr_alloc(), fg.log());
        });
    }
    FgImgRWHandle froxel;
    { // Ray march
        auto &ray_march = fg_builder_.AddNode("VOL RAY MARCH");

        struct PassData {
            FgBufROHandle shared_data;
            FgImgROHandle fr_emission;
            FgImgROHandle fr_scatter;
            FgImgRWHandle output;
        };

        auto *data = fg_builder_.AllocTempData<PassData>();
        data->shared_data = ray_march.AddUniformBufferInput(common_buffers.shared_data, Stg::ComputeShader);
        data->fr_emission = ray_march.AddTextureInput(fr_emission, Stg::ComputeShader);
        data->fr_scatter = ray_march.AddTextureInput(fr_scatter, Stg::ComputeShader);

        { //
            FgImgDesc desc;
            desc.w = (view_state_.ren_res[0] + TileSize - 1) / TileSize;
            desc.h = (view_state_.ren_res[1] + TileSize - 1) / TileSize;
            desc.d = 144;
            desc.format = Ren::eFormat::RGBA16F;
            desc.sampling.filter = Ren::eFilter::Bilinear;
            desc.sampling.wrap = Ren::eWrap::ClampToEdge;

            froxel = data->output = ray_march.AddStorageImageOutput("Vol Scattering Final", desc, Stg::ComputeShader);
        }

        ray_march.set_execute_cb([data, this](const FgContext &fg) {
            const Ren::BufferROHandle unif_sh_data = fg.AccessROBuffer(data->shared_data);
            const Ren::ImageROHandle fr_emission = fg.AccessROImage(data->fr_emission);
            const Ren::ImageROHandle fr_scatter = fg.AccessROImage(data->fr_scatter);

            const Ren::ImageRWHandle output = fg.AccessRWImage(data->output);

            if (view_state_.skip_volumetrics) {
                return;
            }

            const Ren::Binding bindings[] = {{Trg::UBuf, BIND_UB_SHARED_DATA_BUF, unif_sh_data},
                                             {Trg::TexSampled, Fog::FR_EMISSION_TEX_SLOT, fr_emission},
                                             {Trg::TexSampled, Fog::FR_SCATTER_TEX_SLOT, fr_scatter},
                                             {Trg::ImageRW, Fog::OUT_FR_FINAL_IMG_SLOT, output}};

            const Ren::ImageCold &img_cold = fg.storages().images[output].second;
            const auto froxel_res = Ren::Vec4u{img_cold.params.w, img_cold.params.h, img_cold.params.d, 0};

            const Ren::Vec3u grp_count = Ren::Vec3u(Ren::DivCeil(froxel_res[0], Fog::GRP_SIZE_2D_X),
                                                    Ren::DivCeil(froxel_res[1], Fog::GRP_SIZE_2D_Y), 1);

            Fog::Params uniform_params;
            uniform_params.froxel_res = froxel_res;
            uniform_params.scatter_color =
                Ren::Saturate(Ren::Vec4f{p_list_->env.fog.scatter_color, p_list_->env.fog.absorption});

            DispatchCompute(fg.cmd_buf(), pi_vol_ray_march_, fg.storages(), grp_count, bindings, &uniform_params,
                            sizeof(uniform_params), fg.descr_alloc(), fg.log());
        });
    }
    { // Apply
        auto &apply = fg_builder_.AddNode("VOL APPLY");

        struct PassData {
            FgBufROHandle shared_data;
            FgImgROHandle depth;
            FgImgROHandle froxel;
            FgImgRWHandle output;
        };

        auto *data = fg_builder_.AllocTempData<PassData>();
        data->shared_data = apply.AddUniformBufferInput(common_buffers.shared_data, Stg::FragmentShader);
        data->depth = apply.AddTextureInput(frame_textures.depth, Stg::FragmentShader);
        data->froxel = apply.AddTextureInput(froxel, Stg::FragmentShader);

        frame_textures.color = data->output = apply.AddColorOutput(frame_textures.color);

        apply.set_execute_cb([data, this](const FgContext &fg) {
            const Ren::BufferROHandle unif_sh_data = fg.AccessROBuffer(data->shared_data);
            const Ren::ImageROHandle depth = fg.AccessROImage(data->depth);
            const Ren::ImageROHandle froxel = fg.AccessROImage(data->froxel);

            const Ren::ImageRWHandle output = fg.AccessRWImage(data->output);

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

            const Ren::Binding bindings[] = {{Trg::UBuf, BIND_UB_SHARED_DATA_BUF, unif_sh_data},
                                             {Trg::TexSampled, VolCompose::DEPTH_TEX_SLOT, {depth, 1}},
                                             {Trg::TexSampled, VolCompose::FROXELS_TEX_SLOT, froxel}};

            // TODO: Get rid of this!
            const Ren::ImageCold &depth_cold = fg.storages().images[depth].second;
            const auto img_res = Ren::Vec2i{depth_cold.params.w, depth_cold.params.h};

            const Ren::ImageCold &img_cold = fg.storages().images[froxel].second;
            const auto froxel_res = Ren::Vec4i{img_cold.params.w, img_cold.params.h, img_cold.params.d, 0};

            const Ren::RenderTarget render_targets[] = {{output, Ren::eLoadOp::Load, Ren::eStoreOp::Store}};

            VolCompose::Params uniform_params;
            uniform_params.transform = Ren::Vec4f{0.0f, 0.0f, 1.0f, 1.0f};
            uniform_params.froxel_res = Ren::Vec4f(froxel_res);

            prim_draw_.DrawPrim(fg.cmd_buf(), PrimDraw::ePrim::Quad, blit_vol_compose_prog_, {}, render_targets,
                                rast_state, fg.rast_state(), bindings, &uniform_params, sizeof(uniform_params), 0,
                                fg.framebuffers());
        });
    }
}
