#pragma once

#include <atomic>
#include <optional>

#include <Ren/Common.h>
#include <Ren/TextureSplitter.h>
extern "C" {
#include <Ren/SW/SWculling.h>
}

#include "../scene/SceneData.h"
#include "PrimDraw.h"
#include "executors/ExBuildAccStructures.h"
#include "executors/ExEmissive.h"
#include "executors/ExGBufferFill.h"
#include "executors/ExOpaque.h"
#include "executors/ExPostprocess.h"
#include "executors/ExRTGI.h"
#include "executors/ExRTGICache.h"
#include "executors/ExRTReflections.h"
#include "executors/ExRTShadows.h"
#include "executors/ExReadExposure.h"
#include "executors/ExSampleLights.h"
#include "executors/ExSkinning.h"
#include "executors/ExSkydome.h"
#include "executors/ExTransparent.h"
#include "executors/ExUpdateAccBuffers.h"

#include "Renderer_DrawList.h"

namespace Sys {
template <typename T> class MonoAlloc;
class ThreadPool;
} // namespace Sys

namespace Eng {
class Random;
class ShaderLoader;

static_assert(RAY_TYPE_CAMERA == int(AccStructure::eRayType::Camera));
static_assert(RAY_TYPE_DIFFUSE == int(AccStructure::eRayType::Diffuse));
static_assert(RAY_TYPE_SPECULAR == int(AccStructure::eRayType::Specular));
static_assert(RAY_TYPE_REFRACTION == int(AccStructure::eRayType::Refraction));
static_assert(RAY_TYPE_SHADOW == int(AccStructure::eRayType::Shadow));

class Renderer {
  public:
    Renderer(Ren::Context &ctx, ShaderLoader &sh, Random &rand, Sys::ThreadPool &threads);
    ~Renderer();

    void InitPipelines();

    PrimDraw &prim_draw() { return prim_draw_; }

    int accumulated_frames() const { return accumulated_frames_; }
    void reset_accumulation() { frame_index_ = view_state_.frame_index = accumulated_frames_ = 0; }

    float readback_exposure() const {
        if (ex_read_exposure_.exposure() > 0.0f) {
            return std::min(std::max(ex_read_exposure_.exposure(), min_exposure_), max_exposure_);
        }
        return std::min(std::max(1.0f, min_exposure_), max_exposure_);
    }
    void set_pre_exposure(const float exposure) { pre_exposure_ = exposure; }

    const backend_info_t &backend_info() const { return backend_info_; }

    void InitBackendInfo();

    void InitPipelinesForProgram(const Ren::ProgramRef &prog, Ren::Bitmask<Ren::eMatFlags> mat_flags,
                                 Ren::PipelineStorage &storage,
                                 Ren::SmallVectorImpl<Ren::PipelineRef> &out_pipelines) const;

    void PrepareDrawList(const SceneData &scene, const Ren::Camera &cam, const Ren::Camera &ext_cam, DrawList &list);
    void ExecuteDrawList(const DrawList &list, const PersistentGpuData &persistent_data, const Ren::TexRef target = {},
                         bool blit_to_backbuffer = false);

    void SetTonemapLUT(int res, Ren::eTexFormat format, Ren::Span<const uint8_t> data);

    void BlitPixelsTonemap(const uint8_t *data, int w, int h, int stride, Ren::eTexFormat format, float gamma,
                           float min_exposure, float max_exposure, const Ren::TexRef &target, bool compressed,
                           bool blit_to_backbuffer = false);
    void BlitImageTonemap(const Ren::TexRef &result, int w, int h, Ren::eTexFormat format, float gamma,
                          float min_exposure, float max_exposure, const Ren::TexRef &target, bool compressed,
                          bool blit_to_backbuffer = false);
    render_settings_t settings = {};

  private:
    Ren::Context &ctx_;
    ShaderLoader &sh_;
    Random &rand_;
    Sys::ThreadPool &threads_;
    SWcull_ctx cull_ctx_ = {};

