#include "Renderer.h"

#include "../Utils/ShaderLoader.h"
#include <Ren/Camera.h>
#include <Ren/Context.h>
#include <Ren/GL.h>

namespace PrimDrawInternal {
extern const float fs_quad_positions[];
extern const float fs_quad_norm_uvs[];
extern const uint16_t fs_quad_indices[];
} // namespace PrimDrawInternal

namespace RendererInternal {
const int U_MVP_MATR = 0;

const int U_GAMMA = 14;
const int U_EXPOSURE = 15;
const int U_FADE = 16;

const int U_RES = 15;
} // namespace RendererInternal

void Renderer::InitRendererInternal() {
    using namespace RendererInternal;
    using namespace Ren;

    blit_prog_ = sh_.LoadProgram(ctx_, "blit", "internal/blit.vert.glsl", "internal/blit.frag.glsl");
    assert(blit_prog_->ready());
    blit_combine_prog_ =
        sh_.LoadProgram(ctx_, "blit_combine", "internal/blit.vert.glsl", "internal/blit_combine.frag.glsl");
    assert(blit_combine_prog_->ready());
    blit_ms_prog_ = sh_.LoadProgram(ctx_, "blit_ms", "internal/blit.vert.glsl", "internal/blit_ms.frag.glsl");
    assert(blit_ms_prog_->ready());
    blit_down_prog_ = sh_.LoadProgram(ctx_, "blit_down", "internal/blit.vert.glsl", "internal/blit_down.frag.glsl");
    assert(blit_down_prog_->ready());
    blit_gauss_prog_ = sh_.LoadProgram(ctx_, "blit_gauss", "internal/blit.vert.glsl", "internal/blit_gauss.frag.glsl");
    assert(blit_gauss_prog_->ready());
    blit_depth_prog_ = sh_.LoadProgram(ctx_, "blit_depth", "internal/blit.vert.glsl", "internal/blit_depth.frag.glsl");
    assert(blit_depth_prog_->ready());
    blit_rgbm_prog_ = sh_.LoadProgram(ctx_, "blit_rgbm", "internal/blit.vert.glsl", "internal/blit_rgbm.frag.glsl");
    assert(blit_rgbm_prog_->ready());
    blit_mipmap_prog_ =
        sh_.LoadProgram(ctx_, "blit_mipmap", "internal/blit.vert.glsl", "internal/blit_mipmap.frag.glsl");
    assert(blit_mipmap_prog_->ready());
    blit_prefilter_prog_ =
        sh_.LoadProgram(ctx_, "blit_prefilter", "internal/blit.vert.glsl", "internal/blit_prefilter.frag.glsl");
    assert(blit_prefilter_prog_->ready());
    blit_project_sh_prog_ =
        sh_.LoadProgram(ctx_, "blit_project_sh_prog", "internal/blit.vert.glsl", "internal/blit_project_sh.frag.glsl");
    assert(blit_project_sh_prog_->ready());

    Ren::CheckError("[InitRendererInternal]: UBO creation", ctx_.log());

    {
        GLuint probe_sample_pbo;
        glGenBuffers(1, &probe_sample_pbo);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, probe_sample_pbo);
        glBufferData(GL_PIXEL_PACK_BUFFER, GLsizeiptr(4 * probe_sample_buf_.w * probe_sample_buf_.h * sizeof(float)),
                     nullptr, GL_DYNAMIC_READ);

        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        probe_sample_pbo_ = uint32_t(probe_sample_pbo);
    }

    Ren::CheckError("[InitRendererInternal]: probe sample PBO", ctx_.log());

    {
        GLuint temp_framebuf;
        glGenFramebuffers(1, &temp_framebuf);

        temp_framebuf_ = uint32_t(temp_framebuf);
    }

    Ren::CheckError("[InitRendererInternal]: temp framebuffer", ctx_.log());

    {                                                      // Create timer queries
        for (int i = 0; i < Ren::MaxFramesInFlight; i++) { // NOLINT
            glGenQueries(TimersCount, queries_[i]);

            for (int j = 0; j < TimersCount; j++) {
                glQueryCounter(queries_[i][j], GL_TIMESTAMP);
            }
        }
    }

    Ren::CheckError("[InitRendererInternal]: timer queries", ctx_.log());
}

void Renderer::DestroyRendererInternal() {
    Ren::ILog *log = ctx_.log();

    log->Info("DestroyRendererInternal");

    Ren::BufferRef vtx_buf1 = ctx_.default_vertex_buf1(), vtx_buf2 = ctx_.default_vertex_buf2(),
                   ndx_buf = ctx_.default_indices_buf();

    static_assert(sizeof(GLuint) == sizeof(uint32_t), "!");

    {
        auto temp_framebuf = GLuint(temp_framebuf_);
        glDeleteFramebuffers(1, &temp_framebuf);
    }

    {
        auto probe_sample_pbo = GLuint(probe_sample_pbo_);
        glDeleteBuffers(1, &probe_sample_pbo);
    }

    {
        assert(vtx_buf1->FreeSubRegion(temp_buf1_vtx_offset_));
        assert(vtx_buf2->FreeSubRegion(temp_buf2_vtx_offset_));
        assert(ndx_buf->FreeSubRegion(temp_buf_ndx_offset_));
    }

    for (int i = 0; i < Ren::MaxFramesInFlight; i++) {
        static_assert(sizeof(queries_[0][0]) == sizeof(GLuint), "!");
        glDeleteQueries(TimersCount, queries_[i]);
    }
}

