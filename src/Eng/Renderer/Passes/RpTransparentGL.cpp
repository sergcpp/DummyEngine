#include "RpTransparent.h"

#include "../DebugMarker.h"
#include "../Renderer_Structs.h"

#include <Ren/Context.h>
#include <Ren/RastState.h>

namespace RpSharedInternal {
uint32_t _draw_list_range_full(Ren::Context &ctx,
                               const DynArrayConstRef<MainDrawBatch> &main_batches,
                               const DynArrayConstRef<uint32_t> &main_batch_indices,
                               uint32_t i, uint64_t mask, uint64_t &cur_mat_id,
                               uint64_t &cur_prog_id, BackendInfo &backend_info);

uint32_t _draw_list_range_full_rev(Ren::Context &ctx,
                                   const DynArrayConstRef<MainDrawBatch> &main_batches,
                                   const DynArrayConstRef<uint32_t> &main_batch_indices,
                                   uint32_t ndx, uint64_t mask, uint64_t &cur_mat_id,
                                   uint64_t &cur_prog_id, BackendInfo &backend_info);
} // namespace RpSharedInternal

void RpTransparent::DrawTransparent_Simple(Graph::RpBuilder &builder,
                                           Graph::AllocatedBuffer &unif_shared_data_buf) {
    Ren::RastState rast_state;
    rast_state.depth_test.enabled = true;
    rast_state.depth_test.func = Ren::eTestFunc::Less;
    rast_state.depth_mask = false;

    rast_state.blend.enabled = true;
    rast_state.blend.src = Ren::eBlendFactor::SrcAlpha;
    rast_state.blend.dst = Ren::eBlendFactor::OneMinusSrcAlpha;

    rast_state.cull_face.enabled = true;
    rast_state.cull_face.face = Ren::eCullFace::Front;

    if (render_flags_ & DebugWireframe) {
        rast_state.polygon_mode = Ren::ePolygonMode::Line;
    } else {
        rast_state.polygon_mode = Ren::ePolygonMode::Fill;
    }

    // Bind main buffer for drawing
#if defined(REN_DIRECT_DRAWING)
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    rast_state.viewport[2] = view_state_->scr_res[0];
    rast_state.viewport[3] = view_state_->scr_res[1];
#else
    glBindFramebuffer(GL_FRAMEBUFFER, transparent_draw_fb_.id());

    rast_state.viewport[2] = view_state_->act_res[0];
    rast_state.viewport[3] = view_state_->act_res[1];
#endif

    rast_state.Apply();
    Ren::RastState applied_state = rast_state;

    glBindVertexArray(draw_pass_vao_.id());

    //
    // Bind resources (shadow atlas, lightmap, cells item data)
    //

    glBindBufferBase(GL_UNIFORM_BUFFER, REN_UB_SHARED_DATA_LOC,
                     unif_shared_data_buf.ref->id());

    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_SHAD_TEX_SLOT, shadow_tex_.id);

    if (decals_atlas_) {
        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_DECAL_TEX_SLOT,
                                   decals_atlas_->tex_id(0));
    }

    if ((render_flags_ & (EnableZFill | EnableSSAO)) == (EnableZFill | EnableSSAO)) {
        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_SSAO_TEX_SLOT, ssao_tex_.id);
    } else {
        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_SSAO_TEX_SLOT, dummy_white_->id());
    }

    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_BRDF_TEX_SLOT, brdf_lut_->id());

    if ((render_flags_ & EnableLightmap) && env_->lm_direct) {
        for (int sh_l = 0; sh_l < 4; sh_l++) {
            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_LMAP_SH_SLOT + sh_l,
                                       env_->lm_indir_sh[sh_l]->id());
        }
    } else {
        for (int sh_l = 0; sh_l < 4; sh_l++) {
            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_LMAP_SH_SLOT + sh_l,
                                       dummy_black_->id());
        }
    }

    ren_glBindTextureUnit_Comp(GL_TEXTURE_CUBE_MAP_ARRAY, REN_ENV_TEX_SLOT,
                               probe_storage_ ? probe_storage_->tex_id() : 0);

    ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, REN_LIGHT_BUF_SLOT, lights_tbo_->id());
    ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, REN_DECAL_BUF_SLOT, decals_tbo_->id());
    ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, REN_CELLS_BUF_SLOT, cells_tbo_->id());
    ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, REN_ITEMS_BUF_SLOT, items_tbo_->id());

    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_NOISE_TEX_SLOT, noise_tex_->id());
    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_CONE_RT_LUT_SLOT, cone_rt_lut_->id());

    ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, REN_INST_BUF_SLOT,
                               GLuint(instances_tbo_->id()));

    uint64_t cur_prog_id = 0xffffffffffffffff;
    uint64_t cur_mat_id = 0xffffffffffffffff;

    Ren::Context &ctx = builder.ctx();
    BackendInfo backend_info;

    for (int j = int(main_batch_indices_.count) - 1; j >= *alpha_blend_start_index_;
         j--) {
        const MainDrawBatch &batch = main_batches_.data[main_batch_indices_.data[j]];
        if (!batch.alpha_blend_bit || !batch.two_sided_bit) {
            continue;
        }

        if (!batch.instance_count) {
            continue;
        }

        if (batch.depth_write_bit) {
            rast_state.depth_mask = true;
            rast_state.ApplyChanged(applied_state);
            applied_state = rast_state;
        }

        if (cur_prog_id != batch.prog_id) {
            const Ren::Program *p = ctx.GetProgram(batch.prog_id).get();
            glUseProgram(p->id());
        }

        if (cur_mat_id != batch.mat_id) {
            const Ren::Material *mat = ctx.GetMaterial(batch.mat_id).get();

            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_MAT_TEX0_SLOT,
                                       mat->textures[0]->id());
            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_MAT_TEX1_SLOT,
                                       mat->textures[1]->id());
            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_MAT_TEX2_SLOT,
                                       mat->textures[2]->id());
            if (mat->textures[3]) {
                ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_MAT_TEX3_SLOT,
                                           mat->textures[3]->id());
                if (mat->textures[4]) {
                    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_MAT_TEX4_SLOT,
                                               mat->textures[4]->id());
                }
            }
        }

        if (cur_prog_id != batch.prog_id || cur_mat_id != batch.mat_id) {
            const Ren::Material *mat = ctx.GetMaterial(batch.mat_id).get();
            if (mat->params_count) {
                glUniform4fv(REN_U_MAT_PARAM_LOC, 1, ValuePtr(mat->params[0]));
            }
        }

        cur_prog_id = batch.prog_id;
        cur_mat_id = batch.mat_id;

        glUniform4iv(REN_U_INSTANCES_LOC, (batch.instance_count + 3) / 4,
                     &batch.instance_indices[0]);

        glDrawElementsInstancedBaseVertex(
            GL_TRIANGLES, batch.indices_count, GL_UNSIGNED_INT,
            (const GLvoid *)uintptr_t(batch.indices_offset * sizeof(uint32_t)),
            GLsizei(batch.instance_count), GLint(batch.base_vertex));

        backend_info.opaque_draw_calls_count += 2;
        backend_info.tris_rendered += (batch.indices_count / 3) * batch.instance_count;
    }

    rast_state.depth_mask = false;
    rast_state.cull_face.face = Ren::eCullFace::Back;
    rast_state.ApplyChanged(applied_state);
    applied_state = rast_state;

    for (int j = int(main_batch_indices_.count) - 1; j >= *alpha_blend_start_index_;
         j--) {
        const MainDrawBatch &batch = main_batches_.data[main_batch_indices_.data[j]];
        if (!batch.instance_count) {
            continue;
        }

        if (batch.depth_write_bit) {
            rast_state.depth_mask = true;
            rast_state.ApplyChanged(applied_state);
            applied_state = rast_state;
        }

        if (cur_prog_id != batch.prog_id) {
            const Ren::Program *p = ctx.GetProgram(batch.prog_id).get();
            glUseProgram(p->id());
        }

        if (cur_mat_id != batch.mat_id) {
            const Ren::Material *mat = ctx.GetMaterial(batch.mat_id).get();

            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_MAT_TEX0_SLOT,
                                       mat->textures[0]->id());
            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_MAT_TEX1_SLOT,
                                       mat->textures[1]->id());
            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_MAT_TEX2_SLOT,
                                       mat->textures[2]->id());
            if (mat->textures[3]) {
                ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_MAT_TEX3_SLOT,
                                           mat->textures[3]->id());
                if (mat->textures[4]) {
                    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_MAT_TEX4_SLOT,
                                               mat->textures[4]->id());
                }
            }
        }

        if (cur_prog_id != batch.prog_id || cur_mat_id != batch.mat_id) {
            const Ren::Material *mat = ctx.GetMaterial(batch.mat_id).get();
            if (mat->params_count) {
                glUniform4fv(REN_U_MAT_PARAM_LOC, 1, ValuePtr(mat->params[0]));
            }
        }

        cur_prog_id = batch.prog_id;
        cur_mat_id = batch.mat_id;

        glUniform4iv(REN_U_INSTANCES_LOC, (batch.instance_count + 3) / 4,
                     &batch.instance_indices[0]);

        glDrawElementsInstancedBaseVertex(
            GL_TRIANGLES, batch.indices_count, GL_UNSIGNED_INT,
            (const GLvoid *)uintptr_t(batch.indices_offset * sizeof(uint32_t)),
            GLsizei(batch.instance_count), GLint(batch.base_vertex));

        backend_info.opaque_draw_calls_count += 2;
        backend_info.tris_rendered += (batch.indices_count / 3) * batch.instance_count;
    }