    Ren::TexRef dummy_black_, dummy_white_, rand2d_8x8_, rand2d_dirs_4x4_, brdf_lut_, ltc_luts_, cone_rt_lut_,
        noise_tex_;
    Ren::TexRef tonemap_lut_;
    Ren::BufRef sobol_seq_buf_, scrambling_tile_buf_, ranking_tile_buf_;
    Ren::BufRef pmj_samples_buf_;
    Ren::TexRef sky_transmittance_lut_, sky_multiscatter_lut_, sky_moon_tex_, sky_weather_tex_, sky_cirrus_tex_,
        sky_curl_tex_;
    Ren::TexRef sky_noise3d_tex_;

    // FrameBuf probe_sample_buf_;
    Ren::TexRef shadow_depth_tex_, shadow_color_tex_;
    Ren::SamplerRef nearest_sampler_, linear_sampler_;
    Ren::Framebuffer blur_tex_fb_[2], down_tex_4x_fb_;
    eTAAMode taa_mode_ = eTAAMode::Off;
    bool dof_enabled_ = false;

    Ren::VertexInputRef draw_pass_vi_;
    Ren::RenderPassRef rp_main_draw_;
    Ren::RastState rast_states_[int(eFwdPipeline::_Count)];

    Ren::TextureSplitter shadow_splitter_;

    std::vector<uint32_t> litem_to_lsource_;
    DynArray<const Decal *> ditem_to_decal_;

    static const int PxFilterTableSize = 1024;
    ePixelFilter px_filter_table_filter_ = ePixelFilter(-1);
    float px_filter_table_width_ = 0.0f;
    std::vector<float> px_filter_table_;

    struct ProcessedObjData {
        uint32_t base_vertex;
        int32_t rt_sh_index;
        int32_t li_index;
        std::atomic_uint8_t visited_mask;
    };
    std::unique_ptr<ProcessedObjData[]> proc_objects_;
    int proc_objects_capacity_ = 0;
    struct VisObj {
        uint32_t index;
        float dist2;
    };
    struct VisObjStorage {
        std::vector<VisObj> objects;
        std::atomic_int count = 0;

        VisObjStorage() = default;
        VisObjStorage(VisObjStorage &&rhs) noexcept : objects(std::move(rhs.objects)), count(rhs.count.load()) {}
    };
    Ren::HashMap32<uint32_t, VisObjStorage> temp_visible_objects_, temp_rt_visible_objects_;
    DynArray<BBox> decals_boxes_;
    backend_info_t backend_info_;
    uint64_t backend_cpu_start_ = 0, backend_cpu_end_ = 0;
    int backend_gpu_start_ = -1, backend_gpu_end_ = -1;
    Ren::Vec4f prev_wind_scroll_;

    DynArray<Ren::Frustum> temp_sub_frustums_;
    std::vector<sort_span_32_t> temp_sort_spans_32_[2];
    std::vector<sort_span_64_t> temp_sort_spans_64_[2];

    std::vector<float> temp_depth;

    struct ShadowFrustumCache {
        bool valid = false;
        Ren::Vec3f view_pos, view_dir;
        Ren::Mat4f clip_from_world;
    };

    ShadowFrustumCache sun_shadow_cache_[4];

    Ren::SubAllocation skinned_buf1_vtx_, skinned_buf2_vtx_;

    DynArray<ShadReg> allocated_shadow_regions_;

#if defined(__ANDROID__)
    static constexpr int SHADOWMAP_WIDTH = SHADOWMAP_RES_ANDROID;
#else
    static constexpr int SHADOWMAP_WIDTH = SHADOWMAP_RES_PC;
#endif
    static constexpr int SHADOWMAP_HEIGHT = SHADOWMAP_WIDTH / 2;
    // Sun shadow occupies half of atlas
    static constexpr int SUN_SHADOW_RES = SHADOWMAP_WIDTH / 2;