#if 0
void Renderer::DrawObjectsInternal(const DrawList &list, const FrameBuf *target) {
    using namespace Ren;
    using namespace RendererInternal;

    Ren::ILog *log = ctx_.log();

    if (list.render_flags & EnableTimers) {
        glQueryCounter(queries_[cur_query_][TimeDrawStart], GL_TIMESTAMP);
    }

    CheckInitVAOs();

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);

    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);

    backend_info_.shadow_draw_calls_count = 0;
    backend_info_.depth_fill_draw_calls_count = 0;
    backend_info_.opaque_draw_calls_count = 0;

    backend_info_.tris_rendered = 0;

    const Ren::Mat4f clip_from_world_unjittered =
        list.draw_cam.proj_matrix() * list.draw_cam.view_matrix();

    //
    // Update UBO with data that is shared between passes
    //

    // TODO: REMOVE THIS!
    unif_shared_data_buf_[cur_buf_chunk_].read_count = 7;
    unif_shared_data_buf_[cur_buf_chunk_].write_count = 1;
    if ((list.render_flags & DebugWireframe) == 0 && list.env.env_map) {
        unif_shared_data_buf_[cur_buf_chunk_].read_count++;
    }
    if (list.render_flags & DebugProbes) {
        unif_shared_data_buf_[cur_buf_chunk_].read_count++;
    }
    if (list.render_flags & DebugEllipsoids) {
        unif_shared_data_buf_[cur_buf_chunk_].read_count++;
    }
    if (list.render_flags & EnableTaa) {
        unif_shared_data_buf_[cur_buf_chunk_].read_count++;
    }
    if ((list.render_flags & (EnableSSR | EnableBloom | EnableTonemap | EnableDOF)) &&
        ((list.render_flags & DebugWireframe) == 0)) {
        unif_shared_data_buf_[cur_buf_chunk_].read_count++;
    }
    const bool apply_dof =
        (list.render_flags & EnableDOF) && list.draw_cam.focus_near_mul > 0.0f &&
        list.draw_cam.focus_far_mul > 0.0f && ((list.render_flags & DebugWireframe) == 0);

    if (apply_dof) {
        unif_shared_data_buf_[cur_buf_chunk_].read_count++;
    }

    if ((list.render_flags & (EnableSSR | EnableBloom | EnableTonemap)) &&
        ((list.render_flags & DebugWireframe) == 0)) {
        unif_shared_data_buf_[cur_buf_chunk_].read_count++;
    }

    if ((list.render_flags & EnableFxaa) && !(list.render_flags & DebugWireframe)) {
        unif_shared_data_buf_[cur_buf_chunk_].read_count++;
    }

    Graph::RpAllocBuf &unif_shared_data_buf =
        rp_builder_.GetReadBuffer(unif_shared_data_buf_[cur_buf_chunk_]);
    glBindBufferBase(GL_UNIFORM_BUFFER, REN_UB_SHARED_DATA_LOC,
                     GLuint(unif_shared_data_buf.ref->id()));

    //
    // Update vertex buffer for skinned meshes
    //

    if (list.render_flags & EnableTimers) {
        glQueryCounter(queries_[cur_query_][TimeSkinningStart], GL_TIMESTAMP);
    }

    //
    // Update shadow maps
    //

    if (list.render_flags & EnableTimers) {
        glQueryCounter(queries_[cur_query_][TimeShadowMapStart], GL_TIMESTAMP);
    }

    ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, REN_INST_BUF_SLOT,
                               GLuint(instances_tbo_[cur_buf_chunk_]->id()));

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glDisable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Setup viewport
    glViewport(0, 0, view_state_.act_res[0], view_state_.act_res[1]);

    // Can draw skydome without multisampling (not sure if it helps)
    glDisable(GL_MULTISAMPLE);

    //
    // Skydome drawing + implicit depth/specular clear
    //

    glEnable(GL_MULTISAMPLE);

    //
    // Bind persistent resources (shadow atlas, lightmap, cells item data)
    //

    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_SHAD_TEX_SLOT, shadow_tex_->id());

    if (list.decals_atlas) {
        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_DECAL_TEX_SLOT,
                                   list.decals_atlas->tex_id(0));
    }

    if ((list.render_flags & (EnableZFill | EnableSSAO)) == (EnableZFill | EnableSSAO)) {
        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_SSAO_TEX_SLOT, combined_tex_->id());
    } else {
        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_SSAO_TEX_SLOT, dummy_white_->id());
    }

    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_BRDF_TEX_SLOT, brdf_lut_->id());

    if ((list.render_flags & EnableLightmap) && list.env.lm_direct) {
        for (int sh_l = 0; sh_l < 4; sh_l++) {
            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_LMAP_SH_SLOT + sh_l,
                                       list.env.lm_indir_sh[sh_l]->id());
        }
    } else {
        for (int sh_l = 0; sh_l < 4; sh_l++) {
            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_LMAP_SH_SLOT + sh_l,
                                       dummy_black_->id());
        }
    }

    if (list.probe_storage) {
        ren_glBindTextureUnit_Comp(GL_TEXTURE_CUBE_MAP_ARRAY, REN_ENV_TEX_SLOT,
                                   list.probe_storage->tex_id());
    }

    ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, REN_LIGHT_BUF_SLOT,
                               lights_tbo_[cur_buf_chunk_]->id());
    ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, REN_DECAL_BUF_SLOT,
                               decals_tbo_[cur_buf_chunk_]->id());
    ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, REN_CELLS_BUF_SLOT,
                               cells_tbo_[cur_buf_chunk_]->id());
    ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, REN_ITEMS_BUF_SLOT,
                               items_tbo_[cur_buf_chunk_]->id());

    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_NOISE_TEX_SLOT, noise_tex_->id());
    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_CONE_RT_LUT_SLOT, cone_rt_lut_->id());

    //
    // Depth-fill pass (draw opaque surfaces -> draw alpha-tested surfaces)
    //

    if (list.render_flags & EnableTimers) {
        glQueryCounter(queries_[cur_query_][TimeDepthOpaqueStart], GL_TIMESTAMP);
    }

    glDisable(GL_STENCIL_TEST);

    glBindVertexArray(0);

    glDepthFunc(GL_EQUAL);

    //
    // SSAO pass (downsample depth -> calc line integrals ao -> upscale)
    //

    if (list.render_flags & EnableTimers) {
        glQueryCounter(queries_[cur_query_][TimeAOPassStart], GL_TIMESTAMP);
    }