#if !defined(REN_DIRECT_DRAWING)
    if (view_state_->is_multisampled) {
        DebugMarker _resolve_ms("RESOLVE MS BUFFER");

        Ren::RastState rast_state;
        rast_state.cull_face.enabled = true;

        rast_state.viewport[2] = view_state_->act_res[0];
        rast_state.viewport[3] = view_state_->act_res[1];

        rast_state.Apply();
        Ren::RastState applied_state = rast_state;

        const PrimDraw::Binding bindings[] = {
            {Ren::eBindTarget::Tex2DMs, REN_BASE0_TEX_SLOT, color_tex_}};

        const PrimDraw::Uniform uniforms[] = {
            {0, Ren::Vec4f{0.0f, 0.0f, float(view_state_->act_res[0]),
                           float(view_state_->act_res[1])}}};

        prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, {resolved_fb_.id(), 0},
                            blit_ms_resolve_prog_.get(), bindings, 1, uniforms, 1);
    }
#endif
}

void RpTransparent::DrawTransparent_OIT_MomentBased(Graph::RpBuilder &builder) {
    Ren::RastState rast_state;
    rast_state.depth_test.enabled = true;
    rast_state.depth_test.func = Ren::eTestFunc::LEqual;
    rast_state.depth_mask = false;

    rast_state.blend.enabled = true;
    rast_state.blend.src = Ren::eBlendFactor::One;
    rast_state.blend.dst = Ren::eBlendFactor::One;

    rast_state.cull_face.enabled = true;
    rast_state.cull_face.face = Ren::eCullFace::Front;

    rast_state.polygon_mode = Ren::ePolygonMode::Fill;

    rast_state.viewport[2] = view_state_->act_res[0];
    rast_state.viewport[3] = view_state_->act_res[1];

    rast_state.Apply();
    Ren::RastState applied_state = rast_state;

    glBindFramebuffer(GL_FRAMEBUFFER, moments_fb_.id());
    glClear(GL_COLOR_BUFFER_BIT);

    //
    // Bind resources (shadow atlas, lightmap, cells item data)
    //

    Graph::AllocatedBuffer &unif_shared_data_buf = builder.GetReadBuffer(input_[1]);

    glBindBufferBase(GL_UNIFORM_BUFFER, REN_UB_SHARED_DATA_LOC,
                     unif_shared_data_buf.ref->id());

    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_SHAD_TEX_SLOT, shadow_tex_.id);

    if (decals_atlas_) {
        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_DECAL_TEX_SLOT,
                                   decals_atlas_->tex_id(0));
    }

    if ((render_flags_ & (EnableZFill | EnableSSAO)) == (EnableZFill | EnableSSAO)) {
        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_SSAO_TEX_SLOT, ssao_tex_.id);
    } else {
        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_SSAO_TEX_SLOT, dummy_white_->id());
    }

    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_BRDF_TEX_SLOT, brdf_lut_->id());

    if ((render_flags_ & EnableLightmap) && env_->lm_direct) {
        for (int sh_l = 0; sh_l < 4; sh_l++) {
            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_LMAP_SH_SLOT + sh_l,
                                       env_->lm_indir_sh[sh_l]->id());
        }
    } else {
        for (int sh_l = 0; sh_l < 4; sh_l++) {
            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_LMAP_SH_SLOT + sh_l,
                                       dummy_black_->id());
        }
    }

    Ren::Context &ctx = builder.ctx();
    BackendInfo backend_info;

    { // Draw alpha-blended surfaces
        DebugMarker _("MOMENTS GENERATION PASS");

        const Ren::Program *cur_program = nullptr;
        const Ren::Material *cur_mat = nullptr;

        for (int j = int(main_batch_indices_.count) - 1; j >= *alpha_blend_start_index_;
             j--) {
            const MainDrawBatch &batch = main_batches_.data[main_batch_indices_.data[j]];
            if (!batch.instance_count) {
                continue;
            }
            if (!batch.alpha_blend_bit) {
                break;
            }

            const Ren::Program *p = ctx.GetProgram(batch.prog_id).get();
            const Ren::Material *mat = ctx.GetMaterial(batch.mat_id).get();

            if (cur_program != p) {
                glUseProgram(p->id());
                cur_program = p;
            }

            if (cur_mat != mat) {
                ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_MAT_TEX0_SLOT,
                                           mat->textures[0]->id());
                cur_mat = mat;
            }

            glUniform4iv(REN_U_INSTANCES_LOC, (batch.instance_count + 3) / 4,
                         &batch.instance_indices[0]);

            glDrawElementsInstancedBaseVertex(
                GL_TRIANGLES, batch.indices_count, GL_UNSIGNED_INT,
                (const GLvoid *)uintptr_t(batch.indices_offset * sizeof(uint32_t)),
                (GLsizei)batch.instance_count, (GLint)batch.base_vertex);

            backend_info.opaque_draw_calls_count += 2;
            backend_info.tris_rendered +=
                (batch.indices_count / 3) * batch.instance_count;
        }
    }

    { // Change transparency draw mode
        glBindBuffer(GL_UNIFORM_BUFFER, (GLuint)unif_shared_data_buf.ref->id());
        const float transp_mode = view_state_->is_multisampled ? 4.0f : 3.0f;
        glBufferSubData(GL_UNIFORM_BUFFER,
                        offsetof(SharedDataBlock, uTranspParamsAndTime) +
                            2 * sizeof(float),
                        sizeof(float), &transp_mode);
    }

    const uint32_t target_framebuf =
        view_state_->is_multisampled ? color_only_fb_.id() : resolved_fb_.id();

    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)target_framebuf);

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    if (view_state_->is_multisampled) {
        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D_MULTISAMPLE, REN_MOMENTS0_MS_TEX_SLOT,
                                   moments_b0_.id);
        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D_MULTISAMPLE, REN_MOMENTS1_MS_TEX_SLOT,
                                   moments_z_and_z2_.id);
        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D_MULTISAMPLE, REN_MOMENTS2_MS_TEX_SLOT,
                                   moments_z3_and_z4_.id);
    } else {
        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_MOMENTS0_TEX_SLOT, moments_b0_.id);
        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_MOMENTS1_TEX_SLOT,
                                   moments_z_and_z2_.id);
        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_MOMENTS2_TEX_SLOT,
                                   moments_z3_and_z4_.id);
    }

    { // Draw alpha-blended surfaces
        DebugMarker _("COLOR PASS");

        const Ren::Program *cur_program = nullptr;
        const Ren::Material *cur_mat = nullptr;

        for (int j = int(main_batch_indices_.count) - 1; j >= 0; j--) {
            const MainDrawBatch &batch = main_batches_.data[main_batch_indices_.data[j]];
            if (!batch.instance_count) {
                continue;
            }
            if (!batch.alpha_blend_bit) {
                break;
            }

            const Ren::Program *p = ctx.GetProgram(batch.prog_id).get();
            const Ren::Material *mat = ctx.GetMaterial(batch.mat_id).get();

            if (cur_program != p) {
                glUseProgram(p->id());
                cur_program = p;
            }

            if (cur_mat != mat) {
                ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_MAT_TEX0_SLOT,
                                           mat->textures[0]->id());
                ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_MAT_TEX1_SLOT,
                                           mat->textures[1]->id());
                if (mat->textures[3]) {
                    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_MAT_TEX3_SLOT,
                                               mat->textures[3]->id());
                    if (mat->textures[4]) {
                        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_MAT_TEX4_SLOT,
                                                   mat->textures[4]->id());
                    }
                }
                cur_mat = mat;
            }

            glUniform4iv(REN_U_INSTANCES_LOC, (batch.instance_count + 3) / 4,
                         &batch.instance_indices[0]);

            glDrawElementsInstancedBaseVertex(
                GL_TRIANGLES, batch.indices_count, GL_UNSIGNED_INT,
                (const GLvoid *)uintptr_t(batch.indices_offset * sizeof(uint32_t)),
                GLsizei(batch.instance_count), GLint(batch.base_vertex));

            backend_info.opaque_draw_calls_count += 2;
            backend_info.tris_rendered +=
                (batch.indices_count / 3) * batch.instance_count;
        }
    }
}