    FgBuilder fg_builder_;
    std::optional<render_settings_t> cached_settings_;
    int cached_rp_index_ = 0;
    Ren::WeakTexRef env_map_;
    Ren::WeakTexRef lm_direct_, lm_indir_, lm_indir_sh_[4];
    const DrawList *p_list_;
    Ren::SmallVector<FgResRef, 8> backbuffer_sources_;
    float min_exposure_ = 1.0f, max_exposure_ = 1.0f;
    float pre_exposure_ = 1.0f;

    ExSkydomeCube ex_skydome_cube_ = ExSkydomeCube{prim_draw_};
    ExSkydomeScreen ex_skydome_ = ExSkydomeScreen{prim_draw_};
    ExGBufferFill ex_gbuffer_fill_;
    ExOpaque ex_opaque_;
    ExTransparent ex_transparent_ = ExTransparent{prim_draw_};
    ExEmissive ex_emissive_;

    ExRTReflections ex_oit_rt_reflections_ = ExRTReflections{true};
    ExRTGI ex_rt_gi_;
    ExRTGICache ex_rt_gi_cache_;
    ExRTReflections ex_rt_reflections_ = ExRTReflections{false};
    ExRTShadows ex_rt_shadows_;
    ExSampleLights ex_sample_lights_;
    ExReadExposure ex_read_exposure_;
    ExPostprocess::Args ex_postprocess_args_;

    view_state_t view_state_;
    PrimDraw prim_draw_;
    uint32_t frame_index_ = 0, accumulated_frames_ = 0;

    Ren::PipelineRef pi_gbuf_shade_[2];
    // HQ SSR
    Ren::PipelineRef pi_ssr_classify_, pi_ssr_write_indirect_, pi_ssr_trace_hq_[2][2];
    Ren::PipelineRef pi_rt_write_indirect_;
    // SSR Denoiser stuff
    Ren::PipelineRef pi_ssr_reproject_, pi_ssr_temporal_[2], pi_ssr_filter_[4], pi_ssr_stabilization_;
    Ren::PipelineRef pi_tile_clear_[4];
    // GI Cache
    Ren::PipelineRef pi_probe_blend_[3][2], pi_probe_relocate_[3], pi_probe_classify_[3], pi_probe_sample_;
    // GTAO
    Ren::PipelineRef pi_gtao_main_[2], pi_gtao_filter_[2], pi_gtao_accumulate_, pi_gtao_upsample_;
    // GI
    Ren::PipelineRef pi_gi_classify_, pi_gi_write_indirect_, pi_gi_trace_ss_;
    Ren::PipelineRef pi_gi_rt_write_indirect_;
    // GI Denoiser stuff
    Ren::PipelineRef pi_gi_reproject_, pi_gi_temporal_[2], pi_gi_filter_[4], pi_gi_stabilization_;
    // Sun shadows
    Ren::PipelineRef pi_shadow_classify_, pi_sun_shadows_[2], pi_shadow_prepare_mask_, pi_shadow_classify_tiles_,
        pi_shadow_filter_[3], pi_shadow_debug_;
    Ren::PipelineRef pi_sun_brightness_;
    // Bloom
    Ren::PipelineRef pi_bloom_downsample_[2], pi_bloom_upsample_;
    // Autoexposure
    Ren::PipelineRef pi_histogram_sample_, pi_histogram_exposure_;
    // Volumetrics
    Ren::PipelineRef pi_sky_upsample_;
    Ren::PipelineRef pi_fog_inject_light_, pi_fog_ray_march_, pi_fog_apply_;
    // Debug
    Ren::PipelineRef pi_debug_velocity_;

    Ren::ProgramRef blit_static_vel_prog_, blit_gauss_prog_, blit_ao_prog_, blit_bilateral_prog_, blit_taa_prog_[2],
        blit_taa_static_prog_, blit_ssr_prog_, blit_ssr_dilate_prog_, blit_upscale_prog_, blit_down_prog_,
        blit_down_depth_prog_, blit_ssr_compose_prog_, blit_fxaa_prog_;

    struct CommonBuffers {
        FgResRef skin_transforms, shape_keys, instance_indices, cells, rt_cells, lights, decals, items, rt_items,
            shared_data, atomic_cnt;
    };

