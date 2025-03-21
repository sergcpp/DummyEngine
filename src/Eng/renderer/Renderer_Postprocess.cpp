#include "Renderer.h"

#include <Ren/Context.h>

#include "Renderer_Names.h"

#include "shaders/bloom_interface.h"
#include "shaders/histogram_exposure_interface.h"
#include "shaders/histogram_sample_interface.h"

Eng::FgResRef Eng::Renderer::AddAutoexposurePasses(FgResRef hdr_texture) {
    FgResRef histogram;
    { // Clear histogram image
        auto &histogram_clear = fg_builder_.AddNode("HISTOGRAM CLEAR");

        Ren::TexParams params;
        params.w = EXPOSURE_HISTOGRAM_RES + 1;
        params.h = 1;
        params.format = Ren::eTexFormat::R32UI;
        params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

        histogram = histogram_clear.AddTransferImageOutput("Exposure Histogram", params);

        histogram_clear.set_execute_cb([histogram](FgBuilder &builder) {
            FgAllocTex &histogram_tex = builder.GetWriteTexture(histogram);

            const float zero[4] = {};
            Ren::ClearImage(*histogram_tex.ref, zero, builder.ctx().current_cmd_buf());
        });
    }
    { // Sample histogram
        auto &histogram_sample = fg_builder_.AddNode("HISTOGRAM SAMPLE");

        FgResRef input = histogram_sample.AddTextureInput(hdr_texture, Ren::eStageBits::ComputeShader);
        histogram = histogram_sample.AddStorageImageOutput(histogram, Ren::eStageBits::ComputeShader);

        histogram_sample.set_execute_cb([this, input, histogram](FgBuilder &builder) {
            FgAllocTex &input_tex = builder.GetReadTexture(input);
            FgAllocTex &output_tex = builder.GetWriteTexture(histogram);

            const Ren::Binding bindings[] = {
                {Ren::eBindTarget::Tex2DSampled, HistogramSample::HDR_TEX_SLOT, {*input_tex.ref, *linear_sampler_}},
                {Ren::eBindTarget::Image2D, HistogramSample::OUT_IMG_SLOT, *output_tex.ref}};

            HistogramSample::Params uniform_params = {};
            uniform_params.pre_exposure = view_state_.pre_exposure;

            DispatchCompute(*pi_histogram_sample_, Ren::Vec3u{16, 8, 1}, bindings, &uniform_params,
                            sizeof(uniform_params), builder.ctx().default_descr_alloc(), builder.log());
        });
    }
    FgResRef exposure;
    { // Calc exposure
        auto &histogram_exposure = fg_builder_.AddNode("HISTOGRAM EXPOSURE");

        struct PassData {
            FgResRef histogram;
            FgResRef exposure_prev;
            FgResRef exposure;
        };

        auto *data = histogram_exposure.AllocNodeData<PassData>();

        data->histogram = histogram_exposure.AddTextureInput(histogram, Ren::eStageBits::ComputeShader);

        Ren::TexParams params;
        params.w = params.h = 1;
        params.format = Ren::eTexFormat::R32F;
        params.sampling.wrap = Ren::eTexWrap::ClampToEdge;
        exposure = data->exposure =
            histogram_exposure.AddStorageImageOutput(EXPOSURE_TEX, params, Ren::eStageBits::ComputeShader);
        data->exposure_prev = histogram_exposure.AddHistoryTextureInput(exposure, Ren::eStageBits::ComputeShader);

        histogram_exposure.set_execute_cb([this, data](FgBuilder &builder) {
            FgAllocTex &histogram_tex = builder.GetReadTexture(data->histogram);
            FgAllocTex &exposure_prev_tex = builder.GetReadTexture(data->exposure_prev);
            FgAllocTex &exposure_tex = builder.GetWriteTexture(data->exposure);

            const Ren::Binding bindings[] = {
                {Ren::eBindTarget::Tex2DSampled, HistogramExposure::HISTOGRAM_TEX_SLOT, *histogram_tex.ref},
                {Ren::eBindTarget::Tex2DSampled, HistogramExposure::EXPOSURE_PREV_TEX_SLOT, *exposure_prev_tex.ref},
                {Ren::eBindTarget::Image2D, HistogramExposure::OUT_TEX_SLOT, *exposure_tex.ref}};

            HistogramExposure::Params uniform_params = {};
            uniform_params.min_exposure = min_exposure_;
            uniform_params.max_exposure = max_exposure_;
            uniform_params.exposure_factor = (settings.tonemap_mode != Eng::eTonemapMode::Standard) ? 1.25f : 0.5f;

            DispatchCompute(*pi_histogram_exposure_, Ren::Vec3u{1}, bindings, &uniform_params, sizeof(uniform_params),
                            builder.ctx().default_descr_alloc(), builder.log());
        });
    }
    return exposure;
}

