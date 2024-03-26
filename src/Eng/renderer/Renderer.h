#pragma once

#include <atomic>

#include <Ren/Common.h>
#include <Ren/TextureSplitter.h>
extern "C" {
#include <Ren/SW/SWculling.h>
}

#include "../scene/SceneData.h"
#include "PrimDraw.h"
#include "passes/RpBuildAccStructures.h"
#include "passes/RpCombine.h"
#include "passes/RpDOF.h"
#include "passes/RpDebugEllipsoids.h"
#include "passes/RpDebugProbes.h"
#include "passes/RpDebugRT.h"
#include "passes/RpDebugTextures.h"
#include "passes/RpDepthFill.h"
#include "passes/RpDepthHierarchy.h"
#include "passes/RpFXAA.h"
#include "passes/RpGBufferFill.h"
#include "passes/RpOpaque.h"
#include "passes/RpRTGI.h"
#include "passes/RpRTReflections.h"
#include "passes/RpRTShadows.h"
#include "passes/RpReadBrightness.h"
#include "passes/RpSSRCompose.h"
#include "passes/RpSSRCompose2.h"
#include "passes/RpSampleBrightness.h"
#include "passes/RpShadowMaps.h"
#include "passes/RpSkinning.h"
#include "passes/RpSkydome.h"
#include "passes/RpTransparent.h"
#include "passes/RpUpdateAccBuffers.h"

#include "Renderer_DrawList.h"

namespace Sys {
template <typename T> class MonoAlloc;
class ThreadPool;
} // namespace Sys

namespace Eng {
class Random;
class ShaderLoader;

class Renderer {
  public:
    Renderer(Ren::Context &ctx, ShaderLoader &sh, Random &rand, Sys::ThreadPool &threads);
    ~Renderer();

    void reset_accumulation() { accumulated_frames_ = 0; }

    const BackendInfo &backend_info() const { return backend_info_; }

    void InitBackendInfo();

    void InitPipelinesForProgram(const Ren::ProgramRef &prog, Ren::Bitmask<Ren::eMatFlags> mat_flags,
                                 Ren::PipelineStorage &storage,
                                 Ren::SmallVectorImpl<Ren::PipelineRef> &out_pipelines) const;

    void PrepareDrawList(const SceneData &scene, const Ren::Camera &cam, const Ren::Camera &ext_cam, DrawList &list);
    void ExecuteDrawList(const DrawList &list, const PersistentGpuData &persistent_data,
                         const Ren::Tex2DRef target = {});

    void SetTonemapLUT(int res, Ren::eTexFormat format, Ren::Span<const uint8_t> data);

    void BlitPixelsTonemap(const uint8_t *data, int w, int h, int stride, Ren::eTexFormat format, float gamma,
                           float exposure);
    render_settings_t settings = {};

  private:
    Ren::Context &ctx_;
    ShaderLoader &sh_;
    Random &rand_;
    Sys::ThreadPool &threads_;
    SWcull_ctx cull_ctx_ = {};
    Ren::ProgramRef blit_prog_, blit_ms_prog_, blit_combine_prog_, blit_down_prog_, blit_gauss_prog_, blit_depth_prog_,
        blit_rgbm_prog_, blit_mipmap_prog_, blit_prefilter_prog_, blit_project_sh_prog_;

    Ren::Tex2DRef dummy_black_, dummy_white_, rand2d_8x8_, rand2d_dirs_4x4_, brdf_lut_, ltc_luts_, cone_rt_lut_,
        noise_tex_;
    Ren::Tex3DRef tonemap_lut_;
    Ren::BufferRef readback_buf_;
    Ren::BufferRef sobol_seq_buf_, scrambling_tile_1spp_buf_, ranking_tile_1spp_buf_;

    // FrameBuf probe_sample_buf_;
    Ren::Tex2DRef shadow_map_tex_;
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

