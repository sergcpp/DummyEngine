#pragma once

#include <atomic>

#include <Ren/TextureSplitter.h>
extern "C" {
#include <Ren/SW/SWculling.h>
}

#include "../Scene/SceneData.h"
#include "FrameBuf.h"
#include "Passes/RpBlur.h"
#include "Passes/RpCombine.h"
#include "Passes/RpDOF.h"
#include "Passes/RpDebugEllipsoids.h"
#include "Passes/RpDebugProbes.h"
#include "Passes/RpDebugTextures.h"
#include "Passes/RpDepthFill.h"
#include "Passes/RpDownColor.h"
#include "Passes/RpDownDepth.h"
#include "Passes/RpFXAA.h"
#include "Passes/RpInsertFence.h"
#include "Passes/RpOpaque.h"
#include "Passes/RpReflections.h"
#include "Passes/RpResolve.h"
#include "Passes/RpSSAO.h"
#include "Passes/RpSampleBrightness.h"
#include "Passes/RpShadowMaps.h"
#include "Passes/RpSkinning.h"
#include "Passes/RpSkydome.h"
#include "Passes/RpTAA.h"
#include "Passes/RpTransparent.h"
#include "Passes/RpUpdateBuffers.h"
#include "PrimDraw.h"

#include "Renderer_DrawList.h"
#include "Renderer_GL_Defines.inl"

namespace Sys {
class ThreadPool;
}

class ShaderLoader;

class Renderer {
  public:
    Renderer(Ren::Context &ctx, ShaderLoader &sh,
             std::shared_ptr<Sys::ThreadPool> threads);
    ~Renderer();

    uint32_t render_flags() const { return render_flags_; }

    void set_render_flags(const uint32_t f) { render_flags_ = f; }

    BackendInfo backend_info() const { return backend_info_; }

    void PrepareDrawList(const SceneData &scene, const Ren::Camera &cam, DrawList &list);
    void ExecuteDrawList(const DrawList &list, const FrameBuf *target = nullptr);

    void BlitPixels(const void *data, int w, int h, Ren::eTexFormat format);
    void BlitPixelsTonemap(const void *data, int w, int h, Ren::eTexFormat format);
    void BlitBuffer(float px, float py, float sx, float sy, const FrameBuf &buf,
                    int first_att, int att_count, float multiplier = 1.0f);
    void BlitTexture(float px, float py, float sx, float sy, const Ren::Tex2DRef &tex,
                     float multiplier = 1.0f, bool is_ms = false);

    void BlitToTempProbeFace(const FrameBuf &src_buf, const ProbeStorage &dst_store,
                             int face);
    void BlitPrefilterFromTemp(const ProbeStorage &dst_store, int probe_index);
    bool BlitProjectSH(const ProbeStorage &store, int probe_index, int iteration,
                       LightProbe &probe);

  private:
    Ren::Context &ctx_;
    ShaderLoader &sh_;
    std::shared_ptr<Sys::ThreadPool> threads_;
    SWcull_ctx cull_ctx_ = {};
    Ren::ProgramRef blit_prog_, blit_ms_prog_, blit_combine_prog_, blit_down_prog_,
        blit_gauss_prog_, blit_depth_prog_, blit_rgbm_prog_, blit_mipmap_prog_,
        blit_prefilter_prog_, blit_project_sh_prog_;
    Ren::Tex2DRef dummy_black_, dummy_white_, rand2d_8x8_, rand2d_dirs_4x4_, brdf_lut_,
        cone_rt_lut_, noise_tex_;

    FrameBuf probe_sample_buf_;
    Ren::Tex2DRef history_tex_, down_tex_4x_;
    Ren::Framebuffer blur_tex_fb_[2], down_tex_4x_fb_;
    bool taa_enabled_ = false, dof_enabled_ = false;

    Ren::TextureSplitter shadow_splitter_;

    static const uint32_t default_flags =
#if !defined(__ANDROID__)
        (EnableZFill | EnableCulling | EnableSSR | EnableSSAO | EnableLightmap |
         EnableLights | EnableDecals | EnableShadows /*| EnableOIT*/ | EnableTonemap |
         EnableBloom | EnableTaa /*EnableMsaa | EnableFxaa*/ | EnableTimers | EnableDOF /*|
         DebugEllipsoids*/);
#else
        (EnableZFill | EnableCulling | EnableSSR | EnableLightmap | EnableLights |
         EnableDecals | EnableShadows | EnableTonemap | EnableDOF | EnableTimers);
#endif
    uint32_t render_flags_ = default_flags;

    int frame_counter_ = 0;