Eng::FgResRef Eng::Renderer::AddBloomPasses(FgResRef hdr_texture, FgResRef exposure_texture, const bool compressed) {
    static const int BloomMipCount = 5;

    Eng::FgResRef downsampled[BloomMipCount];
    for (int mip = 0; mip < BloomMipCount; ++mip) {
        const std::string node_name = "BLOOM DOWNS. " + std::to_string(mip) + "->" + std::to_string(mip + 1);
        auto &bloom_downsample = fg_builder_.AddNode(node_name);

        struct PassData {
            FgResRef input_tex;
            FgResRef exposure_tex;
            FgResRef output_tex;
        };

        auto *data = bloom_downsample.AllocNodeData<PassData>();
        if (mip == 0) {
            data->input_tex = bloom_downsample.AddTextureInput(hdr_texture, Ren::eStageBits::ComputeShader);
        } else {
            data->input_tex = bloom_downsample.AddTextureInput(downsampled[mip - 1], Ren::eStageBits::ComputeShader);
        }
        data->exposure_tex = bloom_downsample.AddTextureInput(exposure_texture, Ren::eStageBits::ComputeShader);

        { // Texture that holds downsampled bloom image
            Ren::TexParams params;
            params.w = (view_state_.scr_res[0] / 2) >> mip;
            params.h = (view_state_.scr_res[1] / 2) >> mip;
            params.format = compressed ? Ren::eTexFormat::RGBA16F : Ren::eTexFormat::RGBA32F;
            params.sampling.filter = Ren::eTexFilter::Bilinear;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

            const std::string output_name = "Bloom Downsampled " + std::to_string(mip);
            downsampled[mip] = data->output_tex =
                bloom_downsample.AddStorageImageOutput(output_name, params, Ren::eStageBits::ComputeShader);
        }

        bloom_downsample.set_execute_cb([this, data, mip](FgBuilder &builder) {
            FgAllocTex &input_tex = builder.GetReadTexture(data->input_tex);
            FgAllocTex &exposure_tex = builder.GetReadTexture(data->exposure_tex);
            FgAllocTex &output_tex = builder.GetWriteTexture(data->output_tex);

            Bloom::Params uniform_params;
            uniform_params.img_size[0] = output_tex.ref->params.w;
            uniform_params.img_size[1] = output_tex.ref->params.h;
            uniform_params.pre_exposure = view_state_.pre_exposure;

            const Ren::Binding bindings[] = {
                {Ren::eBindTarget::Tex2DSampled, Bloom::INPUT_TEX_SLOT, {*input_tex.ref, *linear_sampler_}},
                {Ren::eBindTarget::Tex2DSampled, Bloom::EXPOSURE_TEX_SLOT, *exposure_tex.ref},
                {Ren::eBindTarget::Image2D, Bloom::OUT_IMG_SLOT, *output_tex.ref}};

            const Ren::Vec3u grp_count = Ren::Vec3u{
                (uniform_params.img_size[0] + Bloom::LOCAL_GROUP_SIZE_X - 1u) / Bloom::LOCAL_GROUP_SIZE_X,
                (uniform_params.img_size[1] + Bloom::LOCAL_GROUP_SIZE_Y - 1u) / Bloom::LOCAL_GROUP_SIZE_Y, 1u};

            DispatchCompute(*pi_bloom_downsample_[mip == 0], grp_count, bindings, &uniform_params,
                            sizeof(uniform_params), builder.ctx().default_descr_alloc(), builder.log());
        });
    }
    Eng::FgResRef upsampled[BloomMipCount - 1];
    for (int mip = BloomMipCount - 2; mip >= 0; --mip) {
        const std::string node_name = "BLOOM UPS. " + std::to_string(mip + 2) + "->" + std::to_string(mip + 1);
        auto &bloom_upsample = fg_builder_.AddNode(node_name);

        struct PassData {
            FgResRef input_tex;
            FgResRef blend_tex;
            FgResRef output_tex;
        };

        auto *data = bloom_upsample.AllocNodeData<PassData>();
        if (mip == BloomMipCount - 2) {
            data->input_tex = bloom_upsample.AddTextureInput(downsampled[mip + 1], Ren::eStageBits::ComputeShader);
        } else {
            data->input_tex = bloom_upsample.AddTextureInput(upsampled[mip + 1], Ren::eStageBits::ComputeShader);
        }
        data->blend_tex = bloom_upsample.AddTextureInput(downsampled[mip], Ren::eStageBits::ComputeShader);

        { // Texture that holds upsampled bloom image
            Ren::TexParams params;
            params.w = (view_state_.scr_res[0] / 2) >> mip;
            params.h = (view_state_.scr_res[1] / 2) >> mip;
            params.format = compressed ? Ren::eTexFormat::RGBA16F : Ren::eTexFormat::RGBA32F;
            params.sampling.filter = Ren::eTexFilter::Bilinear;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

            const std::string output_name = "Bloom Upsampled " + std::to_string(mip);
            upsampled[mip] = data->output_tex =
                bloom_upsample.AddStorageImageOutput(output_name, params, Ren::eStageBits::ComputeShader);
        }

        bloom_upsample.set_execute_cb([this, data, mip](FgBuilder &builder) {
            FgAllocTex &input_tex = builder.GetReadTexture(data->input_tex);
            FgAllocTex &blend_tex = builder.GetReadTexture(data->blend_tex);
            FgAllocTex &output_tex = builder.GetWriteTexture(data->output_tex);

            Bloom::Params uniform_params;
            uniform_params.img_size[0] = output_tex.ref->params.w;
            uniform_params.img_size[1] = output_tex.ref->params.h;
            uniform_params.blend_weight = 1.0f / float(1 + BloomMipCount - 1 - mip);

            const Ren::Binding bindings[] = {{Ren::eBindTarget::Tex2DSampled, Bloom::INPUT_TEX_SLOT, *input_tex.ref},
                                             {Ren::eBindTarget::Tex2DSampled, Bloom::BLEND_TEX_SLOT, *blend_tex.ref},
                                             {Ren::eBindTarget::Image2D, Bloom::OUT_IMG_SLOT, *output_tex.ref}};

            const Ren::Vec3u grp_count = Ren::Vec3u{
                (uniform_params.img_size[0] + Bloom::LOCAL_GROUP_SIZE_X - 1u) / Bloom::LOCAL_GROUP_SIZE_X,
                (uniform_params.img_size[1] + Bloom::LOCAL_GROUP_SIZE_Y - 1u) / Bloom::LOCAL_GROUP_SIZE_Y, 1u};

            DispatchCompute(*pi_bloom_upsample_, grp_count, bindings, &uniform_params, sizeof(uniform_params),
                            builder.ctx().default_descr_alloc(), builder.log());
        });
    }

    return upsampled[0];
}