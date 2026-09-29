#include "Renderer.h"

#include <Ren/Context.h>

#include <algorithm>
#include <cmath>

#include "Renderer_Names.h"

#include "shaders/bloom_interface.h"
#include "shaders/dof_interface.h"
#include "shaders/histogram_exposure_interface.h"
#include "shaders/histogram_sample_interface.h"
#include "shaders/sharpen_interface.h"

Eng::FgImgRWHandle Eng::Renderer::AddAutoexposurePasses(const FgImgROHandle hdr_texture,
                                                        const Ren::Vec2f adaptation_speed) {
    FgImgRWHandle histogram;
    { // Clear histogram image
        auto &histogram_clear = fg_builder_.AddNode("HISTOGRAM CLEAR");

        FgImgDesc desc;
        desc.w = EXPOSURE_HISTOGRAM_RES + 1;
        desc.h = 1;
        desc.format = Ren::eFormat::R32UI;
        desc.sampling.wrap = Ren::eWrap::ClampToEdge;

        histogram = histogram_clear.AddClearImageOutput("Exposure Histogram", desc);

        histogram_clear.set_execute_cb([histogram](const FgContext &fg) {
            const Ren::ImageRWHandle _histogram = fg.AccessRWImage(histogram);

            fg.ren_ctx().CmdClearImage(_histogram, {}, fg.cmd_buf());
        });
    }
    { // Sample histogram
        auto &histogram_sample = fg_builder_.AddNode("HISTOGRAM SAMPLE");

        FgImgROHandle input = histogram_sample.AddTextureInput(hdr_texture, Ren::eStage::ComputeShader);
        histogram = histogram_sample.AddStorageImageOutput(histogram, Ren::eStage::ComputeShader);

        histogram_sample.set_execute_cb([this, input, histogram](const FgContext &fg) {
            const Ren::ImageROHandle _input = fg.AccessROImage(input);

            const Ren::ImageRWHandle output = fg.AccessRWImage(histogram);

            const Ren::Binding bindings[] = {
                {Ren::eBindTarget::TexSampled, HistogramSample::HDR_TEX_SLOT, {_input, linear_sampler_}},
                {Ren::eBindTarget::ImageRW, HistogramSample::OUT_IMG_SLOT, output}};

            HistogramSample::Params uniform_params = {};
            uniform_params.pre_exposure = view_state_.pre_exposure;

            DispatchCompute(fg.cmd_buf(), pi_histogram_sample_, fg.storages(), Ren::Vec3u{16, 8, 1}, bindings,
                            &uniform_params, sizeof(uniform_params), fg.descr_alloc(), fg.log());
        });
    }
    FgImgRWHandle exposure;
    { // Calc exposure
        auto &histogram_exposure = fg_builder_.AddNode("HISTOGRAM EXPOSURE");

        struct PassData {
            FgImgROHandle histogram;
            FgImgROHandle exposure_prev;
            FgImgRWHandle exposure;
        };

        auto *data = fg_builder_.AllocTempData<PassData>();
        data->histogram = histogram_exposure.AddTextureInput(histogram, Ren::eStage::ComputeShader);

        FgImgDesc params;
        params.w = params.h = 1;
        params.format = Ren::eFormat::R32F;
        params.sampling.wrap = Ren::eWrap::ClampToEdge;
        exposure = data->exposure =
            histogram_exposure.AddStorageImageOutput(EXPOSURE_TEX, params, Ren::eStage::ComputeShader);
        data->exposure_prev = histogram_exposure.AddHistoryTextureInput(exposure, Ren::eStage::ComputeShader);

        histogram_exposure.set_execute_cb([this, data, adaptation_speed](const FgContext &fg) {
            const Ren::ImageROHandle histogram = fg.AccessROImage(data->histogram);
            const Ren::ImageROHandle exposure_prev = fg.AccessROImage(data->exposure_prev);

            const Ren::ImageRWHandle exposure = fg.AccessRWImage(data->exposure);

            const Ren::Binding bindings[] = {
                {Ren::eBindTarget::TexSampled, HistogramExposure::HISTOGRAM_TEX_SLOT, histogram},
                {Ren::eBindTarget::TexSampled, HistogramExposure::EXPOSURE_PREV_TEX_SLOT, exposure_prev},
                {Ren::eBindTarget::ImageRW, HistogramExposure::OUT_TEX_SLOT, exposure}};

            HistogramExposure::Params uniform_params = {};
            uniform_params.min_exposure = min_exposure_;
            uniform_params.max_exposure = max_exposure_;
            uniform_params.exposure_factor = (settings.tonemap_mode != Eng::eTonemapMode::Standard) ? 1.25f : 0.5f;
            uniform_params.adaptation_speed_max = adaptation_speed[0];
            uniform_params.adaptation_speed_min = adaptation_speed[1];

            DispatchCompute(fg.cmd_buf(), pi_histogram_exposure_, fg.storages(), Ren::Vec3u{1}, bindings,
                            &uniform_params, sizeof(uniform_params), fg.descr_alloc(), fg.log());
        });
    }
    return exposure;
}