#if !defined(__ANDROID__)
    if (list.render_flags & DebugWireframe) {
        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_FALSE);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    } else {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
#endif

    //
    // Opaque pass (draw opaque surfaces -> resolve multisampled color buffer if enabled)
    //

#if !defined(REN_DIRECT_DRAWING)
#if !defined(__ANDROID__)
    if (list.render_flags & DebugWireframe) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
#endif

    /*if ((list.render_flags & EnableOIT) && clean_buf_.sample_count > 1) {
        DebugMarker _("RESOLVE MS BUFFER");

        const PrimDraw::Binding bindings[] = {{Ren::eBindTarget::Tex2DMs,
                                               REN_BASE0_TEX_SLOT,
                                               clean_buf_.attachments[0].tex->handle()}};

        const PrimDraw::Uniform uniforms[] = {
            {0, Ren::Vec4f{0.0f, 0.0f, float(view_state_.act_res[0]),
                           float(view_state_.act_res[1])}}};

        prim_draw_.DrawPrim(
            PrimDraw::ePrim::Quad,
            {resolved_or_transparent_fb_, 0,
             Ren::Vec4i{0, 0, view_state_.act_res[0], view_state_.act_res[1]}},
            blit_ms_resolve_prog_.get(), bindings, 1, uniforms, 1);
    }*/
#endif

    //
    // Transparent pass
    //

    if (list.render_flags & EnableTimers) {
        glQueryCounter(queries_[cur_query_][TimeTranspStart], GL_TIMESTAMP);
    }

    // glBindVertexArray(draw_pass_vao_.id());

#if !defined(__ANDROID__)
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
#endif

    glDisable(GL_BLEND);
    glDepthFunc(GL_LESS);

    //
    // Reflections pass (calc ssr buffer -> dilate -> combine with cubemap reflections ->
    // blend on top of color buffer)
    //

    if (list.render_flags & EnableTimers) {
        glQueryCounter(queries_[cur_query_][TimeReflStart], GL_TIMESTAMP);
    }

    glBindVertexArray(prim_draw_.fs_quad_vao());

    if (list.render_flags & EnableTimers) {
        glQueryCounter(queries_[cur_query_][TimeTaaStart], GL_TIMESTAMP);
    }

    ren_glBindTextureUnit_Comp(GL_TEXTURE_CUBE_MAP_ARRAY, REN_ENV_TEX_SLOT, 0);

    glDisable(GL_DEPTH_TEST);

    //
    // Blur pass (apply gauss blur to color buffer -> blend on top)
    //

    if (list.render_flags & EnableTimers) {
        glQueryCounter(queries_[cur_query_][TimeBlurStart], GL_TIMESTAMP);
    }

    glBindVertexArray(prim_draw_.fs_quad_vao());
    glDepthMask(GL_FALSE);
    glDepthFunc(GL_LESS);

    // store matrix to use it in next frame
    view_state_.down_buf_view_from_world = list.draw_cam.view_matrix();
    view_state_.prev_clip_from_world = clip_from_world_unjittered;
    view_state_.prev_clip_from_view = list.draw_cam.proj_matrix_offset();

    const float reduced_average = rp_sample_brightness_.reduced_average();

    float exposure = reduced_average > std::numeric_limits<float>::epsilon()
                         ? (1.0f / reduced_average)
                         : 1.0f;
    exposure = std::min(exposure, list.draw_cam.max_exposure);

    //
    // Blit pass (tonemap buffer / apply fxaa / blit to backbuffer)
    //

    if (list.render_flags & EnableTimers) {
        glQueryCounter(queries_[cur_query_][TimeBlitStart], GL_TIMESTAMP);
    }

    Ren::Vec4i blit_viewport;
    uint32_t blit_fb = 0xffffffff;

    if ((list.render_flags & EnableFxaa) && !(list.render_flags & DebugWireframe)) {
        // blit_fb = combined_buf_.fb;
        blit_viewport = Ren::Vec4i(0, 0, view_state_.act_res[0], view_state_.act_res[1]);
    } else {
        if (!target) {
            blit_fb = 0;
            blit_viewport =
                Ren::Vec4i(0, 0, view_state_.scr_res[0], view_state_.scr_res[1]);
        } else {
            blit_fb = target->fb;
            blit_viewport = Ren::Vec4i(0, 0, target->w, target->h);
        }
    }

#if !defined(REN_DIRECT_DRAWING)
    /*{ // Blit main framebuffer
        Ren::Program *blit_prog = blit_combine_prog_.get();

        float gamma = 1.0f;
        if ((list.render_flags & EnableTonemap) && !(list.render_flags & DebugLights)) {
            gamma = 2.2f;
        }

        const PrimDraw::Uniform uniforms[] = {
            {0, Ren::Vec4f{0.0f, 0.0f, float(view_state_.act_res[0]),
                           float(view_state_.act_res[1])}},
            {12, (list.render_flags & EnableTonemap) ? 1.0f : 0.0f},
            {13,
             Ren::Vec2f{float(view_state_.act_res[0]), float(view_state_.act_res[1])}},
            {U_GAMMA, gamma},
            {U_EXPOSURE, (list.render_flags & EnableTonemap) ? exposure : 1.0f},
            {U_FADE, list.draw_cam.fade}};

        PrimDraw::Binding bindings[2];

        if (clean_buf_.sample_count > 1 || ((list.render_flags & EnableTaa) != 0) ||
            apply_dof) {
            if (apply_dof) {
                if ((list.render_flags & EnableTaa) != 0) {
                    bindings[0] = {Ren::eBindTarget::Tex2D, REN_BASE0_TEX_SLOT,
                                   clean_buf_.attachments[0].tex->handle()};
                } else {
                    bindings[0] = {Ren::eBindTarget::Tex2D, REN_BASE0_TEX_SLOT,
                                   dof_tex_->handle()};
                }
            } else {
                bindings[0] = {Ren::eBindTarget::Tex2D, REN_BASE0_TEX_SLOT,
                               resolved_or_transparent_tex_->handle()};
            }
        } else {
            bindings[0] = {Ren::eBindTarget::Tex2D, REN_BASE0_TEX_SLOT,
                           clean_buf_.attachments[REN_OUT_COLOR_INDEX].tex->handle()};
        }

        if ((list.render_flags & EnableBloom) && !(list.render_flags & DebugWireframe)) {
            bindings[1] = {Ren::eBindTarget::Tex2D, REN_BASE1_TEX_SLOT,
                           blur_tex_[0]->handle()};
        } else {
            bindings[1] = {Ren::eBindTarget::Tex2D, REN_BASE1_TEX_SLOT,
                           dummy_black_->handle()};
        }

#ifndef DISABLE_MARKERS
        glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "FINAL BLIT");
#endif

        prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, {blit_fb, 0, blit_viewport}, blit_prog,
                            bindings, 2, uniforms, 6);
    }*/

    /*if ((list.render_flags & EnableFxaa) && !(list.render_flags & DebugWireframe)) {
        Ren::Vec4i viewport;
        uint32_t fb = 0xffffffff;
        if (!target) {
            fb = 0;
            viewport = Ren::Vec4i(0, 0, view_state_.scr_res[0], view_state_.scr_res[1]);
        } else {
            fb = target->fb;
            viewport = Ren::Vec4i(0, 0, target->w, target->h);
        }

        { // Blit fxaa
            Ren::Program *blit_prog = blit_fxaa_prog_.get();

            const PrimDraw::Binding bindings[] = {
                {Ren::eBindTarget::Tex2D, REN_BASE0_TEX_SLOT,
                 combined_buf_.attachments[0].tex->handle()},
                {Ren::eBindTarget::UBuf, REN_UB_SHARED_DATA_LOC,
                 unif_shared_data_buf.ref->handle()}};

            const PrimDraw::Uniform uniforms[] = {
                {0, Ren::Vec4f{0.0f, 0.0f, 1.0f, 1.0f}},
                {12, Ren::Vec2f{1.0f / float(view_state_.scr_res[0]),
                                1.0f / float(view_state_.scr_res[1])}}};

            prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, {fb, 0, viewport}, blit_prog,
                                bindings, 2, uniforms, 2);
        }
    }*/

#ifndef DISABLE_MARKERS
    // glPopDebugGroup();
#endif
#endif // !defined(REN_DIRECT_DRAWING)

    //
    // Debugging (draw auxiliary surfaces)
    //

    /*if (list.render_flags & (DebugLights | DebugDecals)) {
        Ren::RastState rast_state;
        rast_state.cull_face.enabled = true;

        rast_state.viewport = blit_viewport;

        rast_state.blend = { true, eBlendFactor::SrcAlpha, eBlendFactor::OneMinusSrcAlpha
    };

        rast_state.Apply();

        ////

        Ren::Program *blit_prog = nullptr;
        if (clean_buf_.sample_count > 1) {
            blit_prog = blit_debug_ms_prog_.get();
        } else {
            blit_prog = blit_debug_prog_.get();
        }

        PrimDraw::Uniform uniforms[] = {
            {0, Ren::Vec4f{0.0f, 0.0f, float(view_state_.act_res[0]),
                           float(view_state_.act_res[1])}},
            {U_RES, Ren::Vec2i{view_state_.scr_res[0], view_state_.scr_res[1]}},
            {16, 0.0f},
            {17, list.draw_cam.clip_info()}};

        if (list.render_flags & DebugLights) {
            uniforms[2].fdata[0] = 0.5f;
        } else if (list.render_flags & DebugDecals) {
            uniforms[2].fdata[0] = 1.5f;
        }

        PrimDraw::Binding bindings[3];

        if (clean_buf_.sample_count > 1) {
            bindings[0] = {Ren::eBindTarget::Tex2DMs, REN_BASE0_TEX_SLOT,
                           clean_buf_.depth_tex->handle()};
        } else {
            bindings[0] = {Ren::eBindTarget::Tex2D, REN_BASE0_TEX_SLOT,
                           clean_buf_.depth_tex->handle()};
        }

        bindings[1] = {Ren::eBindTarget::TexBuf, REN_CELLS_BUF_SLOT,
                       cells_tbo_[cur_buf_chunk_]->handle()};
        bindings[2] = {Ren::eBindTarget::TexBuf, REN_ITEMS_BUF_SLOT,
                       items_tbo_[cur_buf_chunk_]->handle()};

        prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, {blit_fb, 0}, blit_prog, bindings, 3,
                            uniforms, 4);

        glDisable(GL_BLEND);
    }*/

    /*if (((list.render_flags & (EnableCulling | DebugCulling)) ==
         (EnableCulling | DebugCulling)) &&
        !list.depth_pixels.empty()) {

        Ren::RastState rast_state;
        rast_state.cull_face.enabled = true;
        rast_state.viewport = Ren::Vec4i{0, 0, 512, 256};
        rast_state.Apply();

        ////

        Ren::Tex2DParams params;
        params.w = 256;
        params.h = 128;
        params.format = Ren::eTexFormat::RawRGBA8888;
        params.filter = Ren::eTexFilter::NoFilter;
        params.repeat = Ren::eTexRepeat::ClampToEdge;

        if (!temp_tex_ || temp_tex_->params() != params) {
            Ren::eTexLoadStatus status;
            temp_tex_ = ctx_.LoadTexture2D("__TEMP_BLIT_TEXTURE__", params, &status);
            assert(status == Ren::eTexLoadStatus::TexCreatedDefault);
        }

        ////

        const PrimDraw::Binding bindings[] = {
            {Ren::eBindTarget::Tex2D, REN_BASE0_TEX_SLOT, temp_tex_->handle()}
        };

        const PrimDraw::Uniform uniforms[] = {{0, Ren::Vec4f{0.0f, 0.0f, 256.0f, 128.0f}},
                                              {4, 1.0f}};

        temp_tex_->SetSubImage(0, 0, 0, 256, 128, Ren::eTexFormat::RawRGBA8888,
                               &list.depth_pixels[0]);

        prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, {blit_fb, 0}, blit_prog_.get(),
                            bindings, 1, uniforms, 2);

        /////

        rast_state.viewport = Ren::Vec4i{512, 0, 512, 256};
        rast_state.Apply();

        temp_tex_->SetSubImage(0, 0, 0, 256, 128, Ren::eTexFormat::RawRGBA8888,
                               &list.depth_tiles[0]);

        prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, {blit_fb, 0}, blit_prog_.get(),
                            bindings, 1, uniforms, 2);
    }*/

    /*if (list.render_flags & DebugShadow) {
        glBindVertexArray(temp_vao_.id());

        glUseProgram(blit_depth_prog_->id());

        glUniform4f(0, 0.0f, 0.0f, 1.0f, 1.0f);

        glBindBuffer(GL_ARRAY_BUFFER, last_vertex_buf1_);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, last_index_buffer_);

        glEnableVertexAttribArray(REN_VTX_POS_LOC);
        glVertexAttribPointer(REN_VTX_POS_LOC, 2, GL_FLOAT, GL_FALSE, 0,
                              (const GLvoid *)uintptr_t(temp_buf1_vtx_offset_));

        glEnableVertexAttribArray(REN_VTX_UV1_LOC);
        glVertexAttribPointer(
            REN_VTX_UV1_LOC, 2, GL_FLOAT, GL_FALSE, 0,
            (const GLvoid *)uintptr_t(temp_buf1_vtx_offset_ + 8 * sizeof(float)));

        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, GLintptr(temp_buf_ndx_offset_),
                        6 * sizeof(uint16_t), PrimDrawInternal::fs_quad_indices);

        ren_glTextureParameteri_Comp(GL_TEXTURE_2D, shadow_tex_->id(),
                                     GL_TEXTURE_COMPARE_MODE, GL_NONE);

        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_BASE0_TEX_SLOT, shadow_tex_->id());

        const float k =
            (float(shadow_tex_->params().h) / float(shadow_tex_->params().w)) *
            (float(view_state_.scr_res[0]) / float(view_state_.scr_res[1]));

        { // Clear region
            glEnable(GL_SCISSOR_TEST);

            glScissor(0, 0, view_state_.scr_res[0] / 2,
                      int(k * float(view_state_.scr_res[1]) / 2));
            glClear(GL_COLOR_BUFFER_BIT);

            glDisable(GL_SCISSOR_TEST);
        }

        // Draw visible shadow regions
        for (int i = 0; i < int(list.shadow_lists.count); i++) {
            const ShadowList &sh_list = list.shadow_lists.data[i];
            const ShadowMapRegion &sh_reg = list.shadow_regions.data[i];

            const float positions[] = {
                -1.0f + sh_reg.transform[0],
                -1.0f + sh_reg.transform[1] * k,
                -1.0f + sh_reg.transform[0] + sh_reg.transform[2],
                -1.0f + sh_reg.transform[1] * k,
                -1.0f + sh_reg.transform[0] + sh_reg.transform[2],
                -1.0f + (sh_reg.transform[1] + sh_reg.transform[3]) * k,
                -1.0f + sh_reg.transform[0],
                -1.0f + (sh_reg.transform[1] + sh_reg.transform[3]) * k};

            const float uvs[] = {
                float(sh_list.shadow_map_pos[0]),
                float(sh_list.shadow_map_pos[1]),
                float(sh_list.shadow_map_pos[0] + sh_list.shadow_map_size[0]),
                float(sh_list.shadow_map_pos[1]),
                float(sh_list.shadow_map_pos[0] + sh_list.shadow_map_size[0]),
                float(sh_list.shadow_map_pos[1] + sh_list.shadow_map_size[1]),
                float(sh_list.shadow_map_pos[0]),
                float(sh_list.shadow_map_pos[1] + sh_list.shadow_map_size[1])};

            glBufferSubData(GL_ARRAY_BUFFER, GLintptr(temp_buf1_vtx_offset_),
                            sizeof(positions), positions);
            glBufferSubData(GL_ARRAY_BUFFER,
                            GLintptr(temp_buf1_vtx_offset_ + sizeof(positions)),
                            sizeof(uvs), uvs);

            glUniform1f(1, sh_list.cam_near);
            glUniform1f(2, sh_list.cam_far);

            if (sh_list.shadow_batch_count) {
                // mark updated region with red
                glUniform3f(3, 1.0f, 0.5f, 0.5f);
            } else {
                // mark cached region with green
                glUniform3f(3, 0.5f, 1.0f, 0.5f);
            }

            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT,
                           (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));
        }

        // Draw invisible cached shadow regions
        for (int i = 0; i < int(list.cached_shadow_regions.count); i++) {
            const ShadReg &sh_reg = list.cached_shadow_regions.data[i];

            const float positions[] = {
                -1.0f + float(sh_reg.pos[0]) / SHADOWMAP_WIDTH,
                -1.0f + k * float(sh_reg.pos[1]) / SHADOWMAP_HEIGHT,
                -1.0f + float(sh_reg.pos[0] + sh_reg.size[0]) / SHADOWMAP_WIDTH,
                -1.0f + k * float(sh_reg.pos[1]) / SHADOWMAP_HEIGHT,
                -1.0f + float(sh_reg.pos[0] + sh_reg.size[0]) / SHADOWMAP_WIDTH,
                -1.0f + k * float(sh_reg.pos[1] + sh_reg.size[1]) / SHADOWMAP_HEIGHT,
                -1.0f + float(sh_reg.pos[0]) / SHADOWMAP_WIDTH,
                -1.0f + k * float(sh_reg.pos[1] + sh_reg.size[1]) / SHADOWMAP_HEIGHT};

            const float uvs[] = {float(sh_reg.pos[0]),
                                 float(sh_reg.pos[1]),
                                 float(sh_reg.pos[0] + sh_reg.size[0]),
                                 float(sh_reg.pos[1]),
                                 float(sh_reg.pos[0] + sh_reg.size[0]),
                                 float(sh_reg.pos[1] + sh_reg.size[1]),
                                 float(sh_reg.pos[0]),
                                 float(sh_reg.pos[1] + sh_reg.size[1])};

            glBufferSubData(GL_ARRAY_BUFFER, GLintptr(temp_buf1_vtx_offset_),
                            sizeof(positions), positions);
            glBufferSubData(GL_ARRAY_BUFFER,
                            GLintptr(temp_buf1_vtx_offset_ + sizeof(positions)),
                            sizeof(uvs), uvs);

            glUniform1f(1, sh_reg.cam_near);
            glUniform1f(2, sh_reg.cam_far);

            // mark cached region with blue
            glUniform3f(3, 0.5f, 0.5f, 1.0f);

            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT,
                           (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));
        }

        // Draw view frustum edges
        for (int i = 0; i < int(list.shadow_lists.count); i++) {
            const ShadowList &sh_list = list.shadow_lists.data[i];
            const ShadowMapRegion &sh_reg = list.shadow_regions.data[i];

            if (!sh_list.view_frustum_outline_count) {
                continue;
            }

            for (int j = 0; j < sh_list.view_frustum_outline_count; j += 2) {
                const Ren::Vec2f &p1 = sh_list.view_frustum_outline[j],
                                 &p2 = sh_list.view_frustum_outline[j + 1];

                const float positions[] = {
                    -1.0f + sh_reg.transform[0] +
                        (p1[0] * 0.5f + 0.5f) * sh_reg.transform[2],
                    -1.0f + (sh_reg.transform[1] +
                             (p1[1] * 0.5f + 0.5f) * sh_reg.transform[3]) *
                                k,
                    -1.0f + sh_reg.transform[0] +
                        (p2[0] * 0.5f + 0.5f) * sh_reg.transform[2],
                    -1.0f + (sh_reg.transform[1] +
                             (p2[1] * 0.5f + 0.5f) * sh_reg.transform[3]) *
                                k,
                };

                glBufferSubData(GL_ARRAY_BUFFER, GLintptr(temp_buf1_vtx_offset_),
                                sizeof(positions), positions);

                // draw line with black color
                glUniform3f(3, 0.0f, 0.0f, 0.0f);

                glDrawElements(GL_LINES, 2, GL_UNSIGNED_SHORT,
                               (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));
            }
        }

        // Restore compare mode
        ren_glTextureParameteri_Comp(GL_TEXTURE_2D, shadow_tex_->id(),
                                     GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);

        glDisableVertexAttribArray(REN_VTX_POS_LOC);
        glDisableVertexAttribArray(REN_VTX_UV1_LOC);
    }*/

    glBindVertexArray(temp_vao_.id());

    /*if (list.render_flags & DebugReduce) {
        Ren::RastState rast_state;
        rast_state.cull_face.enabled = true;
        rast_state.viewport = Ren::Vec4i{0, 0, 512, 256};
        rast_state.Apply();

        const PrimDraw::Binding bindings[] = {
            {Ren::eBindTarget::Tex2D, REN_BASE0_TEX_SLOT, reduced_tex_->handle()}};

        const auto &p = reduced_tex_->params();

        const PrimDraw::Uniform uniforms[] = {
            {0, Ren::Vec4f{0.0f, 0.0f, float(p.w), float(p.h)}}, {4, 10.0f}};

        prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, {blit_fb, 0}, blit_prog_.get(),
                            bindings, 1, uniforms, 2);
    }*/

    /*if (list.render_flags & DebugDeferred) {
        BlitBuffer(-1.0f, -1.0f, 0.5f, 0.5f, clean_buf_, 1, 2);
        BlitTexture(0.0f, -1.0f, 0.5f, 0.5f, down_tex_4x_, exposure);
    }*/

    /*if (list.render_flags & DebugBlur) {
        BlitTexture(-1.0f, -1.0f, 1.0f, 1.0f, blur_tex_[0], 400.0f);
    }*/

    /*if (list.render_flags & DebugSSAO) {
        const Ren::Tex2DParams &p = ssao_tex1_->params();
        BlitTexture(-1.0f, -1.0f, 1.0f, 1.0f, ssao_tex1_);
    }*/

    if ((list.render_flags & DebugDecals) && list.decals_atlas) {
        const int resx = list.decals_atlas->resx(), resy = list.decals_atlas->resy();

        float k = float(view_state_.scr_res[0]) / float(view_state_.scr_res[1]);
        k *= float(resy) / float(resx);

        // TODO: fix this
        // BlitTexture(-1.0f, -1.0f, 1.0f, 1.0f * k, list.decals_atlas->tex_id(0), resx,
        //            resy);
    }

    /*if (list.render_flags & DebugBVH) {
        const uint32_t buf_size = uint32_t(list.temp_nodes.size() * sizeof(bvh_node_t));

        if (!nodes_buf_ || buf_size > nodes_buf_->size()) {
            nodes_buf_ = ctx_.CreateBuffer("Nodes buf", Ren::eBufType::Texture,
                                           Ren::eBufAccessType::Draw,
                                           Ren::eBufAccessFreq::Dynamic, buf_size);
            const uint32_t off = nodes_buf_->Alloc(buf_size, list.temp_nodes.data());
            assert(off == 0);

            nodes_tbo_ = ctx_.CreateTexture1D("Nodes TBO", nodes_buf_,
                                              Ren::eTexFormat::RawRGBA32F, 0, buf_size);
        } else {
            const bool res = nodes_buf_->Free(0);
            assert(res);

            const uint32_t off = nodes_buf_->Alloc(buf_size, list.temp_nodes.data());
            assert(off == 0);
        }

        {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            const Ren::Program *debug_bvh_prog = nullptr;
            if (clean_buf_.sample_count > 1) {
                debug_bvh_prog = blit_debug_bvh_ms_prog_.get();
            } else {
                debug_bvh_prog = blit_debug_bvh_prog_.get();
            }
            glUseProgram(debug_bvh_prog->id());

            glUniform4f(0, 0.0f, 0.0f, 1.0f, 1.0f);

            const float uvs[] = {0.0f,
                                 0.0f,
                                 float(view_state_.scr_res[0]),
                                 0.0f,
                                 float(view_state_.scr_res[0]),
                                 float(view_state_.scr_res[1]),
                                 0.0f,
                                 float(view_state_.scr_res[1])};

            glBindBuffer(GL_ARRAY_BUFFER, last_vertex_buf1_);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, last_index_buffer_);

            glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)temp_buf1_vtx_offset_,
                            8 * sizeof(float), PrimDrawInternal::fs_quad_positions);
            glBufferSubData(GL_ARRAY_BUFFER,
                            (GLintptr)(temp_buf1_vtx_offset_ + 8 * sizeof(float)),
                            sizeof(uvs), uvs);
            glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, (GLintptr)temp_buf_ndx_offset_,
                            6 * sizeof(uint16_t), PrimDrawInternal::fs_quad_indices);

            glEnableVertexAttribArray(REN_VTX_POS_LOC);
            glVertexAttribPointer(REN_VTX_POS_LOC, 2, GL_FLOAT, GL_FALSE, 0,
                                  (const GLvoid *)uintptr_t(temp_buf1_vtx_offset_));

            glEnableVertexAttribArray(REN_VTX_UV1_LOC);
            glVertexAttribPointer(
                REN_VTX_UV1_LOC, 2, GL_FLOAT, GL_FALSE, 0,
                (const GLvoid *)uintptr_t(temp_buf1_vtx_offset_ + 8 * sizeof(float)));

            if (clean_buf_.sample_count > 1) {
                ren_glBindTextureUnit_Comp(GL_TEXTURE_2D_MULTISAMPLE, 0,
                                           clean_buf_.depth_tex->id());
            } else {
                ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, 0, clean_buf_.depth_tex->id());
            }

            ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, 1, nodes_tbo_->id());

            glUniform1i(debug_bvh_prog->uniform("uRootIndex").loc, list.root_index);

            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT,
                           (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));

            glDisableVertexAttribArray(REN_VTX_POS_LOC);
            glDisableVertexAttribArray(REN_VTX_UV1_LOC);

            glDisable(GL_BLEND);
        }

        glBindBuffer(GL_TEXTURE_BUFFER, 0);
    }*/

    glBindVertexArray(0);

    if (list.render_flags & EnableTimers) {
        glQueryCounter(queries_[cur_query_][TimeDrawEnd], GL_TIMESTAMP);
    }

    for (int i = REN_MAT_TEX0_SLOT; i <= REN_MAT_TEX4_SLOT; i++) {
        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, i, 0);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, view_state_.scr_res[0], view_state_.scr_res[1]);

    //
    // Retrieve debug timers result
    //

    if (list.render_flags & EnableTimers) {
        // Get timer queries result (for previous frame)

        GLuint64 time_draw_start, time_skinning_start, time_shadow_start,
            time_depth_opaque_start, time_ao_start, time_opaque_start, time_transp_start,
            time_refl_start, time_taa_start, time_blur_start, time_blit_start,
            time_draw_end;

        cur_query_ = (cur_query_ + 1) % FrameSyncWindow;

        glGetQueryObjectui64v(queries_[cur_query_][TimeDrawStart], GL_QUERY_RESULT,
                              &time_draw_start);
        glGetQueryObjectui64v(queries_[cur_query_][TimeSkinningStart], GL_QUERY_RESULT,
                              &time_skinning_start);
        glGetQueryObjectui64v(queries_[cur_query_][TimeShadowMapStart], GL_QUERY_RESULT,
                              &time_shadow_start);
        glGetQueryObjectui64v(queries_[cur_query_][TimeDepthOpaqueStart], GL_QUERY_RESULT,
                              &time_depth_opaque_start);
        glGetQueryObjectui64v(queries_[cur_query_][TimeAOPassStart], GL_QUERY_RESULT,
                              &time_ao_start);
        glGetQueryObjectui64v(queries_[cur_query_][TimeOpaqueStart], GL_QUERY_RESULT,
                              &time_opaque_start);
        glGetQueryObjectui64v(queries_[cur_query_][TimeTranspStart], GL_QUERY_RESULT,
                              &time_transp_start);
        glGetQueryObjectui64v(queries_[cur_query_][TimeReflStart], GL_QUERY_RESULT,
                              &time_refl_start);
        glGetQueryObjectui64v(queries_[cur_query_][TimeTaaStart], GL_QUERY_RESULT,
                              &time_taa_start);
        glGetQueryObjectui64v(queries_[cur_query_][TimeBlurStart], GL_QUERY_RESULT,
                              &time_blur_start);
        glGetQueryObjectui64v(queries_[cur_query_][TimeBlitStart], GL_QUERY_RESULT,
                              &time_blit_start);
        glGetQueryObjectui64v(queries_[cur_query_][TimeDrawEnd], GL_QUERY_RESULT,
                              &time_draw_end);

        // assign values from previous frame
        backend_info_.cpu_start_timepoint_us = backend_cpu_start_;
        backend_info_.cpu_end_timepoint_us = backend_cpu_end_;
        backend_info_.gpu_cpu_time_diff_us = backend_time_diff_;

        backend_info_.gpu_start_timepoint_us = uint64_t(time_draw_start / 1000);
        backend_info_.gpu_end_timepoint_us = uint64_t(time_draw_end / 1000);

        backend_info_.skinning_time_us =
            uint32_t((time_shadow_start - time_skinning_start) / 1000);
        backend_info_.shadow_time_us =
            uint32_t((time_depth_opaque_start - time_shadow_start) / 1000);
        backend_info_.depth_opaque_pass_time_us =
            uint32_t((time_ao_start - time_depth_opaque_start) / 1000);
        backend_info_.ao_pass_time_us =
            uint32_t((time_opaque_start - time_ao_start) / 1000);
        backend_info_.opaque_pass_time_us =
            uint32_t((time_transp_start - time_opaque_start) / 1000);
        backend_info_.transp_pass_time_us =
            uint32_t((time_refl_start - time_transp_start) / 1000);
        backend_info_.refl_pass_time_us =
            uint32_t((time_taa_start - time_refl_start) / 1000);
        backend_info_.taa_pass_time_us =
            uint32_t((time_blur_start - time_taa_start) / 1000);
        backend_info_.blur_pass_time_us =
            uint32_t((time_blit_start - time_blur_start) / 1000);
        backend_info_.blit_pass_time_us =
            uint32_t((time_draw_end - time_blit_start) / 1000);
    }