    RpBuilder rp_builder_;
    std::optional<render_settings_t> cached_settings_;
    int cached_rp_index_ = 0;
    Ren::WeakTex2DRef env_map_;
    Ren::WeakTex2DRef lm_direct_, lm_indir_, lm_indir_sh_[4];
    const DrawList *p_list_;
    const Ren::ProbeStorage *probe_storage_ = nullptr;
    Ren::SmallVector<RpResRef, 8> backbuffer_sources_;

    RpShadowMaps rp_shadow_maps_ = {SHADOWMAP_WIDTH, SHADOWMAP_HEIGHT};
    RpSkydome rp_skydome_ = {prim_draw_};
    RpDepthFill rp_depth_fill_;
    RpDepthHierarchy rp_depth_hierarchy_;
    RpGBufferFill rp_gbuffer_fill_;
    RpOpaque rp_opaque_;
    RpTransparent rp_transparent_ = {prim_draw_};
    RpSSRCompose rp_ssr_compose_ = {prim_draw_};
    RpSSRCompose2 rp_ssr_compose2_ = {prim_draw_};
    RpRTGI rp_rt_gi_;
    RpRTReflections rp_rt_reflections_;
    RpRTShadows rp_rt_shadows_;
    RpSampleBrightness rp_sample_brightness_ = {prim_draw_, Ren::Vec2i{16, 8}};
    RpReadBrightness rp_read_brightness_;
    RpCombineData rp_combine_data_;
    RpCombine rp_combine_ = {prim_draw_};

    RpDebugRT rp_debug_rt_;

    ViewState view_state_;
    PrimDraw prim_draw_;
    uint32_t frame_index_ = 0, accumulated_frames_ = 0;

    Ren::Pipeline pi_skinning_, pi_gbuf_shade_, pi_gbuf_shade_hq_;
    // HQ SSR
    Ren::Pipeline pi_ssr_classify_tiles_[2], pi_ssr_write_indirect_, pi_ssr_trace_hq_;
    Ren::Pipeline pi_rt_write_indirect_;
    // SSR Denoiser stuff
    Ren::Pipeline pi_ssr_reproject_[2], pi_ssr_prefilter_[2], pi_ssr_resolve_temporal_[2];
    // GI
    Ren::Pipeline pi_gi_classify_tiles_, pi_gi_write_indirect_, pi_gi_trace_ss_;
    Ren::Pipeline pi_gi_rt_write_indirect_;
    Ren::Pipeline pi_reconstruct_normals_;
    // GI Denoiser stuff
    Ren::Pipeline pi_gi_reproject_, pi_gi_prefilter_, pi_gi_resolve_temporal_, pi_gi_blur_, pi_gi_post_blur_;
    // Sun shadows
    Ren::Pipeline pi_shadow_classify_, pi_sun_shadows_, pi_shadow_prepare_mask_, pi_shadow_classify_tiles_,
        pi_shadow_filter_[3], pi_shadow_debug_;
    // Debug
    Ren::Pipeline pi_debug_velocity_;

    Ren::ProgramRef blit_static_vel_prog_, blit_gauss2_prog_, blit_ao_prog_, blit_bilateral_prog_, blit_taa_prog_,
        blit_taa_static_prog_, blit_ssr_prog_, blit_ssr_dilate_prog_, blit_upscale_prog_, blit_down2_prog_,
        blit_down_depth_prog_, blit_ssr_compose_prog_;

    struct CommonBuffers {
        RpResRef skin_transforms_res, shape_keys_res, instance_indices_res, cells_res, rt_cells_res, lights_res,
            decals_res, items_res, rt_items_res, shared_data_res, atomic_cnt_res;
    };

    struct FrameTextures {
        Ren::Tex2DParams color_params;
        RpResRef color;
        Ren::Tex2DParams albedo_params;
        RpResRef albedo;
        Ren::Tex2DParams specular_params;
        RpResRef specular;
        Ren::Tex2DParams normal_params;
        RpResRef normal;
        Ren::Tex2DParams depth_params;
        RpResRef depth;
        Ren::Tex2DParams velocity_params;
        RpResRef velocity;

        RpResRef shadowmap;
        RpResRef ssao;
        RpResRef gi;
        RpResRef sun_shadow;
    };