Eng::FgImgRWHandle Eng::Renderer::AddBloomPasses(const FgImgROHandle hdr_texture, const FgImgROHandle exposure_texture,
                                                 const bool compressed) {
    static const int BloomMipCount = 5;

    FgImgRWHandle downsampled[BloomMipCount];
    for (int mip = 0; mip < BloomMipCount; ++mip) {
        const std::string node_name = "BLOOM DOWNS. " + std::to_string(mip) + "->" + std::to_string(mip + 1);
        auto &bloom_downsample = fg_builder_.AddNode(node_name);

        struct PassData {
            FgImgROHandle input;
            FgImgROHandle exposure;
            FgImgRWHandle output;
        };

        auto *data = fg_builder_.AllocTempData<PassData>();
        if (mip == 0) {
            data->input = bloom_downsample.AddTextureInput(hdr_texture, Ren::eStage::ComputeShader);
        } else {
            data->input = bloom_downsample.AddTextureInput(downsampled[mip - 1], Ren::eStage::ComputeShader);
        }
        data->exposure = bloom_downsample.AddTextureInput(exposure_texture, Ren::eStage::ComputeShader);

        { // Image that holds downsampled bloom image
            FgImgDesc desc;
            desc.w = (view_state_.out_res[0] / 2) >> mip;
            desc.h = (view_state_.out_res[1] / 2) >> mip;
            desc.format = compressed ? Ren::eFormat::RGBA16F : Ren::eFormat::RGBA32F;
            desc.sampling.filter = Ren::eFilter::Bilinear;
            desc.sampling.wrap = Ren::eWrap::ClampToEdge;

            const std::string output_name = "Bloom Downsampled " + std::to_string(mip);
            downsampled[mip] = data->output =
                bloom_downsample.AddStorageImageOutput(output_name, desc, Ren::eStage::ComputeShader);
        }

        bloom_downsample.set_execute_cb([this, data, mip, compressed](const FgContext &fg) {
            const Ren::ImageROHandle input = fg.AccessROImage(data->input);
            const Ren::ImageROHandle exposure = fg.AccessROImage(data->exposure);

            const Ren::ImageRWHandle output = fg.AccessRWImage(data->output);

            Bloom::Params uniform_params;
            uniform_params.img_size[0] = (view_state_.out_res[0] / 2) >> mip;
            uniform_params.img_size[1] = (view_state_.out_res[1] / 2) >> mip;
            uniform_params.pre_exposure = view_state_.pre_exposure;

            const Ren::Binding bindings[] = {
                {Ren::eBindTarget::TexSampled, Bloom::INPUT_TEX_SLOT, {input, linear_sampler_}},
                {Ren::eBindTarget::TexSampled, Bloom::EXPOSURE_TEX_SLOT, exposure},
                {Ren::eBindTarget::ImageRW, Bloom::OUT_IMG_SLOT, output}};

            const Ren::Vec3u grp_count =
                Ren::Vec3u{(uniform_params.img_size[0] + Bloom::GRP_SIZE_X - 1u) / Bloom::GRP_SIZE_X,
                           (uniform_params.img_size[1] + Bloom::GRP_SIZE_Y - 1u) / Bloom::GRP_SIZE_Y, 1u};

            DispatchCompute(fg.cmd_buf(), pi_bloom_downsample_[compressed][mip == 0], fg.storages(), grp_count,
                            bindings, &uniform_params, sizeof(uniform_params), fg.descr_alloc(), fg.log());
        });
    }

    FgImgRWHandle upsampled[BloomMipCount - 1];
    for (int mip = BloomMipCount - 2; mip >= 0; --mip) {
        const std::string node_name = "BLOOM UPS. " + std::to_string(mip + 2) + "->" + std::to_string(mip + 1);
        auto &bloom_upsample = fg_builder_.AddNode(node_name);

        struct PassData {
            FgImgROHandle input;
            FgImgROHandle blend;
            FgImgRWHandle output;
        };

        auto *data = fg_builder_.AllocTempData<PassData>();
        if (mip == BloomMipCount - 2) {
            data->input = bloom_upsample.AddTextureInput(downsampled[mip + 1], Ren::eStage::ComputeShader);
        } else {
            data->input = bloom_upsample.AddTextureInput(upsampled[mip + 1], Ren::eStage::ComputeShader);
        }
        data->blend = bloom_upsample.AddTextureInput(downsampled[mip], Ren::eStage::ComputeShader);

        { // Image that holds upsampled bloom image
            FgImgDesc desc;
            desc.w = (view_state_.out_res[0] / 2) >> mip;
            desc.h = (view_state_.out_res[1] / 2) >> mip;
            desc.format = compressed ? Ren::eFormat::RGBA16F : Ren::eFormat::RGBA32F;
            desc.sampling.filter = Ren::eFilter::Bilinear;
            desc.sampling.wrap = Ren::eWrap::ClampToEdge;

            const std::string output_name = "Bloom Upsampled " + std::to_string(mip);
            upsampled[mip] = data->output =
                bloom_upsample.AddStorageImageOutput(output_name, desc, Ren::eStage::ComputeShader);
        }

        bloom_upsample.set_execute_cb([this, data, mip, compressed](const FgContext &fg) {
            const Ren::ImageROHandle input = fg.AccessROImage(data->input);
            const Ren::ImageROHandle blend = fg.AccessROImage(data->blend);

            const Ren::ImageRWHandle output = fg.AccessRWImage(data->output);

            Bloom::Params uniform_params;
            uniform_params.img_size[0] = (view_state_.out_res[0] / 2) >> mip;
            uniform_params.img_size[1] = (view_state_.out_res[1] / 2) >> mip;
            uniform_params.blend_weight = 1.0f / float(1 + BloomMipCount - 1 - mip);

            const Ren::Binding bindings[] = {{Ren::eBindTarget::TexSampled, Bloom::INPUT_TEX_SLOT, input},
                                             {Ren::eBindTarget::TexSampled, Bloom::BLEND_TEX_SLOT, blend},
                                             {Ren::eBindTarget::ImageRW, Bloom::OUT_IMG_SLOT, output}};

            const Ren::Vec3u grp_count =
                Ren::Vec3u{(uniform_params.img_size[0] + Bloom::GRP_SIZE_X - 1u) / Bloom::GRP_SIZE_X,
                           (uniform_params.img_size[1] + Bloom::GRP_SIZE_Y - 1u) / Bloom::GRP_SIZE_Y, 1u};

            DispatchCompute(fg.cmd_buf(), pi_bloom_upsample_[compressed], fg.storages(), grp_count, bindings,
                            &uniform_params, sizeof(uniform_params), fg.descr_alloc(), fg.log());
        });
    }

    return upsampled[0];
}