#if 0
    glFinish();
#endif
}
#endif

uint64_t Renderer::GetGpuTimeBlockingUs() {
    GLint64 time = 0;
    glGetInteger64v(GL_TIMESTAMP, &time);
    return (uint64_t)(time / 1000);
}

void Renderer::BlitPixels(const void *data, const int w, const int h, const Ren::eTexFormat format) {
    using namespace RendererInternal;

    if (!temp_tex_ || temp_tex_->params.w != w || temp_tex_->params.h != h || temp_tex_->params.format != format) {
        temp_tex_ = {};

        Ren::Tex2DParams params;
        params.w = w;
        params.h = h;
        params.format = format;
        params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

        Ren::eTexLoadStatus status;
        temp_tex_ = ctx_.LoadTexture2D("__TEMP_BLIT_TEXTURE__", params, ctx_.default_mem_allocs(), &status);
        assert(status == Ren::eTexLoadStatus::CreatedDefault);
    }

    { // Update texture content
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, temp_tex_->id());

        if (format == Ren::eTexFormat::RawRGBA32F) {
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_FLOAT, data);
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, view_state_.scr_res[0], view_state_.scr_res[1]);

    glBindVertexArray(temp_vtx_input_.gl_vao());
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    BlitTexture(-1.0f, 1.0f, 2.0f, -2.0f, temp_tex_);

    glBindVertexArray(0);
}

