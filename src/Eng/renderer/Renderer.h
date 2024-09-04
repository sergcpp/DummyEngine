#pragma once

#include <atomic>

#include <Ren/Common.h>
#include <Ren/TextureSplitter.h>
extern "C" {
#include <Ren/SW/SWculling.h>
}

#include "../scene/SceneData.h"
#include "PrimDraw.h"
#include "executors/ExBuildAccStructures.h"
#include "executors/ExCombine.h"
#include "executors/ExDebugOIT.h"
#include "executors/ExDebugProbes.h"
#include "executors/ExDebugRT.h"
#include "executors/ExDepthFill.h"
#include "executors/ExDepthHierarchy.h"
#include "executors/ExEmissive.h"
#include "executors/ExGBufferFill.h"
#include "executors/ExOITBlendLayer.h"
#include "executors/ExOITDepthPeel.h"
#include "executors/ExOITScheduleRays.h"
#include "executors/ExOpaque.h"
#include "executors/ExRTGI.h"
#include "executors/ExRTGICache.h"
#include "executors/ExRTReflections.h"
#include "executors/ExRTShadows.h"
#include "executors/ExReadExposure.h"
#include "executors/ExSSRCompose.h"
#include "executors/ExSampleLights.h"
#include "executors/ExShadowMaps.h"
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

    int accumulated_frames() const { return accumulated_frames_; }
    void reset_accumulation() { frame_index_ = view_state_.frame_index = accumulated_frames_ = 0; }

    float readback_exposure() const {
        if (ex_read_exposure_.exposure() > 0.0f) {
            return ex_read_exposure_.exposure();
        }
        return std::min(std::max(1.0f, min_exposure_), max_exposure_);
    }
    void set_pre_exposure(const float exposure) { pre_exposure_ = exposure; }

    const BackendInfo &backend_info() const { return backend_info_; }

    void InitBackendInfo();

    void InitPipelinesForProgram(const Ren::ProgramRef &prog, Ren::Bitmask<Ren::eMatFlags> mat_flags,
                                 Ren::PipelineStorage &storage,
                                 Ren::SmallVectorImpl<Ren::PipelineRef> &out_pipelines) const;

    void PrepareDrawList(const SceneData &scene, const Ren::Camera &cam, const Ren::Camera &ext_cam, DrawList &list);
    void ExecuteDrawList(const DrawList &list, const PersistentGpuData &persistent_data,
                         const Ren::Tex2DRef target = {}, bool blit_to_backbuffer = false);

    void SetTonemapLUT(int res, Ren::eTexFormat format, Ren::Span<const uint8_t> data);

    void BlitPixelsTonemap(const uint8_t *data, int w, int h, int stride, Ren::eTexFormat format, float gamma,
                           float min_exposure, float max_exposure, const Ren::Tex2DRef &target, bool compressed,
                           bool blit_to_backbuffer = false);
    render_settings_t settings = {};

  private:
    Ren::Context &ctx_;
    ShaderLoader &sh_;
    Random &rand_;
    Sys::ThreadPool &threads_;
    SWcull_ctx cull_ctx_ = {};
    Ren::ProgramRef blit_prog_, blit_down_prog_, blit_gauss_prog_, blit_depth_prog_, blit_rgbm_prog_, blit_mipmap_prog_,
        blit_prefilter_prog_, blit_project_sh_prog_;

    Ren::Tex2DRef dummy_black_, dummy_white_, rand2d_8x8_, rand2d_dirs_4x4_, brdf_lut_, ltc_luts_, cone_rt_lut_,
        noise_tex_;
    Ren::Tex3DRef tonemap_lut_;
    Ren::BufferRef sobol_seq_buf_, scrambling_tile_buf_, ranking_tile_buf_;
    Ren::BufferRef pmj_samples_buf_;
    Ren::Tex2DRef sky_transmittance_lut_, sky_multiscatter_lut_, sky_moon_tex_, sky_weather_tex_, sky_cirrus_tex_,
        sky_curl_tex_;
    Ren::Tex3DRef sky_noise3d_tex_;

    // FrameBuf probe_sample_buf_;
    Ren::Tex2DRef shadow_map_tex_;
    Ren::SamplerRef nearest_sampler_;
    Ren::Tex2DRef down_tex_4x_;
    Ren::Framebuffer blur_tex_fb_[2], down_tex_4x_fb_;
    eTAAMode taa_mode_ = eTAAMode::Off;
    bool dof_enabled_ = false;

    Ren::VertexInput draw_pass_vi_;
    Ren::RenderPass rp_main_draw_;
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
    BackendInfo backend_info_;
    uint64_t backend_cpu_start_ = 0, backend_cpu_end_ = 0;
    int backend_gpu_start_ = -1, backend_gpu_end_ = -1;
    Ren::Vec4f prev_wind_scroll_;

    DynArray<Ren::Frustum> temp_sub_frustums_;
    std::vector<SortSpan32> temp_sort_spans_32_[2];
    std::vector<SortSpan64> temp_sort_spans_64_[2];

    std::vector<float> temp_depth;

    struct ShadowFrustumCache {
        bool valid = false;
        Ren::Vec3f view_pos, view_dir;
        Ren::Mat4f clip_from_world;
    };

    ShadowFrustumCache sun_shadow_cache_[4];

    Ren::SubAllocation temp_buf1_vtx_, temp_buf2_vtx_, temp_buf_ndx_, skinned_buf1_vtx_, skinned_buf2_vtx_;

    DynArray<ShadReg> allocated_shadow_regions_;

