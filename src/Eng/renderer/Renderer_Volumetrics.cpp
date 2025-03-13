#include "Renderer.h"

#include <Ren/Context.h>
#include <Sys/AssetFile.h>

#include "../utils/Load.h"
#include "Renderer_Names.h"

#include "shaders/blit_fxaa_interface.h"
#include "shaders/skydome_interface.h"

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
                sky_transmittance_lut_ =
                    ctx_.LoadTexture2D("Sky Transmittance LUT", p, ctx_.default_mem_allocs(), &status);
                assert(status == Ren::eTexLoadStatus::CreatedDefault);

                Ren::Buffer stage_buf("Temp Stage Buf", ctx_.api_ctx(), Ren::eBufType::Upload,
                                      4 * SKY_TRANSMITTANCE_LUT_W * SKY_TRANSMITTANCE_LUT_H * sizeof(float));
                { // init stage buf
                    uint8_t *mapped_ptr = stage_buf.Map();
                    memcpy(mapped_ptr, transmittance_lut.data(),
                           4 * SKY_TRANSMITTANCE_LUT_W * SKY_TRANSMITTANCE_LUT_H * sizeof(float));
                    stage_buf.Unmap();
                }

                sky_transmittance_lut_->SetSubImage(
                    0, 0, 0, SKY_TRANSMITTANCE_LUT_W, SKY_TRANSMITTANCE_LUT_H, Ren::eTexFormat::RGBA32F, stage_buf,
                    ctx_.current_cmd_buf(), 0, 4 * SKY_TRANSMITTANCE_LUT_W * SKY_TRANSMITTANCE_LUT_H * sizeof(float));
            }
            { // Init multiscatter LUT
                Ren::TexParams p;
                p.w = p.h = SKY_MULTISCATTER_LUT_RES;
                p.format = Ren::eTexFormat::RGBA32F;
                p.usage = Ren::Bitmask(Ren::eTexUsage::Transfer) | Ren::eTexUsage::Sampled;
                p.sampling.filter = Ren::eTexFilter::Bilinear;
                p.sampling.wrap = Ren::eTexWrap::ClampToEdge;

                Ren::eTexLoadStatus status;
                sky_multiscatter_lut_ =
                    ctx_.LoadTexture2D("Sky Multiscatter LUT", p, ctx_.default_mem_allocs(), &status);
                assert(status == Ren::eTexLoadStatus::CreatedDefault);

                const std::vector<Ren::Vec4f> multiscatter_lut =
                    Generate_SkyMultiscatterLUT(p_list_->env.atmosphere, transmittance_lut);

                Ren::Buffer stage_buf("Temp Stage Buf", ctx_.api_ctx(), Ren::eBufType::Upload,
                                      4 * SKY_MULTISCATTER_LUT_RES * SKY_MULTISCATTER_LUT_RES * sizeof(float));
                { // init stage buf
                    uint8_t *mapped_ptr = stage_buf.Map();
                    memcpy(mapped_ptr, multiscatter_lut.data(),
                           4 * SKY_MULTISCATTER_LUT_RES * SKY_MULTISCATTER_LUT_RES * sizeof(float));
                    stage_buf.Unmap();
                }

                sky_multiscatter_lut_->SetSubImage(
                    0, 0, 0, SKY_MULTISCATTER_LUT_RES, SKY_MULTISCATTER_LUT_RES, Ren::eTexFormat::RGBA32F, stage_buf,
                    ctx_.current_cmd_buf(), 0, 4 * SKY_MULTISCATTER_LUT_RES * SKY_MULTISCATTER_LUT_RES * sizeof(float));
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
                    sky_moon_tex_ = ctx_.LoadTexture2D(moon_diff, data, p, ctx_.default_mem_allocs(), &status);
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
                    sky_weather_tex_ = ctx_.LoadTexture2D(weather, data, p, ctx_.default_mem_allocs(), &status);
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
                    sky_cirrus_tex_ = ctx_.LoadTexture2D(cirrus, data, p, ctx_.default_mem_allocs(), &status);
                    assert(status == Ren::eTexLoadStatus::CreatedFromData);
                } else {
                    ctx_.log()->Error("Failed to load %s", cirrus.data());
                }
            }
            { // Init Curl texture
                const std::string_view curl = "assets_pc/textures/internal/curl.dds";

                Ren::TexParams p;
                p.flags = Ren::eTexFlags::SRGB;
                p.usage = Ren::Bitmask(Ren::eTexUsage::Transfer) | Ren::eTexUsage::Sampled;
                p.sampling.filter = Ren::eTexFilter::Bilinear;
                p.sampling.wrap = Ren::eTexWrap::Repeat;

                const std::vector<uint8_t> data = LoadDDS(curl, &p);
                if (!data.empty()) {
                    Ren::eTexLoadStatus status;
                    sky_curl_tex_ = ctx_.LoadTexture2D(curl, data, p, ctx_.default_mem_allocs(), &status);
                    assert(status == Ren::eTexLoadStatus::CreatedFromData);
                } else {
                    ctx_.log()->Error("Failed to load %s", curl.data());
                }
            }
            { // Init 3d noise texture
                Sys::AssetFile noise_tex("assets_pc/textures/internal/3dnoise.dds");
                std::vector<uint8_t> data(noise_tex.size());
                noise_tex.Read((char *)&data[0], noise_tex.size());

                Ren::DDSHeader header = {};
                memcpy(&header, &data[0], sizeof(Ren::DDSHeader));

                const uint32_t data_len = header.dwWidth * header.dwHeight * header.dwDepth;

                Ren::TexParams params;
                params.w = header.dwWidth;
                params.h = header.dwHeight;
                params.d = header.dwDepth;
                params.format = Ren::eTexFormat::R8;
                params.usage = Ren::Bitmask(Ren::eTexUsage::Sampled) | Ren::eTexUsage::Transfer;
                params.sampling.filter = Ren::eTexFilter::Bilinear;
                params.sampling.wrap = Ren::eTexWrap::Repeat;

                Ren::eTexLoadStatus status;
                sky_noise3d_tex_ = ctx_.LoadTexture3D("Noise 3d Tex", params, ctx_.default_mem_allocs(), &status);
                assert(status == Ren::eTexLoadStatus::CreatedDefault);

                Ren::Buffer stage_buf = Ren::Buffer("Temp stage buf", ctx_.api_ctx(), Ren::eBufType::Upload, data_len);
                uint8_t *mapped_ptr = stage_buf.Map();
                memcpy(mapped_ptr, &data[0] + sizeof(Ren::DDSHeader), data_len);
                stage_buf.Unmap();

                sky_noise3d_tex_->SetSubImage(0, 0, 0, int(header.dwWidth), int(header.dwHeight), int(header.dwDepth),
                                              Ren::eTexFormat::R8, stage_buf, ctx_.current_cmd_buf(), 0, int(data_len));
            }
        }
    }
}