void Renderer::BlitPixelsTonemap(const void *data, const int w, const int h, const Ren::eTexFormat format) {
    using namespace RendererInternal;

    if (!temp_tex_ || temp_tex_->params.w != w || temp_tex_->params.h != h || temp_tex_->params.format != format) {
        temp_tex_ = {};

        Ren::Tex2DParams params;
        params.w = w;
        params.h = h;
        params.format = format;
        params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

        Ren::eTexLoadStatus status;
        temp_tex_ = ctx_.LoadTexture2D("__TEMP_BLIT_TEXTURE__", params, ctx_.default_mem_allocs(), &status);
        assert(status == Ren::eTexLoadStatus::CreatedDefault);
    }

    { // Update texture content
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, temp_tex_->id());

        if (format == Ren::eTexFormat::RawRGBA32F) {
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_FLOAT, data);
        }
    }

    assert(format == Ren::eTexFormat::RawRGBA32F);

    Ren::Vec3f avarage_color;
    int sample_count = 0;
    const auto *_data = (const float *)data;

    for (int y = 0; y < h; y += 100) {
        for (int x = 0; x < w; x += 100) {
            int i = y * w + x;
            avarage_color += Ren::MakeVec3(&_data[i * 4 + 0]);
            sample_count++;
        }
    }

    avarage_color /= float(sample_count);

    const float lum = Dot(avarage_color, Ren::Vec3f{0.299f, 0.587f, 0.114f});

    const float alpha = 0.25f;
    reduced_average_ = alpha * lum + (1.0f - alpha) * reduced_average_;

    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    { // prepare downsampled buffer
        Ren::RastState rast_state;
        rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

        rast_state.viewport[2] = down_tex_4x_->params.w;
        rast_state.viewport[3] = down_tex_4x_->params.h;

        rast_state.Apply();

        ////

        Ren::Program *cur_program = blit_down_prog_.get();

        // TODO: REMOVE THIS!
        /*unif_shared_data_buf_[cur_buf_chunk_].write_count = 1;

        Graph::RpAllocBuf &unif_shared_data_buf =
            rp_builder_.GetReadBuffer(unif_shared_data_buf_[cur_buf_chunk_]);
        glBindBufferBase(GL_UNIFORM_BUFFER, REN_UB_SHARED_DATA_LOC,
                         (GLuint)unif_shared_data_buf.ref->id());*/

        const PrimDraw::Binding binding = {Ren::eBindTarget::Tex2D, REN_BASE0_TEX_SLOT, *temp_tex_};
        const PrimDraw::Uniform uniforms[] = {{0, Ren::Vec4f{0.0f, 0.0f, 1.0f, 1.0f}}};

        if (!down_tex_4x_fb_.Setup(ctx_.api_ctx(), {}, down_tex_4x_->params.w, down_tex_4x_->params.h, down_tex_4x_, {},
                                   {}, false)) {
            ctx_.log()->Error("Failed to init down_tex_4x_fb_");
        }

        prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, {&down_tex_4x_fb_, 0}, cur_program, &binding, 1, uniforms, 1);
    }

    { // prepare blurred buffer
        Ren::Program *cur_program = blit_gauss_prog_.get();

        // TODO: FIX THIS!

        /*{
            const auto &p = blur_tex_[1]->params();

            Ren::RastState rast_state;
            rast_state.cull_face.enabled = true;

            rast_state.viewport[2] = p.w;
            rast_state.viewport[3] = p.h;

            rast_state.Apply();

            ////

            const PrimDraw::Binding binding = {
                Ren::eBindTarget::Tex2D, REN_BASE0_TEX_SLOT, down_tex_4x_->handle()};

            const PrimDraw::Uniform uniforms[] = {
                {0, Ren::Vec4f{0.0f, 0.0f, float(p.w), float(p.h)}}, {1, 0.0f}};

            Ren::TexHandle blur_tex_handle = blur_tex_[1]->handle();
            blur_tex_fb_[1].Setup(&blur_tex_handle, 1, {}, {}, false);

            prim_draw_.DrawPrim(PrimDraw::ePrim::Quad,
                                {blur_tex_fb_[1].id(), GL_COLOR_BUFFER_BIT}, cur_program,
                                &binding, 1, uniforms, 2);
        }

        {
            const auto &p = blur_tex_[0]->params();

            Ren::RastState rast_state;
            rast_state.cull_face.enabled = true;

            rast_state.viewport[2] = p.w;
            rast_state.viewport[3] = p.h;

            rast_state.Apply();

            ////

            const PrimDraw::Binding binding = {
                Ren::eBindTarget::Tex2D, REN_BASE0_TEX_SLOT, blur_tex_[1]->handle()};

            const PrimDraw::Uniform uniforms[] = {
                {0, Ren::Vec4f{0.0f, 0.0f, float(p.w), float(p.h)}}, {1, 1.0f}};

            prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, {blur_tex_fb_[0].id(), 0},
                                cur_program, &binding, 1, uniforms, 2);
        }*/
    }

    // TODO: FIX THIS!

    /*{ // combine buffers
        Ren::RastState rast_state;
        rast_state.cull_face.enabled = true;

        rast_state.viewport[2] = view_state_.scr_res[2];
        rast_state.viewport[3] = view_state_.scr_res[3];

        rast_state.Apply();

        ////

        Ren::Program *cur_program = blit_combine_prog_.get();

        float exposure = 1.0f / reduced_average_;
        exposure = std::min(exposure, 1000.0f);

        const PrimDraw::Binding bindings[] = {
            {Ren::eBindTarget::Tex2D, REN_BASE0_TEX_SLOT, temp_tex_->handle()},
            {Ren::eBindTarget::Tex2D, REN_BASE1_TEX_SLOT, blur_tex_[0]->handle()}};

        const PrimDraw::Uniform uniforms[] = {
            {0, Ren::Vec4f{0.0f, float(h), float(w), -float(h)}}, // vertically flipped
            {12, 1.0f},
            {13, Ren::Vec2f{float(w), float(h)}},
            {U_GAMMA, 2.2f},
            {U_EXPOSURE, exposure},
            {U_FADE, 0.0f}};

        prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, {0, 0}, cur_program, bindings, 2,
                            uniforms, 6);
    }*/

    glBindVertexArray(0);
}