    struct FrameTextures {
        Ren::TexParams color_params;
        FgResRef color;
        Ren::TexParams albedo_params;
        FgResRef albedo;
        Ren::TexParams specular_params;
        FgResRef specular;
        Ren::TexParams normal_params;
        FgResRef normal;
        Ren::TexParams depth_params;
        FgResRef depth, opaque_depth;
        Ren::TexParams velocity_params;
        FgResRef velocity;

        FgResRef envmap;
        FgResRef shadow_depth, shadow_color;
        FgResRef ssao;
        FgResRef gi;
        FgResRef sun_shadow;
        FgResRef exposure;

        FgResRef gi_cache_irradiance;
        FgResRef gi_cache_distance;
        FgResRef gi_cache_offset;

        FgResRef oit_depth_buf;
    };

    void AddBuffersUpdatePass(CommonBuffers &common_buffers, const PersistentGpuData &persistent_data);
    void AddLightBuffersUpdatePass(CommonBuffers &common_buffers);
    void AddGBufferFillPass(const CommonBuffers &common_buffers, const PersistentGpuData &persistent_data,
                            const BindlessTextureData &bindless, FrameTextures &frame_textures);
    void AddDeferredShadingPass(const CommonBuffers &common_buffers, FrameTextures &frame_textures, bool enable_gi);
    void AddEmissivesPass(const CommonBuffers &common_buffers, const PersistentGpuData &persistent_data,
                          const BindlessTextureData &bindless, FrameTextures &frame_textures);
    void AddForwardOpaquePass(const CommonBuffers &common_buffers, const PersistentGpuData &persistent_data,
                              const BindlessTextureData &bindless, FrameTextures &frame_textures);

    void AddFillStaticVelocityPass(const CommonBuffers &common_buffers, FgResRef depth_tex,
                                   FgResRef &inout_velocity_tex);
    void AddTaaPass(const CommonBuffers &common_buffers, FrameTextures &frame_textures, bool static_accumulation,
                    FgResRef &resolved_color);
    void AddDownsampleDepthPass(const CommonBuffers &common_buffers, FgResRef depth_tex, FgResRef &out_depth_down_2x);

    // GI Cache
    void AddGICachePasses(const Ren::WeakTexRef &env_map, const CommonBuffers &common_buffers,
                          const PersistentGpuData &persistent_data, const AccelerationStructureData &acc_struct_data,
                          const BindlessTextureData &bindless, FgResRef rt_geo_instances_res,
                          FgResRef rt_obj_instances_res, FrameTextures &frame_textures);

    // GI Diffuse
    void AddDiffusePasses(const Ren::WeakTexRef &env_map, const Ren::WeakTexRef &lm_direct,
                          const Ren::WeakTexRef lm_indir_sh[4], bool debug_denoise, const CommonBuffers &common_buffers,
                          const PersistentGpuData &persistent_data, const AccelerationStructureData &acc_struct_data,
                          const BindlessTextureData &bindless, FgResRef depth_hierarchy, FgResRef rt_geo_instances_res,
                          FgResRef rt_obj_instances_res, FrameTextures &frame_textures);
    void AddSSAOPasses(FgResRef depth_down_2x, FgResRef depth_tex, FgResRef &out_ssao);
    FgResRef AddGTAOPasses(eSSAOQuality quality, FgResRef depth_tex, FgResRef velocity_tex, FgResRef norm_tex);

    // GI Specular
    void AddHQSpecularPasses(bool deferred_shading, bool debug_denoise, const CommonBuffers &common_buffers,
                             const PersistentGpuData &persistent_data, const AccelerationStructureData &acc_struct_data,
                             const BindlessTextureData &bindless, FgResRef depth_hierarchy,
                             FgResRef rt_geo_instances_res, FgResRef rt_obj_instances_res,
                             FrameTextures &frame_textures);
    void AddLQSpecularPasses(const CommonBuffers &common_buffers, FgResRef depth_down_2x,
                             FrameTextures &frame_textures);