#if defined(__ANDROID__)
    static const int SHADOWMAP_WIDTH = SHADOWMAP_RES_ANDROID;
#else
    static const int SHADOWMAP_WIDTH = SHADOWMAP_RES_PC;
#endif
    static const int SHADOWMAP_HEIGHT = SHADOWMAP_WIDTH / 2;
    // Sun shadow occupies half of atlas
    static const int SUN_SHADOW_RES = SHADOWMAP_WIDTH / 2;

    FgBuilder fg_builder_;
    std::optional<render_settings_t> cached_settings_;
    int cached_rp_index_ = 0;
    Ren::WeakTex2DRef env_map_;
    Ren::WeakTex2DRef lm_direct_, lm_indir_, lm_indir_sh_[4];
    const DrawList *p_list_;
    Ren::SmallVector<FgResRef, 8> backbuffer_sources_;
    float min_exposure_ = 1.0f, max_exposure_ = 1.0f;
    float pre_exposure_ = 1.0f;

    ExShadowMaps ex_shadow_maps_ = {SHADOWMAP_WIDTH, SHADOWMAP_HEIGHT};
    ExSkydomeCube ex_skydome_cube_ = {prim_draw_};
    ExSkydomeScreen ex_skydome_ = {prim_draw_};
    ExDepthFill ex_depth_fill_;
    ExDepthHierarchy ex_depth_hierarchy_;
    ExGBufferFill ex_gbuffer_fill_;
    ExOpaque ex_opaque_;
    ExTransparent ex_transparent_ = {prim_draw_};
    ExEmissive ex_emissive_;
    ExOITDepthPeel ex_oit_depth_peel_;
    ExOITBlendLayer rp_oit_blend_layer_[OIT_LAYERS_COUNT] = {{prim_draw_}, {prim_draw_}, {prim_draw_}, {prim_draw_}};
    ExOITScheduleRays ex_oit_schedule_rays_;
    ExRTReflections ex_oit_rt_reflections_ = ExRTReflections{true};
    ExSSRCompose ex_ssr_compose_ = {prim_draw_};
    ExRTGI ex_rt_gi_;
    ExRTGICache ex_rt_gi_cache_;
    ExRTReflections ex_rt_reflections_ = ExRTReflections{false};
    ExRTShadows ex_rt_shadows_;
    ExSampleLights ex_sample_lights_;
    ExReadExposure ex_read_exposure_;
    ExCombine::Args ex_combine_args_;
    ExCombine ex_combine_ = {prim_draw_};

    ExDebugProbes ex_debug_probes_ = {prim_draw_};
    ExDebugRT ex_debug_rt_;
    ExDebugOIT ex_debug_oit_;

    ViewState view_state_;
    PrimDraw prim_draw_;
    uint32_t frame_index_ = 0, accumulated_frames_ = 0;

    Ren::Pipeline pi_skinning_, pi_gbuf_shade_[2];
    // HQ SSR
    Ren::Pipeline pi_ssr_classify_, pi_ssr_write_indirect_, pi_ssr_trace_hq_[2][2];
    Ren::Pipeline pi_rt_write_indirect_;
    // SSR Denoiser stuff
    Ren::Pipeline pi_ssr_reproject_, pi_ssr_prefilter_, pi_ssr_temporal_, pi_ssr_blur_[2], pi_ssr_stabilization_;
    // GI Cache
    Ren::Pipeline pi_probe_blend_[3], pi_probe_relocate_[2], pi_probe_classify_[2], pi_probe_sample_;
    // GTAO
    Ren::Pipeline pi_gtao_main_, pi_gtao_filter_, pi_gtao_accumulate_;
    // GI
    Ren::Pipeline pi_gi_classify_, pi_gi_write_indirect_, pi_gi_trace_ss_;
    Ren::Pipeline pi_gi_rt_write_indirect_;
    Ren::Pipeline pi_reconstruct_normals_;
    // GI Denoiser stuff
    Ren::Pipeline pi_gi_reproject_, pi_gi_prefilter_, pi_gi_temporal_, pi_gi_blur_[2], pi_gi_stabilization_;
    // Sun shadows
    Ren::Pipeline pi_shadow_classify_, pi_sun_shadows_, pi_shadow_prepare_mask_, pi_shadow_classify_tiles_,
        pi_shadow_filter_[3], pi_shadow_debug_;
    Ren::Pipeline pi_sun_brightness_;
    // Autoexposure
    Ren::Pipeline pi_histogram_sample_, pi_histogram_exposure_;
    // Sky
    Ren::Pipeline pi_sky_upsample_;
    // Debug
    Ren::Pipeline pi_debug_velocity_;

    Ren::ProgramRef blit_static_vel_prog_, blit_gauss2_prog_, blit_ao_prog_, blit_bilateral_prog_, blit_taa_prog_[2],
        blit_taa_static_prog_, blit_ssr_prog_, blit_ssr_dilate_prog_, blit_upscale_prog_, blit_down2_prog_,
        blit_down_depth_prog_, blit_ssr_compose_prog_;

    struct CommonBuffers {
        FgResRef skin_transforms_res, shape_keys_res, instance_indices_res, cells_res, rt_cells_res, lights_res,
            decals_res, items_res, rt_items_res, shared_data_res, atomic_cnt_res;
    };

    struct FrameTextures {
        Ren::Tex2DParams color_params;
        FgResRef color;
        Ren::Tex2DParams albedo_params;
        FgResRef albedo;
        Ren::Tex2DParams specular_params;
        FgResRef specular;
        Ren::Tex2DParams normal_params;
        FgResRef normal;
        Ren::Tex2DParams depth_params;
        FgResRef depth;
        Ren::Tex2DParams velocity_params;
        FgResRef velocity;

        FgResRef envmap;
        FgResRef shadowmap;
        FgResRef ssao;
        FgResRef gi;
        FgResRef sun_shadow;
        FgResRef exposure;

        FgResRef gi_cache_irradiance;
        FgResRef gi_cache_distance;
        FgResRef gi_cache_offset;

        FgResRef oit_depth_buf;
    };

    void InitSkyResources();

    void AddBuffersUpdatePass(CommonBuffers &common_buffers, const PersistentGpuData &persistent_data);
    void AddLightBuffersUpdatePass(CommonBuffers &common_buffers);
    void AddSkydomePass(const CommonBuffers &common_buffers, FrameTextures &frame_textures);
    void AddSunColorUpdatePass(CommonBuffers &common_buffers);
    void AddGBufferFillPass(const CommonBuffers &common_buffers, const PersistentGpuData &persistent_data,
                            const BindlessTextureData &bindless, FrameTextures &frame_textures);
    void AddDeferredShadingPass(const CommonBuffers &common_buffers, FrameTextures &frame_textures, bool enable_gi);
    void AddEmissivesPass(const CommonBuffers &common_buffers, const PersistentGpuData &persistent_data,
                          const BindlessTextureData &bindless, FrameTextures &frame_textures);
    void AddForwardOpaquePass(const CommonBuffers &common_buffers, const PersistentGpuData &persistent_data,
                              const BindlessTextureData &bindless, FrameTextures &frame_textures);
    void AddOITPasses(const CommonBuffers &common_buffers, const PersistentGpuData &persistent_data,
                      const AccelerationStructureData &acc_struct_data, const BindlessTextureData &bindless,
                      FgResRef depth_hierarchy, FgResRef rt_geo_instances_res, FgResRef rt_obj_instances_res,
                      FrameTextures &frame_textures);
    void AddForwardTransparentPass(const CommonBuffers &common_buffers, const PersistentGpuData &persistent_data,
                                   const BindlessTextureData &bindless, FrameTextures &frame_textures);

    void AddSSAOPasses(FgResRef depth_down_2x, FgResRef depth_tex, FgResRef &out_ssao);
    FgResRef AddGTAOPasses(FgResRef depth_tex, FgResRef velocity_tex, FgResRef norm_tex);
    void AddFillStaticVelocityPass(const CommonBuffers &common_buffers, FgResRef depth_tex,
                                   FgResRef &inout_velocity_tex);
    void AddFrameBlurPasses(const Ren::WeakTex2DRef &input_tex, FgResRef &output_tex);
    void AddTaaPass(const CommonBuffers &common_buffers, FrameTextures &frame_textures, bool static_accumulation,
                    FgResRef &resolved_color);
    void AddDownsampleColorPass(FgResRef input_tex, FgResRef &output_tex);
    void AddDownsampleDepthPass(const CommonBuffers &common_buffers, FgResRef depth_tex, FgResRef &out_depth_down_2x);

    void AddHQSpecularPasses(bool deferred_shading, bool debug_denoise, const CommonBuffers &common_buffers,
                             const PersistentGpuData &persistent_data, const AccelerationStructureData &acc_struct_data,
                             const BindlessTextureData &bindless, FgResRef depth_hierarchy,
                             FgResRef rt_geo_instances_res, FgResRef rt_obj_instances_res,
                             FrameTextures &frame_textures);
    void AddLQSpecularPasses(const CommonBuffers &common_buffers, FgResRef depth_down_2x,
                             FrameTextures &frame_textures);

    void AddDiffusePasses(const Ren::WeakTex2DRef &env_map, const Ren::WeakTex2DRef &lm_direct,
                          const Ren::WeakTex2DRef lm_indir_sh[4], bool debug_denoise,
                          const CommonBuffers &common_buffers, const PersistentGpuData &persistent_data,
                          const AccelerationStructureData &acc_struct_data, const BindlessTextureData &bindless,
                          FgResRef depth_hierarchy, FgResRef rt_geo_instances_res, FgResRef rt_obj_instances_res,
                          FrameTextures &frame_textures);

    void AddGICachePasses(const Ren::WeakTex2DRef &env_map, const CommonBuffers &common_buffers,
                          const PersistentGpuData &persistent_data, const AccelerationStructureData &acc_struct_data,
                          const BindlessTextureData &bindless, FgResRef rt_geo_instances_res,
                          FgResRef rt_obj_instances_res, FrameTextures &frame_textures);

    void AddHQSunShadowsPasses(const CommonBuffers &common_buffers, const PersistentGpuData &persistent_data,
                               const AccelerationStructureData &acc_struct_data, const BindlessTextureData &bindless,
                               FgResRef rt_geo_instances_res, FgResRef rt_obj_instances_res,
                               FrameTextures &frame_textures, bool debug_denoise);
    void AddLQSunShadowsPass(const CommonBuffers &common_buffers, const PersistentGpuData &persistent_data,
                             const AccelerationStructureData &acc_struct_data, const BindlessTextureData &bindless,
                             bool enabled, FrameTextures &frame_textures);

    FgResRef AddAutoexposurePasses(FgResRef hdr_texture);

    void AddDebugVelocityPass(FgResRef velocity, FgResRef &output_tex);

    void GatherDrawables(const SceneData &scene, const Ren::Camera &cam, const Ren::Camera &ext_cam, DrawList &list);

    bool InitPipelines();
    // void InitRendererInternal();
    void UpdatePixelFilterTable(ePixelFilter filter, float filter_width);

    // Parallel Jobs
    static void GatherObjectsForZSlice_Job(const Ren::Frustum &frustum, const SceneData &scene,
                                           const Ren::Vec3f &cam_pos, const Ren::Mat4f &clip_from_identity,
                                           uint64_t comp_mask, SWcull_ctx *cull_ctx, uint8_t visit_mask,
                                           ProcessedObjData proc_objects[],
                                           Ren::HashMap32<uint32_t, VisObjStorage> &out_visible_objects2);
    static void ClusterItemsForZSlice_Job(int slice, const Ren::Frustum *sub_frustums, const BBox *decals_boxes,
                                          const LightSource *const light_sources,
                                          Ren::Span<const uint32_t> litem_to_lsource, const DrawList &list,
                                          CellData out_cells[], ItemData out_items[], std::atomic_int &items_count);

    // Generate auxiliary textures
    static std::unique_ptr<uint16_t[]> Generate_BRDF_LUT(int res, std::string &out_c_header);
    static std::unique_ptr<int8_t[]> Generate_PeriodicPerlin(int res, std::string &out_c_header);
    static std::unique_ptr<uint8_t[]> Generate_SSSProfile_LUT(int res, int gauss_count, const float gauss_variances[],
                                                              const Ren::Vec3f diffusion_weights[]);
    static std::unique_ptr<int16_t[]> Generate_RandDirs(int res, std::string &out_c_header);
    static std::unique_ptr<uint8_t[]> Generate_ConeTraceLUT(int resx, int resy, const float cone_angles[4],
                                                            std::string &out_c_header);
    static std::vector<Ren::Vec4f> Generate_SkyTransmittanceLUT(const AtmosphereParams &params);
    static std::vector<Ren::Vec4f> Generate_SkyMultiscatterLUT(const AtmosphereParams &params,
                                                               Ren::Span<const Ren::Vec4f> transmittance_lut);
};
} // namespace Eng