    void AddBuffersUpdatePass(CommonBuffers &common_buffers);
    void AddLightBuffersUpdatePass(CommonBuffers &common_buffers);
    void AddSkydomePass(const CommonBuffers &common_buffers, bool clear, FrameTextures &frame_textures);
    void AddGBufferFillPass(const CommonBuffers &common_buffers, const PersistentGpuData &persistent_data,
                            const BindlessTextureData &bindless, FrameTextures &frame_textures);
    void AddDeferredShadingPass(const CommonBuffers &common_buffers, FrameTextures &frame_textures, bool enable_gi);
    void AddForwardOpaquePass(const CommonBuffers &common_buffers, const PersistentGpuData &persistent_data,
                              const BindlessTextureData &bindless, FrameTextures &frame_textures);
    void AddForwardTransparentPass(const CommonBuffers &common_buffers, const PersistentGpuData &persistent_data,
                                   const BindlessTextureData &bindless, FrameTextures &frame_textures);

    void AddSSAOPasses(RpResRef depth_down_2x, RpResRef depth_tex, RpResRef &out_ssao);
    void AddFillStaticVelocityPass(const CommonBuffers &common_buffers, RpResRef depth_tex,
                                   RpResRef &inout_velocity_tex);
    void AddFrameBlurPasses(const Ren::WeakTex2DRef &input_tex, RpResRef &output_tex);
    void AddTaaPass(const CommonBuffers &common_buffers, FrameTextures &frame_textures, float max_exposure,
                    bool static_accumulation, RpResRef &resolved_color);
    void AddDownsampleColorPass(RpResRef input_tex, RpResRef &output_tex);
    void AddDownsampleDepthPass(const CommonBuffers &common_buffers, RpResRef depth_tex, RpResRef &out_depth_down_2x);

    void AddHQSpecularPasses(const Ren::WeakTex2DRef &env_map, const Ren::WeakTex2DRef &lm_direct,
                             const Ren::WeakTex2DRef lm_indir_sh[4], bool deferred_shading, bool debug_denoise,
                             const Ren::ProbeStorage *probe_storage, const CommonBuffers &common_buffers,
                             const PersistentGpuData &persistent_data, const AccelerationStructureData &acc_struct_data,
                             const BindlessTextureData &bindless, RpResRef depth_hierarchy,
                             RpResRef rt_obj_instances_res, FrameTextures &frame_textures);
    void AddLQSpecularPasses(const Ren::ProbeStorage *probe_storage, const CommonBuffers &common_buffers,
                             RpResRef depth_down_2x, FrameTextures &frame_textures);

    void AddDiffusePasses(const Ren::WeakTex2DRef &env_map, const Ren::WeakTex2DRef &lm_direct,
                          const Ren::WeakTex2DRef lm_indir_sh[4], bool debug_denoise,
                          const Ren::ProbeStorage *probe_storage, const CommonBuffers &common_buffers,
                          const PersistentGpuData &persistent_data, const AccelerationStructureData &acc_struct_data,
                          const BindlessTextureData &bindless, const RpResRef depth_hierarchy,
                          RpResRef rt_obj_instances_res, FrameTextures &frame_textures);

    void AddHQSunShadowsPasses(const CommonBuffers &common_buffers, const PersistentGpuData &persistent_data,
                               const AccelerationStructureData &acc_struct_data, const BindlessTextureData &bindless,
                               RpResRef rt_obj_instances_res, FrameTextures &frame_textures, bool debug_denoise);
    void AddLQSunShadowsPasses(const CommonBuffers &common_buffers, const PersistentGpuData &persistent_data,
                               const AccelerationStructureData &acc_struct_data, const BindlessTextureData &bindless,
                               bool enabled, FrameTextures &frame_textures);

    void AddDebugVelocityPass(RpResRef velocity, RpResRef &output_tex);

    void GatherDrawables(const SceneData &scene, const Ren::Camera &cam, const Ren::Camera &ext_cam, DrawList &list);

    void InitPipelines();
    // void InitRendererInternal();

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
};
} // namespace Eng