void Renderer::BlitBuffer(const float px, const float py, const float sx, const float sy, const FrameBuf &buf,
                          const int first_att, const int att_count, const float multiplier) {
    using namespace RendererInternal;

    Ren::BufferRef vtx_buf1 = ctx_.default_vertex_buf1(), vtx_buf2 = ctx_.default_vertex_buf2(),
                   ndx_buf = ctx_.default_indices_buf();

    glBindVertexArray(temp_vtx_input_.gl_vao());

    Ren::Program *cur_program = nullptr;

    if (buf.sample_count > 1) {
        cur_program = blit_ms_prog_.get();
    } else {
        cur_program = blit_prog_.get();
    }
    glUseProgram(cur_program->id());

    glUniform4f(0, 0.0f, 0.0f, 1.0f, 1.0f);

    for (int i = first_att; i < first_att + att_count; i++) {
        const float positions[] = {
            px + float(i - first_att) * sx,     py,      px + float(i - first_att + 1) * sx, py,
            px + float(i - first_att + 1) * sx, py + sy, px + float(i - first_att) * sx,     py + sy};

        if (i == first_att) {
            const float uvs[] = {0.0f, 0.0f, float(buf.w), 0.0f, float(buf.w), float(buf.h), 0.0f, float(buf.h)};

            glBindBuffer(GL_ARRAY_BUFFER, vtx_buf1->id());
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ndx_buf->id());

            glBufferSubData(GL_ARRAY_BUFFER, GLintptr(temp_buf1_vtx_offset_ + sizeof(positions)), sizeof(uvs), uvs);
            glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, GLintptr(temp_buf_ndx_offset_), 6 * sizeof(uint16_t),
                            PrimDrawInternal::fs_quad_indices);

            glEnableVertexAttribArray(REN_VTX_POS_LOC);
            glVertexAttribPointer(REN_VTX_POS_LOC, 2, GL_FLOAT, GL_FALSE, 0,
                                  (const GLvoid *)uintptr_t(temp_buf1_vtx_offset_));

            glEnableVertexAttribArray(REN_VTX_UV1_LOC);
            glVertexAttribPointer(REN_VTX_UV1_LOC, 2, GL_FLOAT, GL_FALSE, 0,
                                  (const GLvoid *)uintptr_t(temp_buf1_vtx_offset_ + sizeof(positions)));

            glUniform1f(4, multiplier);
        }

        glBufferSubData(GL_ARRAY_BUFFER, GLintptr(temp_buf1_vtx_offset_), sizeof(positions), positions);

        if (buf.sample_count > 1) {
            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D_MULTISAMPLE, REN_BASE0_TEX_SLOT, buf.attachments[i].tex->id());
        } else {
            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_BASE0_TEX_SLOT, buf.attachments[i].tex->id());
        }

        glBindVertexArray(temp_vtx_input_.gl_vao());

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));
    }

    glDisableVertexAttribArray(REN_VTX_POS_LOC);
    glDisableVertexAttribArray(REN_VTX_UV1_LOC);
}

