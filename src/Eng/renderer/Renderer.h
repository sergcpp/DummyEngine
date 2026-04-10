#pragma once

#include <atomic>
#include <optional>

#include <Ren/Common.h>
#include <Ren/utils/ImageSplitter.h>
extern "C" {
#include <Ren/SW/SWculling.h>
}

#include "../scene/SceneData.h"
#include "PrimDraw.h"
#include "framegraph/FgBuilder.h"

#include "executors/ExPostprocess.h"
#include "executors/ExReadExposure.h"

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
static_assert(RAY_TYPE_VOLUME == int(AccStructure::eRayType::Volume));

class Renderer {
  public:
    Renderer(Ren::Context &ctx, ShaderLoader &sh, Random &rand, Sys::ThreadPool &threads);
    ~Renderer();

    void InitPipelines();

    Ren::Context &ctx() { return ctx_; }
    PrimDraw &prim_draw() { return prim_draw_; }

    int accumulated_frames() const { return accumulated_frames_; }
    void reset_accumulation() { frame_index_ = view_state_.frame_index = accumulated_frames_ = 0; }

    float readback_exposure() const {
        if (ex_read_exposure_.exposure() > 0.0f) {
            return std::min(std::max(ex_read_exposure_.exposure(), min_exposure_), max_exposure_);
        }
        return std::min(std::max(1.0f, min_exposure_), max_exposure_);
    }
    void set_pre_exposure(const float exposure) { custom_pre_exposure_ = exposure; }
    void reset_pre_exposure() { custom_pre_exposure_ = {}; }

    const backend_info_t &backend_info() const { return backend_info_; }

    void InitBackendInfo();

    void InitPipelinesForProgram(Ren::ProgramHandle prog, Ren::Bitmask<Ren::eMatFlags> mat_flags,
                                 Ren::SmallVectorImpl<Ren::PipelineHandle> &out_pipelines) const;

    void PrepareDrawList(const SceneData &scene, const Ren::Camera &cam, const Ren::Camera &ext_cam, DrawList &list);
    void ExecuteDrawList(const DrawList &list, const PersistentGpuData &persistent_data, Ren::ImageHandle target = {},
                         bool blit_to_backbuffer = false);

    void SetTonemapLUT(int res, Ren::eFormat format, Ren::Span<const uint8_t> data);

    void BlitPixelsTonemap(const uint8_t *data, int w, int h, int stride, Ren::eFormat format, float gamma,
                           float min_exposure, float max_exposure, Ren::ImageHandle target, bool compressed,
                           bool blit_to_backbuffer = false);
    void BlitImageTonemap(Ren::ImageHandle result, int w, int h, Ren::eFormat format, float gamma, float min_exposure,
                          float max_exposure, Ren::ImageHandle target, bool compressed,
                          bool blit_to_backbuffer = false);
    render_settings_t settings = {};

  private:
    Ren::Context &ctx_;
    ShaderLoader &sh_;
    Random &rand_;
    Sys::ThreadPool &threads_;
    SWcull_ctx cull_ctx_ = {};

    Ren::ImageHandle noise_;
    Ren::ImageHandle dummy_black_, dummy_white_, brdf_lut_, ltc_luts_, cone_rt_lut_;
    Ren::ImageHandle tonemap_lut_;
    Ren::BufferHandle bn_pmj_2D_64spp_seq_buf_;
    Ren::BufferHandle pmj_samples_buf_;
    Ren::ImageHandle stbn_1D_64spp_;
    Ren::ImageHandle sky_transmittance_lut_, sky_multiscatter_lut_, sky_moon_, sky_weather_, sky_cirrus_, sky_curl_;
    Ren::ImageHandle sky_noise3d_;

    Ren::ImageHandle shadow_depth_, shadow_color_;
    Ren::SamplerHandle nearest_sampler_, linear_sampler_;
    eTAAMode taa_mode_ = eTAAMode::Off;
    bool dof_enabled_ = false;

    Ren::VertexInputHandle draw_pass_vi_;
    Ren::RenderPassHandle rp_main_draw_;
    Ren::RastState rast_states_[int(eFwdPipeline::_Count)];