Eng::FgImgRWHandle Eng::Renderer::AddSharpenPass(const FgImgROHandle input_tex, const FgImgROHandle exposure_tex,
                                                 const bool compressed) {
    auto &sharpen = fg_builder_.AddNode("SHARPEN");

    struct PassData {
        FgImgROHandle input;
        FgImgROHandle exposure;
        FgImgRWHandle output;
    };

    auto *data = fg_builder_.AllocTempData<PassData>();
    data->input = sharpen.AddTextureInput(input_tex, Ren::eStage::ComputeShader);
    data->exposure = sharpen.AddTextureInput(exposure_tex, Ren::eStage::ComputeShader);

    FgImgRWHandle output;
    { // Image that holds output image
        FgImgDesc desc;
        desc.w = view_state_.out_res[0];
        desc.h = view_state_.out_res[1];
        desc.format = compressed ? Ren::eFormat::RGBA16F : Ren::eFormat::RGBA32F;
        desc.sampling.filter = Ren::eFilter::Bilinear;
        desc.sampling.wrap = Ren::eWrap::ClampToEdge;

        output = data->output = sharpen.AddStorageImageOutput("Sharpen Output", desc, Ren::eStage::ComputeShader);
    }

    sharpen.set_execute_cb([this, data, compressed](const FgContext &fg) {
        const Ren::ImageROHandle input = fg.AccessROImage(data->input);
        const Ren::ImageROHandle exposure = fg.AccessROImage(data->exposure);

        const Ren::ImageRWHandle output = fg.AccessRWImage(data->output);

        Sharpen::Params uniform_params;
        uniform_params.img_size[0] = view_state_.out_res[0];
        uniform_params.img_size[1] = view_state_.out_res[1];
        uniform_params.sharpness = 0.15f; // hardcoded for now
        uniform_params.pre_exposure = view_state_.pre_exposure;

        const Ren::Binding bindings[] = {
            {Ren::eBindTarget::TexSampled, Sharpen::INPUT_TEX_SLOT, {input, linear_sampler_}},
            {Ren::eBindTarget::TexSampled, Sharpen::EXPOSURE_TEX_SLOT, exposure},
            {Ren::eBindTarget::ImageRW, Sharpen::OUT_IMG_SLOT, output}};

        const Ren::Vec3u grp_count =
            Ren::Vec3u{(uniform_params.img_size[0] + Sharpen::GRP_SIZE_X - 1u) / Sharpen::GRP_SIZE_X,
                       (uniform_params.img_size[1] + Sharpen::GRP_SIZE_Y - 1u) / Sharpen::GRP_SIZE_Y, 1u};

        DispatchCompute(fg.cmd_buf(), pi_sharpen_[compressed], fg.storages(), grp_count, bindings, &uniform_params,
                        sizeof(uniform_params), fg.descr_alloc(), fg.log());
    });

    return output;
}

