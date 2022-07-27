#pragma once

#include <atomic>

#include <Ren/Common.h>
#include <Ren/TextureSplitter.h>
extern "C" {
#include <Ren/SW/SWculling.h>
}

#include "../Scene/SceneData.h"
#include "FrameBuf.h"
#include "Passes/RpBuildAccStructures.h"
#include "Passes/RpCombine.h"
#include "Passes/RpDOF.h"
#include "Passes/RpDebugEllipsoids.h"
#include "Passes/RpDebugProbes.h"
#include "Passes/RpDebugRT.h"
#include "Passes/RpDebugTextures.h"
#include "Passes/RpDepthFill.h"
#include "Passes/RpDepthHierarchy.h"
#include "Passes/RpDownColor.h"
#include "Passes/RpDownDepth.h"
#include "Passes/RpFXAA.h"
#include "Passes/RpGBufferFill.h"
#include "Passes/RpGBufferShade.h"
#include "Passes/RpOpaque.h"
#include "Passes/RpRTGI.h"
#include "Passes/RpRTReflections.h"
#include "Passes/RpRTShadows.h"
#include "Passes/RpReadBrightness.h"
#include "Passes/RpSSRBlur.h"
#include "Passes/RpSSRCompose.h"
#include "Passes/RpSSRCompose2.h"
#include "Passes/RpSSRDilate.h"
#include "Passes/RpSSRTrace.h"
#include "Passes/RpSSRVSDepth.h"
#include "Passes/RpSampleBrightness.h"
#include "Passes/RpShadowMaps.h"
#include "Passes/RpSkinning.h"
#include "Passes/RpSkydome.h"
#include "Passes/RpTransparent.h"
#include "Passes/RpUpdateAccBuffers.h"
#include "Passes/RpUpscale.h"
#include "PrimDraw.h"

#include "Renderer_DrawList.h"
#include "Renderer_GL_Defines.inl"

namespace Sys {
template <typename T> class MonoAlloc;
class ThreadPool;
} // namespace Sys

class Random;
class ShaderLoader;

class Renderer {
  public:
    Renderer(Ren::Context &ctx, ShaderLoader &sh, Random &rand, std::shared_ptr<Sys::ThreadPool> threads);
    ~Renderer();

    uint64_t render_flags() const { return render_flags_; }

    void set_render_flags(const uint64_t f) { render_flags_ = f; }

    const BackendInfo &backend_info() const { return backend_info_; }

    void InitBackendInfo();

    void PrepareDrawList(const SceneData &scene, const Ren::Camera &cam, DrawList &list);
    void ExecuteDrawList(const DrawList &list, const PersistentGpuData &persistent_data,
                         const FrameBuf *target = nullptr);

    void BlitPixels(const void *data, int w, int h, Ren::eTexFormat format);
    void BlitPixelsTonemap(const void *data, int w, int h, Ren::eTexFormat format);
    void BlitBuffer(float px, float py, float sx, float sy, const FrameBuf &buf, int first_att, int att_count,
                    float multiplier = 1.0f);
    void BlitTexture(float px, float py, float sx, float sy, const Ren::Tex2DRef &tex, float multiplier = 1.0f,
                     bool is_ms = false);

    void BlitToTempProbeFace(const FrameBuf &src_buf, const Ren::ProbeStorage &dst_store, int face);
    void BlitPrefilterFromTemp(const Ren::ProbeStorage &dst_store, int probe_index);
    bool BlitProjectSH(const Ren::ProbeStorage &store, int probe_index, int iteration, LightProbe &probe);

  private:
    Ren::Context &ctx_;
    ShaderLoader &sh_;
    Random &rand_;
    std::shared_ptr<Sys::ThreadPool> threads_;
    SWcull_ctx cull_ctx_ = {};
    Ren::ProgramRef blit_prog_, blit_ms_prog_, blit_combine_prog_, blit_down_prog_, blit_gauss_prog_, blit_depth_prog_,
        blit_rgbm_prog_, blit_mipmap_prog_, blit_prefilter_prog_, blit_project_sh_prog_;
    Ren::Tex2DRef dummy_black_, dummy_white_, rand2d_8x8_, rand2d_dirs_4x4_, brdf_lut_, cone_rt_lut_, noise_tex_;
    Ren::BufferRef readback_buf_;
    Ren::BufferRef sobol_seq_buf_, scrambling_tile_1spp_buf_, ranking_tile_1spp_buf_;