void Renderer::BlitTexture(const float px, const float py, const float sx, const float sy, const Ren::Tex2DRef &tex,
                           const float multiplier, const bool is_ms) {
    using namespace RendererInternal;

    Ren::BufferRef vtx_buf1 = ctx_.default_vertex_buf1(), vtx_buf2 = ctx_.default_vertex_buf2(),
                   ndx_buf = ctx_.default_indices_buf();

    Ren::Program *cur_program = nullptr;

    if (is_ms) {
        cur_program = blit_ms_prog_.get();
    } else {
        cur_program = blit_prog_.get();
    }
    glUseProgram(cur_program->id());

    glUniform4f(0, 0.0f, 0.0f, 1.0f, 1.0f);

    {
        const auto &p = tex->params;

        const float positions[] = {px, py, px + sx, py, px + sx, py + sy, px, py + sy};

        const float uvs[] = {0.0f, 0.0f, float(p.w), 0.0f, float(p.w), float(p.h), 0.0f, float(p.h)};

        uint16_t indices[] = {0, 1, 2, 0, 2, 3};

        if (sy < 0.0f) {
            // keep counter-clockwise winding order
            std::swap(indices[0], indices[2]);
            std::swap(indices[3], indices[5]);
        }

        glBindBuffer(GL_ARRAY_BUFFER, vtx_buf1->id());
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ndx_buf->id());

        glBufferSubData(GL_ARRAY_BUFFER, GLintptr(temp_buf1_vtx_offset_), sizeof(positions), positions);
        glBufferSubData(GL_ARRAY_BUFFER, GLintptr(temp_buf1_vtx_offset_ + sizeof(positions)), sizeof(uvs), uvs);
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, GLintptr(temp_buf_ndx_offset_), sizeof(indices), indices);

        glEnableVertexAttribArray(REN_VTX_POS_LOC);
        glVertexAttribPointer(REN_VTX_POS_LOC, 2, GL_FLOAT, GL_FALSE, 0,
                              (const GLvoid *)uintptr_t(temp_buf1_vtx_offset_));

        glEnableVertexAttribArray(REN_VTX_UV1_LOC);
        glVertexAttribPointer(REN_VTX_UV1_LOC, 2, GL_FLOAT, GL_FALSE, 0,
                              (const GLvoid *)uintptr_t(temp_buf1_vtx_offset_ + sizeof(positions)));

        glUniform1f(4, multiplier);

        if (is_ms) {
            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D_MULTISAMPLE, REN_BASE0_TEX_SLOT, tex->id());
        } else {
            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_BASE0_TEX_SLOT, tex->id());
        }

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));
    }

    glDisableVertexAttribArray(REN_VTX_POS_LOC);
    glDisableVertexAttribArray(REN_VTX_UV1_LOC);
}

void Renderer::BlitToTempProbeFace(const FrameBuf &src_buf, const ProbeStorage &dst_store, const int face) {
    using namespace RendererInternal;

    Ren::BufferRef vtx_buf1 = ctx_.default_vertex_buf1(), vtx_buf2 = ctx_.default_vertex_buf2(),
                   ndx_buf = ctx_.default_indices_buf();

    GLint framebuf_before;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuf_before);

    GLint viewport_before[4];
    glGetIntegerv(GL_VIEWPORT, viewport_before);

    auto framebuf = GLuint(temp_framebuf_);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuf);

    int temp_probe_index = dst_store.reserved_temp_layer();

    auto cube_array = GLuint(dst_store.handle().id);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, cube_array, 0, GLint(temp_probe_index * 6 + face));
    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

    glViewport(0, 0, GLint(dst_store.res()), GLint(dst_store.res()));

    glDisable(GL_BLEND);

    glBindVertexArray(temp_vtx_input_.gl_vao());

    glBindBuffer(GL_ARRAY_BUFFER, vtx_buf1->id());
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ndx_buf->id());

    glEnableVertexAttribArray(REN_VTX_POS_LOC);
    glVertexAttribPointer(REN_VTX_POS_LOC, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)uintptr_t(temp_buf1_vtx_offset_));

    glEnableVertexAttribArray(REN_VTX_UV1_LOC);
    glVertexAttribPointer(REN_VTX_UV1_LOC, 2, GL_FLOAT, GL_FALSE, 0,
                          (const GLvoid *)uintptr_t(temp_buf1_vtx_offset_ + 8 * sizeof(float)));

    glBufferSubData(GL_ARRAY_BUFFER, GLintptr(temp_buf1_vtx_offset_), 8 * sizeof(float),
                    PrimDrawInternal::fs_quad_positions);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, GLintptr(temp_buf_ndx_offset_), 6 * sizeof(uint16_t),
                    PrimDrawInternal::fs_quad_indices);

    { // Update first mip level of a cubemap
        Ren::Program *prog = blit_rgbm_prog_.get();
        glUseProgram(prog->id());

        glUniform4f(0, 0.0f, 0.0f, 1.0f, 1.0f);

        const float uvs[] = {
            0.0f, 0.0f, (float)src_buf.w, 0.0f, (float)src_buf.w, (float)src_buf.h, 0.0f, (float)src_buf.h};

        glBufferSubData(GL_ARRAY_BUFFER, GLintptr(temp_buf1_vtx_offset_ + 8 * sizeof(float)), sizeof(uvs), uvs);

        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, 0, src_buf.attachments[0].tex->id());

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));
    }

    { // Update rest of mipmaps
        Ren::Program *prog = blit_mipmap_prog_.get();
        glUseProgram(prog->id());

        glUniform4f(0, 0.0f, 0.0f, 1.0f, 1.0f);

        glUniform1f(1, float(temp_probe_index));
        glUniform1i(2, face);

        const float uvs[] = {-1.0f, 1.0f, -1.0, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f};

        glBufferSubData(GL_ARRAY_BUFFER, GLintptr(temp_buf1_vtx_offset_ + 8 * sizeof(float)), sizeof(uvs), uvs);

        ren_glBindTextureUnit_Comp(GL_TEXTURE_CUBE_MAP_ARRAY, 0, cube_array);

        int res = dst_store.res() / 2;
        int level = 1;

        while (level <= dst_store.max_level()) {
            glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, cube_array, level,
                                      GLint(temp_probe_index * 6 + face));
            assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

            glViewport(0, 0, res, res);

            glUniform1f(3, float(level - 1));

            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));

            res /= 2;
            level++;
        }
    }

    glDisableVertexAttribArray(REN_VTX_POS_LOC);
    glDisableVertexAttribArray(REN_VTX_UV1_LOC);

    glBindVertexArray(0);

    glBindFramebuffer(GL_FRAMEBUFFER, framebuf_before);
    glViewport(viewport_before[0], viewport_before[1], viewport_before[2], viewport_before[3]);
}