void Eng::Renderer::AddSkydomePass(const CommonBuffers &common_buffers, FrameTextures &frame_textures) {
    using Stg = Ren::eStageBits;
    using Trg = Ren::eBindTarget;

    // TODO: Remove this condition
    if (!p_list_->env.env_map) {
        return;
    }

    if (p_list_->env.env_map_name == "physical_sky") {
        auto &skydome_cube = fg_builder_.AddNode("SKYDOME CUBE");

        auto *data = skydome_cube.AllocNodeData<ExSkydomeCube::Args>();

        data->shared_data =
            skydome_cube.AddUniformBufferInput(common_buffers.shared_data, Stg::VertexShader | Stg::FragmentShader);

        data->transmittance_lut = skydome_cube.AddTextureInput(sky_transmittance_lut_, Stg::FragmentShader);
        data->multiscatter_lut = skydome_cube.AddTextureInput(sky_multiscatter_lut_, Stg::FragmentShader);
        data->moon_tex = skydome_cube.AddTextureInput(sky_moon_tex_, Stg::FragmentShader);
        data->weather_tex = skydome_cube.AddTextureInput(sky_weather_tex_, Stg::FragmentShader);
        data->cirrus_tex = skydome_cube.AddTextureInput(sky_cirrus_tex_, Stg::FragmentShader);
        data->curl_tex = skydome_cube.AddTextureInput(sky_curl_tex_, Stg::FragmentShader);
        data->noise3d_tex = skydome_cube.AddTextureInput(sky_noise3d_tex_.get(), Stg::FragmentShader);
        frame_textures.envmap = data->color_tex = skydome_cube.AddColorOutput(p_list_->env.env_map);

        ex_skydome_cube_.Setup(&view_state_, data);
        skydome_cube.set_executor(&ex_skydome_cube_);
    }

    FgResRef sky_temp;
    { // Main pass
        auto &skymap = fg_builder_.AddNode("SKYDOME");

        auto *data = skymap.AllocNodeData<ExSkydomeScreen::Args>();
        data->shared_data =
            skymap.AddUniformBufferInput(common_buffers.shared_data, Stg::VertexShader | Stg::FragmentShader);
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
            data->phys.noise3d_tex = skymap.AddTextureInput(sky_noise3d_tex_.get(), Stg::FragmentShader);

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

        ex_skydome_.Setup(&view_state_, data);
        skymap.set_executor(&ex_skydome_);
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
                                                 {Trg::Tex2DSampled, Skydome::ENV_TEX_SLOT, *env_map_tex.ref},
                                                 {Trg::Tex2DSampled, Skydome::SKY_TEX_SLOT, *sky_temp_tex.ref},
                                                 {Trg::Tex2DSampled, Skydome::SKY_HIST_TEX_SLOT, *sky_hist_tex.ref},
                                                 {Trg::Image2D, Skydome::OUT_IMG_SLOT, *output_tex.ref}};

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

                const Ren::Binding bindings[] = {{Trg::Tex2DSampled, FXAA::INPUT_TEX_SLOT, *sky_tex.ref}};

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