    FrameBuf probe_sample_buf_;
    Ren::Tex2DRef shadow_map_tex_;
    Ren::Tex2DRef down_tex_4x_;
    Ren::Framebuffer blur_tex_fb_[2], down_tex_4x_fb_;
    bool taa_enabled_ = false, dof_enabled_ = false;

    Ren::TextureSplitter shadow_splitter_;

    static const uint64_t DefaultFlags =
#if !defined(__ANDROID__)
        (EnableZFill | EnableCulling | EnableSSR | EnableSSR_HQ | EnableSSAO | EnableLightmap | EnableLights |
         EnableDecals | EnableShadows | EnableTonemap | EnableBloom | EnableTaa | EnableTimers | EnableDOF /*|
         EnableRTShadows | EnableDeferred*/);
#else
        (EnableZFill | EnableCulling | EnableSSR | EnableLightmap | EnableLights | EnableDecals | EnableShadows |
         EnableTonemap | EnableDOF | EnableTimers);
#endif
    uint64_t render_flags_ = DefaultFlags;

    DynArray<const LightSource *> litem_to_lsource_;
    DynArray<const Decal *> ditem_to_decal_;

    struct ProcessedObjData {
        int32_t instance_index;
        uint32_t base_vertex;
        int32_t rt_sh_index;
    };
    DynArray<ProcessedObjData> proc_objects_;
    DynArray<BBox> decals_boxes_;
    BackendInfo backend_info_;
    uint64_t backend_cpu_start_ = 0, backend_cpu_end_ = 0;
    int backend_gpu_start_ = -1, backend_gpu_end_ = -1;
    int64_t backend_time_diff_ = 0;
    float reduced_average_ = 0.0f;
    Ren::Vec4f prev_wind_scroll_;

    DynArray<Ren::Frustum> temp_sub_frustums_;
    DynArray<SortSpan32> temp_sort_spans_32_[2];
    DynArray<SortSpan64> temp_sort_spans_64_[2];

    std::vector<float> temp_depth;

    struct ShadowFrustumCache {
        bool valid = false;
        Ren::Vec3f view_pos, view_dir;
        Ren::Mat4f clip_from_world;
    };

    ShadowFrustumCache sun_shadow_cache_[4];

    uint32_t temp_buf1_vtx_offset_, temp_buf2_vtx_offset_, temp_buf_ndx_offset_, skinned_buf1_vtx_offset_,
        skinned_buf2_vtx_offset_;

#if defined(USE_GL_RENDER)
    Ren::Tex2DRef temp_tex_;

    uint32_t temp_framebuf_ = 0;

    // uint32_t unif_shared_data_block_[FrameSyncWindow];
    Ren::VertexInput temp_vtx_input_;
    //::Tex1DRef lights_tbo_[FrameSyncWindow], decals_tbo_[FrameSyncWindow];
    uint32_t /*reduce_pbo_[FrameSyncWindow], */ probe_sample_pbo_;
#endif

    DynArray<ShadReg> allocated_shadow_regions_;

#if defined(__ANDROID__)
    static const int SHADOWMAP_WIDTH = REN_SHAD_RES_ANDROID;
#else
    static const int SHADOWMAP_WIDTH = REN_SHAD_RES_PC;
#endif
    static const int SHADOWMAP_HEIGHT = SHADOWMAP_WIDTH / 2;
    // Sun shadow occupies half of atlas
    static const int SUN_SHADOW_RES = SHADOWMAP_WIDTH / 2;