    Ren::ImageSplitter shadow_splitter_;

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
        Ren::Vec3d world_origin;
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
    Ren::ImageROHandle env_map_;
    const DrawList *p_list_;
    Ren::SmallVector<std::variant<FgBufRWHandle, FgImgRWHandle>, 8> backbuffer_sources_;
    float min_exposure_ = 1.0f, max_exposure_ = 1.0f;
    std::optional<float> custom_pre_exposure_;

    ExReadExposure ex_read_exposure_;
    ExPostprocess::Args ex_postprocess_args_;

    view_state_t view_state_;
    PrimDraw prim_draw_;
    uint32_t frame_index_ = 0, accumulated_frames_ = 0;

    Ren::PipelineHandle pi_gbuf_shade_[2];
    // Specular GI
    Ren::PipelineHandle pi_specular_classify_, pi_specular_write_indirect_[3], pi_specular_trace_ss_[2][2],
        pi_specular_shade_[9];
    // Specular GI Denoiser stuff
    Ren::PipelineHandle pi_specular_reproject_, pi_specular_temporal_[2], pi_specular_filter_[4],
        pi_specular_stabilization_;
    Ren::PipelineHandle pi_tile_clear_[4];
    // GI Cache
    Ren::PipelineHandle pi_probe_blend_[3][2], pi_probe_relocate_[3], pi_probe_classify_[5], pi_probe_sample_;
    // GTAO
    Ren::PipelineHandle pi_gtao_main_[2], pi_gtao_filter_[2], pi_gtao_accumulate_[2];
    // Diffuse GI
    Ren::PipelineHandle pi_diffuse_classify_, pi_diffuse_write_indirect_[3], pi_diffuse_trace_ss_, pi_diffuse_shade_[7];
    // Diffuse GI Denoiser stuff
    Ren::PipelineHandle pi_diffuse_reproject_, pi_diffuse_temporal_[2], pi_diffuse_filter_[4],
        pi_diffuse_stabilization_;
    // Sun shadows
    Ren::PipelineHandle pi_shadow_classify_, pi_sun_shadows_[2], pi_shadow_prepare_mask_, pi_shadow_classify_tiles_,
        pi_shadow_filter_[3], pi_shadow_debug_;
    Ren::PipelineHandle pi_sun_brightness_;
    // Bloom
    Ren::PipelineHandle pi_bloom_downsample_[2][2], pi_bloom_upsample_[2];
    // Autoexposure
    Ren::PipelineHandle pi_histogram_sample_, pi_histogram_exposure_;
    // Volumetrics
    Ren::PipelineHandle pi_sky_upsample_;
    Ren::PipelineHandle pi_vol_scatter_[2][2], pi_vol_ray_march_;
    // TSR
    Ren::PipelineHandle pi_reconstruct_depth_, pi_prepare_disocclusion_, pi_sharpen_[2];
    // Motion blur
    Ren::PipelineHandle pi_motion_blur_classify_[2], pi_motion_blur_dilate_, pi_motion_blur_filter_;
    // Debug
    Ren::PipelineHandle pi_debug_velocity_, pi_debug_gbuffer_[4];

    Ren::ProgramHandle blit_static_vel_prog_, blit_gauss_prog_, blit_ao_prog_, blit_bilateral_prog_, blit_tsr_prog_,
        blit_tsr_static_prog_, blit_ssr_prog_, blit_ssr_dilate_prog_, blit_upscale_prog_, blit_down_prog_,
        blit_down_depth_prog_, blit_ssr_compose_prog_, blit_fxaa_prog_, blit_vol_compose_prog_;

    struct CommonBuffers {
        FgBufRWHandle cells, rt_cells, lights, decals, items, rt_items;
        FgBufRWHandle skin_transforms, shape_keys, instance_indices, shared_data, atomic_cnt;

        // External
        FgBufRWHandle vertex_buf1, vertex_buf2, indices_buf;
        FgBufRWHandle skin_vertex_buf, delta_buf;