    DynArray<const LightSource *> litem_to_lsource_;
    DynArray<const Decal *> ditem_to_decal_;

    struct ProcessedObjData {
        uint32_t instance_index;
        uint32_t base_vertex;
    };
    DynArray<ProcessedObjData> proc_objects_;
    DynArray<BBox> decals_boxes_;
    BackendInfo backend_info_;
    uint64_t backend_cpu_start_ = 0, backend_cpu_end_ = 0;
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

#if defined(USE_GL_RENDER)
    Ren::Tex2DRef temp_tex_;

    uint32_t temp_framebuf_ = 0;

    // uint32_t unif_shared_data_block_[FrameSyncWindow];
    Ren::Vao temp_vao_;
    uint32_t temp_buf1_vtx_offset_, temp_buf2_vtx_offset_, temp_buf_ndx_offset_,
        skinned_buf1_vtx_offset_, skinned_buf2_vtx_offset_;
    Ren::Tex1DRef lights_tbo_[FrameSyncWindow], decals_tbo_[FrameSyncWindow];
    uint32_t /*reduce_pbo_[FrameSyncWindow], */ probe_sample_pbo_;
    // int cur_reduce_pbo_ = 0;

    enum {
        TimeDrawStart,
        TimeSkinningStart,
        TimeShadowMapStart,
        TimeDepthOpaqueStart,
        TimeAOPassStart,
        TimeOpaqueStart,
        TimeTranspStart,
        TimeReflStart,
        TimeTaaStart,
        TimeBlurStart,
        TimeBlitStart,
        TimeDrawEnd,
        TimersCount
    };
    uint32_t queries_[FrameSyncWindow][TimersCount];
    int cur_query_ = 0;

    void *buf_range_fences_[FrameSyncWindow] = {};
    int cur_buf_chunk_ = 0;
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

    RpUpdateBuffers rp_update_buffers_;
    RpSkinning rp_skinning_;
    RpShadowMaps rp_shadow_maps_ = {SHADOWMAP_WIDTH, SHADOWMAP_HEIGHT};
    RpSkydome rp_skydome_;
    RpDepthFill rp_depth_fill_;
    RpDownColor rp_down_color_ = {prim_draw_};
    RpDownDepth rp_down_depth_ = {prim_draw_};
    RpSSAO rp_ssao_ = {prim_draw_};
    RpOpaque rp_opaque_;
    RpResolve rp_resolve_ = {prim_draw_};
    RpTransparent rp_transparent_ = {prim_draw_};
    RpReflections rp_reflections_ = {prim_draw_};
    RpTAA rp_taa_ = {prim_draw_};
    RpDOF rp_dof_ = {prim_draw_};
    RpBlur rp_blur_ = {prim_draw_};
    RpInsertFence rp_fence_;
    RpSampleBrightness rp_sample_brightness_ = {prim_draw_, Ren::Vec2i{16, 8},
                                                FrameSyncWindow};
    RpCombine rp_combine_ = {prim_draw_};
    RpFXAA rp_fxaa_ = {prim_draw_};

    // debugging passes
    RpDebugEllipsoids rp_debug_ellipsoids_ = {prim_draw_};
    RpDebugProbes rp_debug_probes_ = {prim_draw_};
    RpDebugTextures rp_debug_textures_ = {prim_draw_};

    ViewState view_state_;
    PrimDraw prim_draw_;

    void GatherDrawables(const SceneData &scene, const Ren::Camera &cam, DrawList &list);

    void InitRendererInternal();
    void DestroyRendererInternal();
    static uint64_t GetGpuTimeBlockingUs();

    // Parallel Jobs
    static void GatherItemsForZSlice_Job(int slice, const Ren::Frustum *sub_frustums,
                                         const BBox *decals_boxes,
                                         const LightSource *const *litem_to_lsource,
                                         DrawList &list, std::atomic_int &items_count);

    // Generate auxiliary textures
    static std::unique_ptr<uint16_t[]> Generate_BRDF_LUT(int res,
                                                         std::string &out_c_header);
    static std::unique_ptr<int8_t[]> Generate_PeriodicPerlin(int res,
                                                             std::string &out_c_header);
    static std::unique_ptr<uint8_t[]>
    Generate_SSSProfile_LUT(int res, int gauss_count, const float gauss_variances[],
                            const Ren::Vec3f diffusion_weights[]);
    static std::unique_ptr<int16_t[]> Generate_RandDirs(int res,
                                                        std::string &out_c_header);
    static std::unique_ptr<uint8_t[]> Generate_ConeTraceLUT(int resx, int resy,
                                                            const float cone_angles[4],
                                                            std::string &out_c_header);
};