    RpBuilder rp_builder_;
    uint64_t cached_render_flags_ = 0;
    Ren::WeakTex2DRef env_map_;
    Ren::WeakTex2DRef lm_direct_, lm_indir_, lm_indir_sh_[4];
    const DrawList *p_list_;
    const Ren::ProbeStorage *probe_storage_ = nullptr;
    Ren::SmallVector<RpResRef, 8> backbuffer_sources_;

    RpShadowMaps rp_shadow_maps_ = {SHADOWMAP_WIDTH, SHADOWMAP_HEIGHT};
    RpSkydome rp_skydome_ = {prim_draw_};
    RpDepthFill rp_depth_fill_;
    RpDownColor rp_down_color_ = {prim_draw_};
    RpDownDepth rp_down_depth_ = {prim_draw_};
    RpDepthHierarchy rp_depth_hierarchy_;
    RpUpscale rp_ssao_upscale_ = {prim_draw_};
    RpGBufferFill rp_gbuffer_fill_;
    RpOpaque rp_opaque_;
    RpTransparent rp_transparent_ = {prim_draw_};
    RpSSRTrace rp_ssr_trace_ = {prim_draw_};
    RpSSRDilate rp_ssr_dilate_ = {prim_draw_};
    RpSSRCompose rp_ssr_compose_ = {prim_draw_};
    RpSSRCompose2 rp_ssr_compose2_ = {prim_draw_};
    RpRTGI rp_rt_gi_;
    RpRTReflections rp_rt_reflections_;
    RpRTShadows rp_rt_shadows_;
    RpSampleBrightness rp_sample_brightness_ = {prim_draw_, Ren::Vec2i{16, 8}};
    RpReadBrightness rp_read_brightness_;
    RpCombineData rp_combine_data_;
    RpCombine rp_combine_ = {prim_draw_};

#if defined(USE_VK_RENDER)
    RpDebugRT rp_debug_rt_;
#elif defined(USE_GL_RENDER)
    // RpDOF rp_dof_ = {prim_draw_};
    // RpFXAA rp_fxaa_ = {prim_draw_};

    // debugging passes
    // RpDebugEllipsoids rp_debug_ellipsoids_ = {prim_draw_};
    // RpDebugProbes rp_debug_probes_ = {prim_draw_};
    // RpDebugTextures rp_debug_textures_ = {prim_draw_};
#endif

    ViewState view_state_;
    PrimDraw prim_draw_;
    uint32_t frame_index_ = 0;

    Ren::Pipeline pi_skinning_, pi_gbuf_shade_;
    // HQ SSR
    Ren::Pipeline pi_ssr_classify_tiles_, pi_ssr_write_indirect_, pi_ssr_trace_hq_;
    Ren::Pipeline pi_rt_write_indirect_;
    // SSR Denoiser stuff
    Ren::Pipeline pi_ssr_reproject_, pi_ssr_prefilter_, pi_ssr_resolve_temporal_;
    // GI
    Ren::Pipeline pi_gi_classify_tiles_, pi_gi_write_indirect_, pi_gi_trace_ss_;
    Ren::Pipeline pi_gi_rt_write_indirect_;
    Ren::Pipeline pi_reconstruct_normals_;
    // GI Denoiser stuff
    Ren::Pipeline pi_gi_reproject_, pi_gi_prefilter_, pi_gi_resolve_temporal_, pi_gi_blur_, pi_gi_post_blur_;
    // Sun shadows
    Ren::Pipeline pi_shadow_classify_, pi_sun_shadows_, pi_shadow_prepare_mask_, pi_shadow_classify_tiles_,
        pi_shadow_filter_[3], pi_shadow_debug_;

    Ren::ProgramRef blit_static_vel_prog_, blit_gauss2_prog_, blit_ao_prog_, blit_bilateral_prog_, blit_taa_prog_;