void Renderer::BlitPrefilterFromTemp(const ProbeStorage &dst_store, const int probe_index) {
    using namespace RendererInternal;

    Ren::BufferRef vtx_buf1 = ctx_.default_vertex_buf1(), vtx_buf2 = ctx_.default_vertex_buf2(),
                   ndx_buf = ctx_.default_indices_buf();

    GLint framebuf_before;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuf_before);

    GLint viewport_before[4];
    glGetIntegerv(GL_VIEWPORT, viewport_before);

    auto framebuf = GLuint(temp_framebuf_);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuf);

    int temp_probe_index = dst_store.reserved_temp_layer();

    auto cube_array = GLuint(dst_store.handle().id);

    glDisable(GL_BLEND);

    glBindVertexArray(temp_vtx_input_.gl_vao());

    glBindBuffer(GL_ARRAY_BUFFER, vtx_buf1->id());
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ndx_buf->id());

    glEnableVertexAttribArray(REN_VTX_POS_LOC);
    glVertexAttribPointer(REN_VTX_POS_LOC, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)uintptr_t(temp_buf1_vtx_offset_));

    glEnableVertexAttribArray(REN_VTX_UV1_LOC);
    glVertexAttribPointer(REN_VTX_UV1_LOC, 2, GL_FLOAT, GL_FALSE, 0,
                          (const GLvoid *)uintptr_t(temp_buf1_vtx_offset_ + 8 * sizeof(float)));

    glBufferSubData(GL_ARRAY_BUFFER, GLintptr(temp_buf1_vtx_offset_), 8 * sizeof(float),
                    PrimDrawInternal::fs_quad_positions);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, GLintptr(temp_buf_ndx_offset_), 6 * sizeof(uint16_t),
                    PrimDrawInternal::fs_quad_indices);

    const float uvs[] = {-1.0f, 1.0f, -1.0, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f};

    glBufferSubData(GL_ARRAY_BUFFER, GLintptr(temp_buf1_vtx_offset_ + 8 * sizeof(float)), sizeof(uvs), uvs);

    Ren::Program *prog = blit_prefilter_prog_.get();
    glUseProgram(prog->id());

    glUniform4f(0, 0.0f, 0.0f, 1.0f, 1.0f);
    glUniform1f(1, float(temp_probe_index));

    ren_glBindTextureUnit_Comp(GL_TEXTURE_CUBE_MAP_ARRAY, 0, cube_array);

    int res = dst_store.res();
    int level = 0;

    while (level <= dst_store.max_level()) {
        glViewport(0, 0, res, res);

        const float roughness = (1.0f / 6.0f) * float(level);
        glUniform1f(3, roughness);

        for (int face = 0; face < 6; face++) {
            glUniform1i(2, face);

            glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, cube_array, level,
                                      GLint(probe_index * 6 + face));
            assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));
        }

        res /= 2;
        level++;
    }

    glDisableVertexAttribArray(REN_VTX_POS_LOC);
    glDisableVertexAttribArray(REN_VTX_UV1_LOC);

    glBindVertexArray(0);

    glBindFramebuffer(GL_FRAMEBUFFER, framebuf_before);
    glViewport(viewport_before[0], viewport_before[1], viewport_before[2], viewport_before[3]);
}

bool Renderer::BlitProjectSH(const ProbeStorage &store, const int probe_index, const int iteration, LightProbe &probe) {
    using namespace RendererInternal;

    Ren::BufferRef vtx_buf1 = ctx_.default_vertex_buf1(), vtx_buf2 = ctx_.default_vertex_buf2(),
                   ndx_buf = ctx_.default_indices_buf();

    GLint framebuf_before;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuf_before);

    GLint viewport_before[4];
    glGetIntegerv(GL_VIEWPORT, viewport_before);

    glBindFramebuffer(GL_FRAMEBUFFER, probe_sample_buf_.fb);

    glViewport(0, 0, probe_sample_buf_.w, probe_sample_buf_.h);

    glDisable(GL_BLEND);

    glBindVertexArray(temp_vtx_input_.gl_vao());

    glBindBuffer(GL_ARRAY_BUFFER, vtx_buf1->id());
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ndx_buf->id());

    glEnableVertexAttribArray(REN_VTX_POS_LOC);
    glVertexAttribPointer(REN_VTX_POS_LOC, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)uintptr_t(temp_buf1_vtx_offset_));

    glEnableVertexAttribArray(REN_VTX_UV1_LOC);
    glVertexAttribPointer(REN_VTX_UV1_LOC, 2, GL_FLOAT, GL_FALSE, 0,
                          (const GLvoid *)uintptr_t(temp_buf1_vtx_offset_ + 8 * sizeof(float)));

    const float uvs[] = {0.0f,
                         0.0f,
                         float(probe_sample_buf_.w),
                         0.0f,
                         float(probe_sample_buf_.w),
                         float(probe_sample_buf_.h),
                         0.0f,
                         float(probe_sample_buf_.h)};

    glBufferSubData(GL_ARRAY_BUFFER, GLintptr(temp_buf1_vtx_offset_), 8 * sizeof(float),
                    PrimDrawInternal::fs_quad_positions);
    glBufferSubData(GL_ARRAY_BUFFER, GLintptr(temp_buf1_vtx_offset_ + 8 * sizeof(float)), sizeof(uvs), uvs);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, GLintptr(temp_buf_ndx_offset_), 6 * sizeof(uint16_t),
                    PrimDrawInternal::fs_quad_indices);

    if (iteration != 0) {
        // Retrieve result of previous read
        glBindBuffer(GL_PIXEL_PACK_BUFFER, GLuint(probe_sample_pbo_));

        Ren::Vec3f sh_coeffs[4];

        auto *pixels = (float *)glMapBufferRange(
            GL_PIXEL_PACK_BUFFER, 0, GLsizeiptr(4) * probe_sample_buf_.w * probe_sample_buf_.h * sizeof(float),
            GL_MAP_READ_BIT);
        if (pixels) {
            for (int y = 0; y < probe_sample_buf_.h; y++) {
                for (int x = 0; x < probe_sample_buf_.w; x++) {
                    const int i = (x >= 8) ? ((x >= 16) ? 2 : 1) : 0;

                    sh_coeffs[0][i] += pixels[4 * (y * probe_sample_buf_.w + x) + 0];
                    sh_coeffs[1][i] += pixels[4 * (y * probe_sample_buf_.w + x) + 1];
                    sh_coeffs[2][i] += pixels[4 * (y * probe_sample_buf_.w + x) + 2];
                    sh_coeffs[3][i] += pixels[4 * (y * probe_sample_buf_.w + x) + 3];
                }
            }
            glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
        }
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

        const float inv_weight = 1.0f / float(probe_sample_buf_.h * probe_sample_buf_.h);
        for (Ren::Vec3f &sh_coeff : sh_coeffs) {
            sh_coeff *= inv_weight;
        }

        const float k = 1.0f / float(iteration);
        for (int i = 0; i < 4; i++) {
            const Ren::Vec3f diff = sh_coeffs[i] - probe.sh_coeffs[i];
            probe.sh_coeffs[i] += diff * k;
        }
    }

    if (iteration < 64) {
        { // Sample cubemap and project on sh basis
            Ren::Program *prog = blit_project_sh_prog_.get();
            glUseProgram(prog->id());

            glUniform4f(0, 0.0f, 0.0f, 1.0f, 1.0f);

            ren_glBindTextureUnit_Comp(GL_TEXTURE_CUBE_MAP_ARRAY, 1, store.handle().id);

            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, 1, rand2d_8x8_->id());

            glUniform1f(1, float(probe_index));
            glUniform1i(2, iteration);

            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));
        }

        { // Start readback from buffer (result will be retrieved at the start of next
          // iteration)
            glReadBuffer(GL_COLOR_ATTACHMENT0);
            glBindBuffer(GL_PIXEL_PACK_BUFFER, GLuint(probe_sample_pbo_));

            glReadPixels(0, 0, probe_sample_buf_.w, probe_sample_buf_.h, GL_RGBA, GL_FLOAT, nullptr);

            glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        }
    }

    glDisableVertexAttribArray(REN_VTX_POS_LOC);
    glDisableVertexAttribArray(REN_VTX_UV1_LOC);

    glBindVertexArray(0);

    glBindFramebuffer(GL_FRAMEBUFFER, framebuf_before);
    glViewport(viewport_before[0], viewport_before[1], viewport_before[2], viewport_before[3]);

    return iteration == 64;
}

#undef _AS_STR
#undef AS_STR