        FgBufROHandle pmj_samples;
        FgBufROHandle bn_pmj_2D_64spp_seq;

        FgBufROHandle instances;
        FgBufROHandle materials;

        FgBufROHandle stoch_lights, stoch_lights_nodes;
    };

    struct AccelerationStructures {
        FgBufRWHandle rt_tlas_buf[int(eTLASIndex::_Count)];
        struct {
            uint32_t rt_tlas_build_scratch_size = 0;
        } hwrt;
        struct {
            FgBufROHandle rt_prim_indices;
            uint32_t rt_root_node = 0;
            FgBufROHandle rt_blas_buf;
        } swrt;

        Ren::AccStructHandle rt_tlases[int(eTLASIndex::_Count)];
    };

    struct FrameTextures {
        FgImgDesc color_desc;
        FgImgRWHandle color;
        FgImgDesc albedo_desc;
        FgImgRWHandle albedo;
        FgImgDesc specular_desc;
        FgImgRWHandle specular;
        FgImgDesc normal_desc;
        FgImgRWHandle normal;
        FgImgDesc depth_desc;
        FgImgRWHandle depth, opaque_depth;
        FgImgDesc velocity_desc;
        FgImgRWHandle velocity;

        FgImgROHandle exposure;
        FgImgROHandle sun_shadow;
        FgBufRWHandle oit_depth;

        FgImgRWHandle ssao;
        FgImgROHandle gi_diffuse;

        FgImgROHandle dilated_depth;
        FgImgROHandle dilated_velocity;
        FgImgRWHandle disocclusion_mask;

        // External
        FgImgRWHandle envmap;
        FgImgRWHandle shadow_depth, shadow_color;

        FgImgROHandle noise;
        FgImgROHandle dummy_white;
        FgImgROHandle dummy_black;
        FgImgROHandle ltc_luts;
        FgImgROHandle brdf_lut;
        FgImgROHandle cone_rt_lut;
        FgImgROHandle stbn_1D_64spp;
        FgImgROHandle tonemap_lut;

        FgImgRWHandle gi_cache_irradiance;
        FgImgRWHandle gi_cache_distance;
        FgImgRWHandle gi_cache_offset;

        FgImgROHandle sky_transmittance_lut;
        FgImgROHandle sky_multiscatter_lut;
        FgImgROHandle sky_moon;
        FgImgROHandle sky_weather;
        FgImgROHandle sky_cirrus;
        FgImgROHandle sky_curl;
        FgImgROHandle sky_noise3d;
    };

    void AddBuffersUpdatePass(CommonBuffers &common_buffers, const PersistentGpuData &persistent_data);
    void AddLightBuffersUpdatePass(CommonBuffers &common_buffers);
    void AddGBufferFillPass(const CommonBuffers &common_buffers, const BindlessTextureData &bindless,
                            FrameTextures &frame_textures);
    void AddDeferredShadingPass(const CommonBuffers &common_buffers, FrameTextures &frame_textures, bool enable_gi);
    void AddEmissivePass(const CommonBuffers &common_buffers, const PersistentGpuData &persistent_data,
                         const BindlessTextureData &bindless, FrameTextures &frame_textures);
    void AddForwardOpaquePass(const CommonBuffers &common_buffers, const PersistentGpuData &persistent_data,
                              const BindlessTextureData &bindless, FrameTextures &frame_textures);

    void AddFillStaticVelocityPass(const CommonBuffers &common_buffers, FgImgRWHandle depth,
                                   FgImgRWHandle &inout_velocity);
    std::tuple<FgImgROHandle, FgImgROHandle, FgImgRWHandle> AddDisocclusionPasses(FgImgROHandle depth,
                                                                                  FgImgROHandle velocity);
    FgImgRWHandle AddTSRPass(const FrameTextures &frame_textures, eTAAMode taa_mode);
    FgImgRWHandle AddSharpenPass(FgImgROHandle input, FgImgROHandle exposure, bool compressed);
    FgImgRWHandle AddMotionBlurPasses(FgImgROHandle input, FrameTextures &frame_textures);
    FgImgRWHandle AddDownsampleDepthPass(const CommonBuffers &common_buffers, FgImgROHandle depth);