void RpTransparent::DrawTransparent_OIT_WeightedBlended(Graph::RpBuilder &builder) {}

//
// This is needed for moment-based OIT
//

#if 0
#if (REN_OIT_MODE == REN_OIT_MOMENT_BASED)
{ // Attach depth from clean buffer to moments buffer
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)moments_buf_.fb);

    const auto depth_tex = (GLuint)clean_buf_.depth_tex->id();
    if (clean_buf_.sample_count > 1) {
        assert(moments_buf_.sample_count == clean_buf_.sample_count);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
            GL_TEXTURE_2D_MULTISAMPLE, depth_tex, 0);
    } else {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
            GL_TEXTURE_2D, depth_tex, 0);
    }

    const GLenum s = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    result &= (s == GL_FRAMEBUFFER_COMPLETE);
}
if (clean_buf_.sample_count == 1) {
    // Attach depth from clean buffer to transparent buffer
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)resolved_or_transparent_buf_.fb);

    const auto depth_tex = (GLuint)clean_buf_.depth_tex->id();
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D,
        depth_tex, 0);

    const GLenum s = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    result &= (s == GL_FRAMEBUFFER_COMPLETE);
}
#endif

////////////////////////////////////////////////////

#if (REN_OIT_MODE != REN_OIT_DISABLED)
if (list.render_flags & EnableOIT) {
    DebugMarker _("COMPOSE TRANSPARENT");

    glEnable(GL_BLEND);
#if (REN_OIT_MODE == REN_OIT_WEIGHTED_BLENDED) ||                                        \
    (REN_OIT_MODE == REN_OIT_MOMENT_BASED && REN_OIT_MOMENT_RENORMALIZE)
    glBlendFunc(GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA);
#else
    glBlendFunc(GL_ONE, GL_SRC_ALPHA);
#endif

    Ren::Program* blit_transparent_compose =
        (clean_buf_.sample_count > 1) ? blit_transparent_compose_ms_prog_.get()
        : blit_transparent_compose_prog_.get();

    const uint32_t target_framebuffer = (clean_buf_.sample_count > 1)
        ? resolved_or_transparent_buf_.fb
        : transparent_comb_framebuf_;

    // glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)target_framebuffer);
    // glViewport(0, 0, view_state_.act_res[0], view_state_.act_res[1]);

    // glUseProgram(blit_transparent_compose->id());
    // glUniform4f(0, 0.0f, 0.0f, float(view_state_.act_res[0]),
    //            float(view_state_.act_res[1]));

    PrimDraw::Binding bindings[3];

    if (clean_buf_.sample_count > 1) {
        // ren_glBindTextureUnit_Comp(
        //    GL_TEXTURE_2D_MULTISAMPLE, REN_BASE0_TEX_SLOT,
        //    clean_buf_.attachments[REN_OUT_COLOR_INDEX].tex->id());

        bindings[0] = { Ren::eBindTarget::Tex2DMs, REN_BASE0_TEX_SLOT,
                       clean_buf_.attachments[REN_OUT_COLOR_INDEX].tex->handle() };

#if REN_OIT_MODE == REN_OIT_MOMENT_BASED
        // ren_glBindTextureUnit_Comp(GL_TEXTURE_2D_MULTISAMPLE, REN_BASE1_TEX_SLOT,
        //                           moments_buf_.attachments[0].tex->id());

        bindings[1] = { Ren::eBindTarget::Tex2DMs, REN_BASE1_TEX_SLOT,
                       moments_buf_.attachments[0].tex->handle() };
#elif REN_OIT_MODE == REN_OIT_WEIGHTED_BLENDED
        // ren_glBindTextureUnit_Comp(GL_TEXTURE_2D_MULTISAMPLE, REN_BASE1_TEX_SLOT,
        //                           clean_buf_.attachments[REN_OUT_NORM_INDEX].tex);

        bindings[1] = { Ren::eBindTarget::Tex2DMs, REN_BASE1_TEX_SLOT,
                       clean_buf_.attachments[REN_OUT_NORM_INDEX].tex->handle() };
#endif
    } else {
        // ren_glBindTextureUnit_Comp(
        //   GL_TEXTURE_2D, REN_BASE0_TEX_SLOT,
        //    resolved_or_transparent_buf_.attachments[0].tex->id());

        bindings[0] = { Ren::eBindTarget::Tex2D, REN_BASE0_TEX_SLOT,
                       resolved_or_transparent_buf_.attachments[0].tex->handle() };
#if REN_OIT_MODE == REN_OIT_MOMENT_BASED
        // ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_BASE1_TEX_SLOT,
        //                           moments_buf_.attachments[0].tex->id());

        bindings[1] = { Ren::eBindTarget::Tex2D, REN_BASE1_TEX_SLOT,
                       moments_buf_.attachments[0].tex->handle() };
#elif REN_OIT_MODE == REN_OIT_WEIGHTED_BLENDED
        // ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_BASE1_TEX_SLOT,
        //                           clean_buf_.attachments[REN_OUT_NORM_INDEX].tex);

        bindings[1] = { Ren::eBindTarget::Tex2D, REN_BASE1_TEX_SLOT,
                       clean_buf_.attachments[REN_OUT_NORM_INDEX].tex->handle() };
#endif
    }

    // glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT,
    //               (const GLvoid *)uintptr_t(quad_ndx_offset_));

    const PrimDraw::Uniform uniforms[] = {
        {0, Ren::Vec4f{0.0f, 0.0f, float(view_state_.act_res[0]),
                       float(view_state_.act_res[1])}} };

    prim_draw_.DrawFsQuad(
        { target_framebuffer, 0,
         Ren::Vec4i{0, 0, view_state_.act_res[0], view_state_.act_res[1]} },
        blit_transparent_compose, bindings, sizeof(bindings) / sizeof(bindings[0]),
        uniforms, 1);

    glDisable(GL_BLEND);
}
#endif

#endif