Eng::FgImgRWHandle Eng::Renderer::AddDofPasses(FgImgROHandle input, const CommonBuffers &common_buffers,
                                               FrameTextures &frame_textures) {
    using namespace Dof;

    // Port of "Life of a bokeh" (K. Ilich, GPU Pro 5): classify -> dilate -> presort -> filter -> median -> upsample.
    // Runs on the resolved color at output resolution; temporal stability is provided by the TSR pass upstream.

    const Ren::Camera &cam = p_list_->draw_cam;
    const float full_w = float(view_state_.out_res[0]), full_h = float(view_state_.out_res[1]);

    // COC radius of the farthest out-of-focus pixel, in halfres pixels
    constexpr float MAX_SCREEN_FRACTION = 0.01f;
    const float focal_len = cam.focal_length();
    const float coc_factor = full_w / 2.0f * focal_len * (0.5f * focal_len / cam.fstop) / cam.sensor_height;
    const float max_kernel_radius = std::min(MAX_SCREEN_FRACTION * full_w / 2.0f, float(4 * TILE_RES));

    // Per-frame rotation of the sampling patterns (different sequences per pass to avoid correlations)
    const float fi = float(view_state_.frame_index);
    auto golden_angle = [](float v) {
        return float(std::modf(v, &v)) * 6.283185307179586477f; // frac(v) * 2pi
    };
    const uint32_t seed = uint32_t(view_state_.frame_index) * 0x9E3779B9u + 0x12345678u;

    Params base_params;
    base_params.focal_params = Ren::Vec4f{cam.focus_distance, coc_factor, focal_len, max_kernel_radius};
    base_params.img_size = Ren::Vec4f{};
    base_params.depth_size = Ren::Vec4f{float(frame_textures.depth_desc.w), float(frame_textures.depth_desc.h), 0.0f,
                                        0.0f};
    base_params.clip_info = view_state_.clip_info;
    base_params.rot = Ren::Vec4f{1.0f, 0.0f, 0.0f, 1.0f};
    base_params.rot_angle = golden_angle(fi * 0.6180339887498948482f);
    base_params.seed = seed;

    auto make_rotation = [](float angle) {
        const float ca = std::cos(angle), sa = std::sin(angle);
        return Ren::Vec4f{ca, sa, -sa, ca};
    };

    FgImgRWHandle coc_tiles;
    { // Tile classification (max depth, max COC, min depth per TILE_RES x TILE_RES tile)
        auto &classify = fg_builder_.AddNode("DOF CLASSIFY");

        struct PassData {
            FgImgROHandle in_depth;
            FgBufROHandle in_random_seq;
            FgImgRWHandle out_tiles;
            Params params;
        };

        auto *data = fg_builder_.AllocTempData<PassData>();
        data->in_depth = classify.AddTextureInput(frame_textures.depth, Ren::eStage::ComputeShader);
        data->in_random_seq = classify.AddStorageReadonlyInput(common_buffers.pmj_samples, Ren::eStage::ComputeShader);

        const uint32_t tiles_w = Ren::DivCeil<uint32_t>(view_state_.out_res[0], TILE_RES);
        const uint32_t tiles_h = Ren::DivCeil<uint32_t>(view_state_.out_res[1], TILE_RES);

        { // Image that holds COC tiles
            FgImgDesc desc;
            desc.w = tiles_w;
            desc.h = tiles_h;
            desc.format = Ren::eFormat::RGBA16F;
            desc.sampling.filter = Ren::eFilter::Bilinear;
            desc.sampling.wrap = Ren::eWrap::ClampToEdge;

            coc_tiles = data->out_tiles =
                classify.AddStorageImageOutput("DOF Coc Tiles", desc, Ren::eStage::ComputeShader);
        }

        data->params = base_params;
        data->params.img_size = Ren::Vec4f{float(tiles_w), float(tiles_h), full_w, full_h};

        classify.set_execute_cb([this, data](const FgContext &fg) {
            const Ren::ImageROHandle in_depth = fg.AccessROImage(data->in_depth);
            const Ren::BufferROHandle in_random_seq = fg.AccessROBuffer(data->in_random_seq);
            const Ren::ImageRWHandle out_tiles = fg.AccessRWImage(data->out_tiles);

            const Ren::Binding bindings[] = {{Ren::eBindTarget::TexSampled, DEPTH_TEX_SLOT, {in_depth, 1}},
                                             {Ren::eBindTarget::UTBuf, RANDOM_SEQ_BUF_SLOT, in_random_seq},
                                             {Ren::eBindTarget::ImageRW, OUT_IMG_SLOT, out_tiles}};

            const auto grp_count = Ren::Vec3u{Ren::DivCeil<uint32_t>(uint32_t(data->params.img_size[0]), GRP_SIZE_X),
                                              Ren::DivCeil<uint32_t>(uint32_t(data->params.img_size[1]), GRP_SIZE_Y), 1u};

            DispatchCompute(fg.cmd_buf(), pi_dof_classify_, fg.storages(), grp_count, bindings, &data->params,
                            sizeof(Params), fg.descr_alloc(), fg.log());
        });
    }
    { // COC dilation, one tile per iteration so that kernels up to max_kernel_radius find their data
        int n_dilate = std::clamp(int(std::ceil(max_kernel_radius / TILE_RES)), 1, 4);

        FgImgRWHandle in_tiles = coc_tiles;
        for (int i = 0; i < n_dilate; ++i) {
            auto &dilate = fg_builder_.AddNode("DOF DILATE");

            struct PassData {
                FgImgROHandle in_tiles;
                FgImgRWHandle out_tiles;
                Params params;
            };

            auto *data = fg_builder_.AllocTempData<PassData>();
            data->in_tiles = dilate.AddTextureInput(in_tiles, Ren::eStage::ComputeShader);

            const uint32_t tiles_w = Ren::DivCeil<uint32_t>(view_state_.out_res[0], TILE_RES);
            const uint32_t tiles_h = Ren::DivCeil<uint32_t>(view_state_.out_res[1], TILE_RES);

            FgImgDesc desc;
            desc.w = tiles_w;
            desc.h = tiles_h;
            desc.format = Ren::eFormat::RGBA16F;
            desc.sampling.filter = Ren::eFilter::Bilinear;
            desc.sampling.wrap = Ren::eWrap::ClampToEdge;

            FgImgRWHandle out_tiles = dilate.AddStorageImageOutput("DOF Coc Dilated", desc, Ren::eStage::ComputeShader);
            data->out_tiles = out_tiles;
            in_tiles = out_tiles;

            data->params = base_params;
            data->params.img_size = Ren::Vec4f{float(tiles_w), float(tiles_h), float(tiles_w), float(tiles_h)};

            dilate.set_execute_cb([this, data](const FgContext &fg) {
                const Ren::ImageROHandle in_tiles = fg.AccessROImage(data->in_tiles);
                const Ren::ImageRWHandle out_tiles = fg.AccessRWImage(data->out_tiles);

                const Ren::Binding bindings[] = {{Ren::eBindTarget::TexSampled, COC_TILES_TEX_SLOT, in_tiles},
                                                 {Ren::eBindTarget::ImageRW, OUT_IMG_SLOT, out_tiles}};

                const auto grp_count = Ren::Vec3u{Ren::DivCeil<uint32_t>(uint32_t(data->params.img_size[0]), GRP_SIZE_X),
                                                  Ren::DivCeil<uint32_t>(uint32_t(data->params.img_size[1]), GRP_SIZE_Y), 1u};

                DispatchCompute(fg.cmd_buf(), pi_dof_dilate_, fg.storages(), grp_count, bindings, &data->params,
                                sizeof(Params), fg.descr_alloc(), fg.log());
            });
        }
        coc_tiles = in_tiles;
    }

    FgImgRWHandle downsampled_scene, presorted_data;
    { // Foreground/background separation + color downsampling to halfres
        auto &presort = fg_builder_.AddNode("DOF PRESORT");

        struct PassData {
            FgImgROHandle in_color;
            FgImgROHandle in_depth;
            FgImgROHandle in_tiles;
            FgBufROHandle in_random_seq;
            FgImgRWHandle out_presort;
            FgImgRWHandle out_downsampled;
            Params params;
        };

        auto *data = fg_builder_.AllocTempData<PassData>();
        data->in_color = presort.AddTextureInput(input, Ren::eStage::ComputeShader);
        data->in_depth = presort.AddTextureInput(frame_textures.depth, Ren::eStage::ComputeShader);
        data->in_tiles = presort.AddTextureInput(coc_tiles, Ren::eStage::ComputeShader);
        data->in_random_seq = presort.AddStorageReadonlyInput(common_buffers.pmj_samples, Ren::eStage::ComputeShader);

        const uint32_t half_w = (view_state_.out_res[0] + 1) / 2;
        const uint32_t half_h = (view_state_.out_res[1] + 1) / 2;

        { // Halfres presort data (signed COC, foreground/background weights)
            FgImgDesc desc;
            desc.w = half_w;
            desc.h = half_h;
            desc.format = Ren::eFormat::RGBA16F;
            desc.sampling.wrap = Ren::eWrap::ClampToEdge;

            presorted_data = data->out_presort =
                presort.AddStorageImageOutput("DOF Presort", desc, Ren::eStage::ComputeShader);
        }
        { // Halfres blurred color (separated foreground/background averages)
            FgImgDesc desc;
            desc.w = half_w;
            desc.h = half_h;
            desc.format = Ren::eFormat::RGBA16F;
            desc.sampling.wrap = Ren::eWrap::ClampToEdge;

            downsampled_scene = data->out_downsampled =
                presort.AddStorageImageOutput("DOF Downsampled", desc, Ren::eStage::ComputeShader);
        }

        data->params = base_params;
        data->params.img_size = Ren::Vec4f{float(half_w), float(half_h), full_w, full_h};
        const float presort_angle = golden_angle((fi + 0.1f) * 0.6180339887498948482f);
        data->params.rot = make_rotation(presort_angle);

        presort.set_execute_cb([this, data](const FgContext &fg) {
            const Ren::ImageROHandle in_color = fg.AccessROImage(data->in_color);
            const Ren::ImageROHandle in_depth = fg.AccessROImage(data->in_depth);
            const Ren::ImageROHandle in_tiles = fg.AccessROImage(data->in_tiles);
            const Ren::BufferROHandle in_random_seq = fg.AccessROBuffer(data->in_random_seq);
            const Ren::ImageRWHandle out_presort = fg.AccessRWImage(data->out_presort);
            const Ren::ImageRWHandle out_downsampled = fg.AccessRWImage(data->out_downsampled);

            const Ren::Binding bindings[] = {{Ren::eBindTarget::TexSampled, COLOR_TEX_SLOT, in_color},
                                             {Ren::eBindTarget::TexSampled, DEPTH_TEX_SLOT, {in_depth, 1}},
                                             {Ren::eBindTarget::TexSampled, COC_TILES_TEX_SLOT, in_tiles},
                                             {Ren::eBindTarget::UTBuf, RANDOM_SEQ_BUF_SLOT, in_random_seq},
                                             {Ren::eBindTarget::ImageRW, OUT_IMG_SLOT, out_presort},
                                             {Ren::eBindTarget::ImageRW, OUT2_IMG_SLOT, out_downsampled}};

            const auto grp_count = Ren::Vec3u{Ren::DivCeil<uint32_t>(uint32_t(data->params.img_size[0]), GRP_SIZE_X),
                                              Ren::DivCeil<uint32_t>(uint32_t(data->params.img_size[1]), GRP_SIZE_Y), 1u};

            DispatchCompute(fg.cmd_buf(), pi_dof_presort_, fg.storages(), grp_count, bindings, &data->params,
                            sizeof(Params), fg.descr_alloc(), fg.log());
        });
    }

    FgImgRWHandle filtered_scene, filter_alpha;
    { // Bokeh filter at halfres (49 ring samples from presort data)
        auto &filter = fg_builder_.AddNode("DOF FILTER");

        struct PassData {
            FgImgROHandle in_downsampled;
            FgImgROHandle in_tiles;
            FgImgROHandle in_presort;
            FgBufROHandle in_random_seq;
            FgImgRWHandle out_filtered;
            FgImgRWHandle out_alpha;
            Params params;
        };

        auto *data = fg_builder_.AllocTempData<PassData>();
        data->in_downsampled = filter.AddTextureInput(downsampled_scene, Ren::eStage::ComputeShader);
        data->in_tiles = filter.AddTextureInput(coc_tiles, Ren::eStage::ComputeShader);
        data->in_presort = filter.AddTextureInput(presorted_data, Ren::eStage::ComputeShader);
        data->in_random_seq = filter.AddStorageReadonlyInput(common_buffers.pmj_samples, Ren::eStage::ComputeShader);

        const uint32_t half_w = (view_state_.out_res[0] + 1) / 2;
        const uint32_t half_h = (view_state_.out_res[1] + 1) / 2;

        { // Halfres filtered color
            FgImgDesc desc;
            desc.w = half_w;
            desc.h = half_h;
            desc.format = Ren::eFormat::RGBA16F;
            desc.sampling.filter = Ren::eFilter::Bilinear;
            desc.sampling.wrap = Ren::eWrap::ClampToEdge;

            filtered_scene = data->out_filtered =
                filter.AddStorageImageOutput("DOF Filtered", desc, Ren::eStage::ComputeShader);
        }
        { // Halfres blend alpha (foreground/background mix of the filtered result)
            FgImgDesc desc;
            desc.w = half_w;
            desc.h = half_h;
            desc.format = Ren::eFormat::R16F;
            desc.sampling.filter = Ren::eFilter::Bilinear;
            desc.sampling.wrap = Ren::eWrap::ClampToEdge;

            filter_alpha = data->out_alpha =
                filter.AddStorageImageOutput("DOF Alpha", desc, Ren::eStage::ComputeShader);
        }

        data->params = base_params;
        data->params.img_size = Ren::Vec4f{float(half_w), float(half_h), full_w, full_h};
        const float filter_angle = golden_angle((fi + 0.37f) * 0.6180339887498948482f);
        data->params.rot = make_rotation(filter_angle);

        filter.set_execute_cb([this, data](const FgContext &fg) {
            const Ren::ImageROHandle in_downsampled = fg.AccessROImage(data->in_downsampled);
            const Ren::ImageROHandle in_tiles = fg.AccessROImage(data->in_tiles);
            const Ren::ImageROHandle in_presort = fg.AccessROImage(data->in_presort);
            const Ren::BufferROHandle in_random_seq = fg.AccessROBuffer(data->in_random_seq);
            const Ren::ImageRWHandle out_filtered = fg.AccessRWImage(data->out_filtered);
            const Ren::ImageRWHandle out_alpha = fg.AccessRWImage(data->out_alpha);

            const Ren::Binding bindings[] = {{Ren::eBindTarget::TexSampled, DOWNSAMPLED_TEX_SLOT, in_downsampled},
                                             {Ren::eBindTarget::TexSampled, COC_TILES_TEX_SLOT, in_tiles},
                                             {Ren::eBindTarget::TexSampled, PRESORT_TEX_SLOT, in_presort},
                                             {Ren::eBindTarget::UTBuf, RANDOM_SEQ_BUF_SLOT, in_random_seq},
                                             {Ren::eBindTarget::ImageRW, OUT_IMG_SLOT, out_filtered},
                                             {Ren::eBindTarget::ImageRW, OUT2_IMG_SLOT, out_alpha}};

            const auto grp_count = Ren::Vec3u{Ren::DivCeil<uint32_t>(uint32_t(data->params.img_size[0]), GRP_SIZE_X),
                                              Ren::DivCeil<uint32_t>(uint32_t(data->params.img_size[1]), GRP_SIZE_Y), 1u};

            DispatchCompute(fg.cmd_buf(), pi_dof_filter_, fg.storages(), grp_count, bindings, &data->params,
                            sizeof(Params), fg.descr_alloc(), fg.log());
        });
    }

    { // Median filter to remove bokeh noise from the halfres result
        auto &median = fg_builder_.AddNode("DOF MEDIAN");

        struct PassData {
            FgImgROHandle in_source;
            FgImgROHandle in_tiles;
            FgImgRWHandle out_filtered;
            Params params;
        };

        auto *data = fg_builder_.AllocTempData<PassData>();
        data->in_source = median.AddTextureInput(filtered_scene, Ren::eStage::ComputeShader);
        data->in_tiles = median.AddTextureInput(coc_tiles, Ren::eStage::ComputeShader);

        const uint32_t half_w = (view_state_.out_res[0] + 1) / 2;
        const uint32_t half_h = (view_state_.out_res[1] + 1) / 2;

        FgImgDesc desc;
        desc.w = half_w;
        desc.h = half_h;
        desc.format = Ren::eFormat::RGBA16F;
        desc.sampling.filter = Ren::eFilter::Bilinear;
        desc.sampling.wrap = Ren::eWrap::ClampToEdge;

        data->out_filtered = median.AddStorageImageOutput("DOF Median", desc, Ren::eStage::ComputeShader);
        filtered_scene = data->out_filtered;

        data->params = base_params;
        data->params.img_size = Ren::Vec4f{float(half_w), float(half_h), full_w, full_h};

        median.set_execute_cb([this, data](const FgContext &fg) {
            const Ren::ImageROHandle in_source = fg.AccessROImage(data->in_source);
            const Ren::ImageROHandle in_tiles = fg.AccessROImage(data->in_tiles);
            const Ren::ImageRWHandle out_filtered = fg.AccessRWImage(data->out_filtered);

            const Ren::Binding bindings[] = {{Ren::eBindTarget::TexSampled, DOWNSAMPLED_TEX_SLOT, in_source},
                                             {Ren::eBindTarget::TexSampled, COC_TILES_TEX_SLOT, in_tiles},
                                             {Ren::eBindTarget::ImageRW, OUT_IMG_SLOT, out_filtered}};

            const auto grp_count = Ren::Vec3u{Ren::DivCeil<uint32_t>(uint32_t(data->params.img_size[0]), GRP_SIZE_X),
                                              Ren::DivCeil<uint32_t>(uint32_t(data->params.img_size[1]), GRP_SIZE_Y), 1u};

            DispatchCompute(fg.cmd_buf(), pi_dof_median_, fg.storages(), grp_count, bindings, &data->params,
                            sizeof(Params), fg.descr_alloc(), fg.log());
        });
    }

    FgImgRWHandle output;
    { // Guided upsample back to fullres with sharp foreground from the original color
        auto &upsample = fg_builder_.AddNode("DOF UPSAMPLE");

        struct PassData {
            FgImgROHandle in_color;
            FgImgROHandle in_depth;
            FgImgROHandle in_tiles;
            FgImgROHandle in_filtered;
            FgImgROHandle in_alpha;
            FgBufROHandle in_random_seq;
            FgImgRWHandle output;
            Params params;
        };

        auto *data = fg_builder_.AllocTempData<PassData>();
        data->in_color = upsample.AddTextureInput(input, Ren::eStage::ComputeShader);
        data->in_depth = upsample.AddTextureInput(frame_textures.depth, Ren::eStage::ComputeShader);
        data->in_tiles = upsample.AddTextureInput(coc_tiles, Ren::eStage::ComputeShader);
        data->in_filtered = upsample.AddTextureInput(filtered_scene, Ren::eStage::ComputeShader);
        data->in_alpha = upsample.AddTextureInput(filter_alpha, Ren::eStage::ComputeShader);
        data->in_random_seq = upsample.AddStorageReadonlyInput(common_buffers.pmj_samples, Ren::eStage::ComputeShader);

        { // Fullres DOF result
            FgImgDesc desc;
            desc.w = view_state_.out_res[0];
            desc.h = view_state_.out_res[1];
            desc.format = Ren::eFormat::RGBA16F;
            desc.sampling.wrap = Ren::eWrap::ClampToEdge;

            output = data->output = upsample.AddStorageImageOutput("DOF Output", desc, Ren::eStage::ComputeShader);
        }

        data->params = base_params;
        data->params.img_size = Ren::Vec4f{full_w, full_h, full_w, full_h};

        upsample.set_execute_cb([this, data](const FgContext &fg) {
            const Ren::ImageROHandle in_color = fg.AccessROImage(data->in_color);
            const Ren::ImageROHandle in_depth = fg.AccessROImage(data->in_depth);
            const Ren::ImageROHandle in_tiles = fg.AccessROImage(data->in_tiles);
            const Ren::ImageROHandle in_filtered = fg.AccessROImage(data->in_filtered);
            const Ren::ImageROHandle in_alpha = fg.AccessROImage(data->in_alpha);
            const Ren::BufferROHandle in_random_seq = fg.AccessROBuffer(data->in_random_seq);
            const Ren::ImageRWHandle out = fg.AccessRWImage(data->output);

            const Ren::Binding bindings[] = {{Ren::eBindTarget::TexSampled, COLOR_TEX_SLOT, in_color},
                                             {Ren::eBindTarget::TexSampled, DEPTH_TEX_SLOT, {in_depth, 1}},
                                             {Ren::eBindTarget::TexSampled, COC_TILES_TEX_SLOT, in_tiles},
                                             {Ren::eBindTarget::TexSampled, FILTERED_TEX_SLOT, in_filtered},
                                             {Ren::eBindTarget::TexSampled, ALPHA_TEX_SLOT, in_alpha},
                                             {Ren::eBindTarget::UTBuf, RANDOM_SEQ_BUF_SLOT, in_random_seq},
                                             {Ren::eBindTarget::ImageRW, OUT_IMG_SLOT, out}};

            const auto grp_count = Ren::Vec3u{Ren::DivCeil<uint32_t>(uint32_t(data->params.img_size[0]), GRP_SIZE_X),
                                              Ren::DivCeil<uint32_t>(uint32_t(data->params.img_size[1]), GRP_SIZE_Y), 1u};

            DispatchCompute(fg.cmd_buf(), pi_dof_upsample_, fg.storages(), grp_count, bindings, &data->params,
                            sizeof(Params), fg.descr_alloc(), fg.log());
        });
    }

    return output;
}