    struct CommonBuffers {
        RpResRef skin_transforms_res, shape_keys_res, instances_res, instance_indices_res, cells_res, lights_res,
            decals_res, items_res, shared_data_res, atomic_cnt_res;
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
    void AddDeferredShadingPass(const CommonBuffers &common_buffers, FrameTextures &frame_textures);
    void AddForwardOpaquePass(const CommonBuffers &common_buffers, const PersistentGpuData &persistent_data,
                              const BindlessTextureData &bindless, FrameTextures &frame_textures);
    void AddForwardTransparentPass(const CommonBuffers &common_buffers, const PersistentGpuData &persistent_data,
                                   const BindlessTextureData &bindless, FrameTextures &frame_textures);

    void AddSSAOPasses(RpResRef depth_down_2x, RpResRef depth_tex, RpResRef &out_ssao);
    void AddFillStaticVelocityPass(const CommonBuffers &common_buffers, RpResRef depth_tex, RpResRef &inout_velocity_tex);
    void AddFrameBlurPasses(const Ren::WeakTex2DRef &input_tex, RpResRef &output_tex);
    void AddTaaPass(const CommonBuffers &common_buffers, FrameTextures &frame_textures, float max_exposure,
                    RpResRef &resolved_color);

    void AddHQSpecularPasses(const Ren::WeakTex2DRef &env_map, const Ren::WeakTex2DRef &lm_direct,
                             const Ren::WeakTex2DRef lm_indir_sh[4], bool debug_denoise,
                             const Ren::ProbeStorage *probe_storage, const CommonBuffers &common_buffers,
                             const PersistentGpuData &persistent_data, const AccelerationStructureData &acc_struct_data,
                             const BindlessTextureData &bindless, RpResRef depth_hierarchy,
                             FrameTextures &frame_textures);
    void AddLQSpecularPasses(const Ren::ProbeStorage *probe_storage, const CommonBuffers &common_buffers,
                             RpResRef depth_down_2x, FrameTextures &frame_textures);

    void AddDiffusePasses(const Ren::WeakTex2DRef &env_map, const Ren::WeakTex2DRef &lm_direct,
                          const Ren::WeakTex2DRef lm_indir_sh[4], bool debug_denoise,
                          const Ren::ProbeStorage *probe_storage, const CommonBuffers &common_buffers,
                          const PersistentGpuData &persistent_data, const AccelerationStructureData &acc_struct_data,
                          const BindlessTextureData &bindless, const RpResRef depth_hierarchy,
                          FrameTextures &frame_textures);

    void AddHQSunShadowsPasses(const CommonBuffers &common_buffers, const PersistentGpuData &persistent_data,
                               const AccelerationStructureData &acc_struct_data, const BindlessTextureData &bindless,
                               FrameTextures &frame_textures, bool debug_denoise);
    void AddLQSunShadowsPasses(const CommonBuffers &common_buffers, const PersistentGpuData &persistent_data,
                               const AccelerationStructureData &acc_struct_data, const BindlessTextureData &bindless,
                               FrameTextures &frame_textures);

    void GatherDrawables(const SceneData &scene, const Ren::Camera &cam, DrawList &list);

    void InitPipelines();
    void InitRendererInternal();
    void DestroyRendererInternal();
    static uint64_t GetGpuTimeBlockingUs();

    // Parallel Jobs
    static void ClusterItemsForZSlice_Job(int slice, const Ren::Frustum *sub_frustums, const BBox *decals_boxes,
                                          const LightSource *const *litem_to_lsource, DrawList &list,
                                          std::atomic_int &items_count);

    // Generate auxiliary textures
    static std::unique_ptr<uint16_t[]> Generate_BRDF_LUT(int res, std::string &out_c_header);
    static std::unique_ptr<int8_t[]> Generate_PeriodicPerlin(int res, std::string &out_c_header);
    static std::unique_ptr<uint8_t[]> Generate_SSSProfile_LUT(int res, int gauss_count, const float gauss_variances[],
                                                              const Ren::Vec3f diffusion_weights[]);
    static std::unique_ptr<int16_t[]> Generate_RandDirs(int res, std::string &out_c_header);
    static std::unique_ptr<uint8_t[]> Generate_ConeTraceLUT(int resx, int resy, const float cone_angles[4],
                                                            std::string &out_c_header);
};