    // Sun Shadows
    FgResRef AddHQSunShadowsPasses(const CommonBuffers &common_buffers, const PersistentGpuData &persistent_data,
                                   const AccelerationStructureData &acc_struct_data,
                                   const BindlessTextureData &bindless, FgResRef rt_geo_instances_res,
                                   FgResRef rt_obj_instances_res, const FrameTextures &frame_textures,
                                   bool debug_denoise);
    FgResRef AddLQSunShadowsPass(const CommonBuffers &common_buffers, const PersistentGpuData &persistent_data,
                                 const AccelerationStructureData &acc_struct_data, const BindlessTextureData &bindless,
                                 const FrameTextures &frame_textures);

    // Transparency
    void AddOITPasses(const CommonBuffers &common_buffers, const PersistentGpuData &persistent_data,
                      const AccelerationStructureData &acc_struct_data, const BindlessTextureData &bindless,
                      FgResRef depth_hierarchy, FgResRef rt_geo_instances_res, FgResRef rt_obj_instances_res,
                      FrameTextures &frame_textures);
    void AddForwardTransparentPass(const CommonBuffers &common_buffers, const PersistentGpuData &persistent_data,
                                   const BindlessTextureData &bindless, FrameTextures &frame_textures);

    // Volumetrics
    void InitSkyResources();
    void AddSkydomePass(const CommonBuffers &common_buffers, FrameTextures &frame_textures);
    void AddSunColorUpdatePass(CommonBuffers &common_buffers);
    void AddFogPasses(const CommonBuffers &common_buffers, FrameTextures &frame_textures);

    // Debugging
    void AddDebugVelocityPass(FgResRef velocity, FgResRef &output_tex);

    void GatherDrawables(const SceneData &scene, const Ren::Camera &cam, const Ren::Camera &ext_cam, DrawList &list);

    void UpdatePixelFilterTable(ePixelFilter filter, float filter_width);

    // Postprocess
    FgResRef AddAutoexposurePasses(FgResRef hdr_texture);
    FgResRef AddBloomPasses(FgResRef hdr_texture, FgResRef exposure_texture, bool compressed);

    // Parallel Jobs
    static void GatherObjectsForZSlice_Job(const Ren::Frustum &frustum, const SceneData &scene,
                                           const Ren::Vec3f &cam_pos, const Ren::Mat4f &clip_from_identity,
                                           uint64_t comp_mask, SWcull_ctx *cull_ctx, uint8_t visit_mask,
                                           ProcessedObjData proc_objects[],
                                           Ren::HashMap32<uint32_t, VisObjStorage> &out_visible_objects2);
    static void ClusterItemsForZSlice_Job(int slice, const Ren::Frustum *sub_frustums, const BBox *decals_boxes,
                                          const LightSource *light_sources, Ren::Span<const uint32_t> litem_to_lsource,
                                          const DrawList &list, cell_data_t out_cells[], item_data_t out_items[],
                                          std::atomic_int &items_count);

    // Generate auxiliary textures
    static std::unique_ptr<uint16_t[]> Generate_BRDF_LUT(int res, std::string &out_c_header);
    static std::unique_ptr<int8_t[]> Generate_PeriodicPerlin(int res, std::string &out_c_header);
    static std::unique_ptr<uint8_t[]> Generate_SSSProfile_LUT(int res, int gauss_count, const float gauss_variances[],
                                                              const Ren::Vec3f diffusion_weights[]);
    static std::unique_ptr<int16_t[]> Generate_RandDirs(int res, std::string &out_c_header);
    static std::unique_ptr<uint8_t[]> Generate_ConeTraceLUT(int resx, int resy, const float cone_angles[4],
                                                            std::string &out_c_header);
    static std::vector<Ren::Vec4f> Generate_SkyTransmittanceLUT(const atmosphere_params_t &params);
    static std::vector<Ren::Vec4f> Generate_SkyMultiscatterLUT(const atmosphere_params_t &params,
                                                               Ren::Span<const Ren::Vec4f> transmittance_lut);
};
} // namespace Eng