    // GI Cache
    void AddGICachePasses(const CommonBuffers &common_buffers, const PersistentGpuData &persistent_data,
                          const AccelerationStructures &acc_structs, const BindlessTextureData &bindless,
                          FgBufROHandle rt_geo_instances_res, FgBufROHandle rt_obj_instances_res,
                          FrameTextures &frame_textures);

    // GI Diffuse
    void AddDiffusePasses(bool debug_denoise, const CommonBuffers &common_buffers,
                          const AccelerationStructures &acc_struct, const BindlessTextureData &bindless,
                          FgImgROHandle depth_hierarchy, FgBufROHandle rt_geo_instances_res,
                          FgBufROHandle rt_obj_instances_res, FrameTextures &frame_textures);
    FgImgRWHandle AddSSAOPasses(FgImgROHandle depth_down_2x, FgImgROHandle depth);
    FgImgRWHandle AddGTAOPasses(eSSAOQuality quality, FgImgROHandle depth, FgImgROHandle velocity, FgImgROHandle norm);

    // GI Specular
    void AddHQSpecularPasses(bool deferred_shading, bool debug_denoise, const CommonBuffers &common_buffers,
                             const AccelerationStructures &acc_structs, const BindlessTextureData &bindless,
                             FgImgROHandle depth_hierarchy, FgBufROHandle rt_geo_instances_res,
                             FgBufROHandle rt_obj_instances_res, FrameTextures &frame_textures);
    void AddLQSpecularPasses(const CommonBuffers &common_buffers, FgImgROHandle depth_down_2x,
                             FrameTextures &frame_textures);

    // Sun Shadows
    FgImgRWHandle AddHQSunShadowsPasses(const CommonBuffers &common_buffers, const AccelerationStructures &acc_structs,
                                        const BindlessTextureData &bindless, FgBufROHandle rt_geo_instances_res,
                                        FgBufROHandle rt_obj_instances_res, const FrameTextures &frame_textures,
                                        bool debug_denoise);
    FgImgRWHandle AddLQSunShadowsPass(const CommonBuffers &common_buffers, const BindlessTextureData &bindless,
                                      const FrameTextures &frame_textures);

    // Transparency
    void AddOITPasses(const CommonBuffers &common_buffers, const AccelerationStructures &acc_structs,
                      const BindlessTextureData &bindless, FgImgROHandle depth_hierarchy,
                      FgBufROHandle rt_geo_instances_res, FgBufROHandle rt_obj_instances_res,
                      FrameTextures &frame_textures);
    void AddForwardTransparentPass(const CommonBuffers &common_buffers, const PersistentGpuData &persistent_data,
                                   const BindlessTextureData &bindless, FrameTextures &frame_textures);

    // Volumetrics
    void InitSkyResources();
    void AddSkydomePass(const CommonBuffers &common_buffers, FrameTextures &frame_textures);
    void AddSunColorUpdatePass(CommonBuffers &common_buffers, const FrameTextures &frame_textures);
    void AddVolumetricPasses(const CommonBuffers &common_buffers, const AccelerationStructures &acc_structs,
                             FgBufROHandle rt_geo_instances_res, FgBufROHandle rt_obj_instances_res,
                             FrameTextures &frame_textures);

    // Debugging
    FgImgRWHandle AddDebugVelocityPass(FgImgROHandle velocity);
    FgImgRWHandle AddDebugGBufferPass(const FrameTextures &frame_textures, int pi_index);

    void GatherDrawables(const SceneData &scene, const Ren::Camera &cam, const Ren::Camera &ext_cam, DrawList &list);

    void UpdatePixelFilterTable(ePixelFilter filter, float filter_width);

    // Postprocess
    FgImgRWHandle AddAutoexposurePasses(FgImgROHandle hdr_texture,
                                        Ren::Vec2f adaptation_speed = Ren::Vec2f{0.15f, 0.01f});
    FgImgRWHandle AddBloomPasses(FgImgROHandle hdr_texture, FgImgROHandle exposure_texture, bool compressed);

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