#include "RpTransparent.h"

#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

#include <Ren/Context.h>
#include <Ren/DebugMarker.h>
#include <Ren/GL.h>
#include <Ren/RastState.h>

namespace RpSharedInternal {
void _bind_textures_and_samplers(Ren::Context &ctx, const Ren::Material &mat,
                                 Ren::SmallVectorImpl<Ren::SamplerRef> &temp_samplers);
uint32_t _draw_list_range_full(Eng::RpBuilder &builder, const Ren::MaterialStorage *materials,
                               const Ren::Pipeline pipelines[], Ren::Span<const Eng::CustomDrawBatch> main_batches,
                               Ren::Span<const uint32_t> main_batch_indices, uint32_t i, uint64_t mask,
                               uint64_t &cur_mat_id, uint64_t &cur_pipe_id, uint64_t &cur_prog_id,
                               Eng::BackendInfo &backend_info);

uint32_t _draw_list_range_full_rev(Eng::RpBuilder &builder, const Ren::MaterialStorage *materials,
                                   const Ren::Pipeline pipelines[], Ren::Span<const Eng::CustomDrawBatch> main_batches,
                                   Ren::Span<const uint32_t> main_batch_indices, uint32_t ndx, uint64_t mask,
                                   uint64_t &cur_mat_id, uint64_t &cur_pipe_id, uint64_t &cur_prog_id,
                                   Eng::BackendInfo &backend_info);
} // namespace RpSharedInternal

void Eng::RpTransparent::DrawTransparent_Simple(RpBuilder &builder, RpAllocBuf &instances_buf,
                                                RpAllocBuf &instance_indices_buf, RpAllocBuf &unif_shared_data_buf,
                                                RpAllocBuf &materials_buf, RpAllocBuf &cells_buf, RpAllocBuf &items_buf,
                                                RpAllocBuf &lights_buf, RpAllocBuf &decals_buf, RpAllocTex &shad_tex,
                                                RpAllocTex &color_tex, RpAllocTex &ssao_tex) {
    using namespace RpSharedInternal;

    Ren::RastState rast_state;
    rast_state.poly.cull = uint8_t(Ren::eCullFace::Front);

    if ((*p_list_)->render_settings.debug_wireframe) {
        rast_state.poly.mode = uint8_t(Ren::ePolygonMode::Line);
    } else {
        rast_state.poly.mode = uint8_t(Ren::ePolygonMode::Fill);
    }

    rast_state.depth.test_enabled = true;
    rast_state.depth.write_enabled = false;
    rast_state.depth.compare_op = unsigned(Ren::eCompareOp::Less);

    rast_state.blend.enabled = true;
    rast_state.blend.src_color = rast_state.blend.src_alpha = unsigned(Ren::eBlendFactor::SrcAlpha);
    rast_state.blend.dst_color = rast_state.blend.dst_alpha = unsigned(Ren::eBlendFactor::OneMinusSrcAlpha);

    // Bind main buffer for drawing
    glBindFramebuffer(GL_FRAMEBUFFER, transparent_draw_fb_[0][fb_to_use_].id());

    rast_state.viewport[2] = view_state_->act_res[0];
    rast_state.viewport[3] = view_state_->act_res[1];

    rast_state.ApplyChanged(builder.rast_state());
    builder.rast_state() = rast_state;

    glBindVertexArray(draw_pass_vi_.gl_vao());

    auto &ctx = builder.ctx();

    //
    // Bind resources (shadow atlas, lightmap, cells item data)
    //

    RpAllocBuf &textures_buf = builder.GetReadBuffer(textures_buf_);

    RpAllocTex &brdf_lut = builder.GetReadTexture(brdf_lut_);
    RpAllocTex &noise_tex = builder.GetReadTexture(noise_tex_);
    RpAllocTex &cone_rt_lut = builder.GetReadTexture(cone_rt_lut_);
    RpAllocTex &dummy_black = builder.GetReadTexture(dummy_black_);

    if (!(*p_list_)->probe_storage || (*p_list_)->alpha_blend_start_index == -1) {
        return;
    }

    glBindBufferBase(GL_UNIFORM_BUFFER, BIND_UB_SHARED_DATA_BUF, unif_shared_data_buf.ref->id());

    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, BIND_SHAD_TEX, shad_tex.ref->id());

    if ((*p_list_)->decals_atlas) {
        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, BIND_DECAL_TEX, (*p_list_)->decals_atlas->tex_id(0));
    }

    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, BIND_SSAO_TEX_SLOT, ssao_tex.ref->id());

    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, BIND_BRDF_LUT, brdf_lut.ref->id());

    if ((*p_list_)->render_settings.enable_lightmap && (*p_list_)->env.lm_direct) {
        for (int sh_l = 0; sh_l < 4; sh_l++) {
            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, BIND_LMAP_SH + sh_l, (*p_list_)->env.lm_indir_sh[sh_l]->id());
        }
    } else {
        for (int sh_l = 0; sh_l < 4; sh_l++) {
            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, BIND_LMAP_SH + sh_l, dummy_black.ref->id());
        }
    }

    ren_glBindTextureUnit_Comp(GL_TEXTURE_CUBE_MAP_ARRAY, BIND_ENV_TEX,
                               (*p_list_)->probe_storage ? (*p_list_)->probe_storage->handle().id : 0);

    ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, BIND_LIGHT_BUF, GLuint(lights_buf.tbos[0]->id()));
    ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, BIND_DECAL_BUF, GLuint(decals_buf.tbos[0]->id()));
    ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, BIND_CELLS_BUF, GLuint(cells_buf.tbos[0]->id()));
    ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, BIND_ITEMS_BUF, GLuint(items_buf.tbos[0]->id()));

    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, BIND_NOISE_TEX, noise_tex.ref->id());
    // ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, BIND_CONE_RT_LUT, cone_rt_lut.ref->id());

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BIND_MATERIALS_BUF, GLuint(materials_buf.ref->id()));
    if (ctx.capabilities.bindless_texture) {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BIND_BINDLESS_TEX, GLuint(textures_buf.ref->id()));
    }
    ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, BIND_INST_BUF, GLuint(instances_buf.tbos[0]->id()));
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BIND_INST_NDX_BUF, GLuint(instance_indices_buf.ref->id()));

    uint64_t cur_pipe_id = 0xffffffffffffffff;
    uint64_t cur_prog_id = 0xffffffffffffffff;
    uint64_t cur_mat_id = 0xffffffffffffffff;

    BackendInfo backend_info;

    for (int j = int((*p_list_)->custom_batch_indices.size()) - 1; j >= (*p_list_)->alpha_blend_start_index; j--) {
        const auto &batch = (*p_list_)->custom_batches[(*p_list_)->custom_batch_indices[j]];
        if (!batch.alpha_blend_bit || !batch.two_sided_bit) {
            continue;
        }

        if (!batch.instance_count) {
            continue;
        }

        if (batch.depth_write_bit) {
            rast_state.depth.write_enabled = true;
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;
        }

        if (cur_pipe_id != batch.pipe_id) {
            if (cur_prog_id != pipelines_[batch.pipe_id].prog().index()) {
                const Ren::Program *p = pipelines_[batch.pipe_id].prog().get();
                glUseProgram(p->id());

                cur_prog_id = pipelines_[batch.pipe_id].prog().index();
            }
        }

        if (!ctx.capabilities.bindless_texture && cur_mat_id != batch.mat_id) {
            const Ren::Material &mat = (*p_list_)->materials->at(batch.mat_id);
            _bind_textures_and_samplers(builder.ctx(), mat, builder.temp_samplers);
        }

        cur_pipe_id = batch.pipe_id;
        cur_mat_id = batch.mat_id;

        glUniform1ui(REN_U_BASE_INSTANCE_LOC, batch.instance_start);

        glDrawElementsInstancedBaseVertex(GL_TRIANGLES, batch.indices_count, GL_UNSIGNED_INT,
                                          (const GLvoid *)uintptr_t(batch.indices_offset * sizeof(uint32_t)),
                                          GLsizei(batch.instance_count), GLint(batch.base_vertex));

        backend_info.opaque_draw_calls_count += 2;
        backend_info.tris_rendered += (batch.indices_count / 3) * batch.instance_count;
    }

    rast_state.depth.write_enabled = false;
    rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);
    rast_state.ApplyChanged(builder.rast_state());
    builder.rast_state() = rast_state;

    for (int j = int((*p_list_)->custom_batch_indices.size()) - 1; j >= (*p_list_)->alpha_blend_start_index; j--) {
        const auto &batch = (*p_list_)->custom_batches[(*p_list_)->custom_batch_indices[j]];
        if (!batch.instance_count) {
            continue;
        }

        if (batch.depth_write_bit) {
            rast_state.depth.write_enabled = true;
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;
        }

        if (cur_pipe_id != batch.pipe_id) {
            if (cur_prog_id != pipelines_[batch.pipe_id].prog().index()) {
                const Ren::Program *p = pipelines_[batch.pipe_id].prog().get();
                glUseProgram(p->id());

                cur_prog_id = pipelines_[batch.pipe_id].prog().index();
            }
        }

        if (!ctx.capabilities.bindless_texture && cur_mat_id != batch.mat_id) {
            const Ren::Material &mat = (*p_list_)->materials->at(batch.mat_id);
            _bind_textures_and_samplers(builder.ctx(), mat, builder.temp_samplers);
        }

        cur_pipe_id = batch.pipe_id;
        cur_mat_id = batch.mat_id;

        glUniform1ui(REN_U_BASE_INSTANCE_LOC, batch.instance_start);

        glDrawElementsInstancedBaseVertex(GL_TRIANGLES, batch.indices_count, GL_UNSIGNED_INT,
                                          (const GLvoid *)uintptr_t(batch.indices_offset * sizeof(uint32_t)),
                                          GLsizei(batch.instance_count), GLint(batch.base_vertex));

        backend_info.opaque_draw_calls_count += 2;
        backend_info.tris_rendered += (batch.indices_count / 3) * batch.instance_count;
    }

    if (view_state_->is_multisampled) {
        /*Ren::DebugMarker _resolve_ms(ctx.api_ctx(), ctx.current_cmd_buf(), "RESOLVE MS BUFFER");

        Ren::RastState rast_state;
        rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

        rast_state.viewport[2] = view_state_->act_res[0];
        rast_state.viewport[3] = view_state_->act_res[1];

        rast_state.Apply();
        Ren::RastState applied_state = rast_state;

        const Ren::Binding bindings[] = {{Ren::eBindTarget::Tex2DMs, BIND_BASE0_TEX, *color_tex.ref}};

        const PrimDraw::Uniform uniforms[] = {
            {0, Ren::Vec4f{0.0f, 0.0f, float(view_state_->act_res[0]), float(view_state_->act_res[1])}}};

        prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, {&resolved_fb_, 0}, blit_ms_resolve_prog_.get(), bindings,
        uniforms);*/
    }
}

void Eng::RpTransparent::DrawTransparent_OIT_MomentBased(RpBuilder &builder) {
    using namespace RpSharedInternal;

#if 0
    Ren::RastState rast_state;
    rast_state.depth.test_enabled = true;
    rast_state.depth.write_enabled = false;
    rast_state.depth.compare_op = uint8_t(Ren::eCompareOp::LEqual);

    rast_state.blend.enabled = true;
    rast_state.blend.src = uint8_t(Ren::eBlendFactor::One);
    rast_state.blend.dst = uint8_t(Ren::eBlendFactor::One);

    rast_state.poly.cull = uint8_t(Ren::eCullFace::Front);
    rast_state.poly.mode = uint8_t(Ren::ePolygonMode::Fill);

    rast_state.viewport[2] = view_state_->act_res[0];
    rast_state.viewport[3] = view_state_->act_res[1];

    rast_state.ApplyChanged(builder.rast_state());
    builder.rast_state() = rast_state;

    glBindFramebuffer(GL_FRAMEBUFFER, moments_fb_.id());
    glClear(GL_COLOR_BUFFER_BIT);

    //
    // Bind resources (shadow atlas, lightmap, cells item data)
    //

    RpAllocBuf &unif_shared_data_buf = builder.GetReadBuffer(shared_data_buf_);

    glBindBufferBase(GL_UNIFORM_BUFFER, BIND_UB_SHARED_DATA_BUF, unif_shared_data_buf.ref->id());

    // ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, BIND_SHAD_TEX,
    // shadowmap_tex.ref->id());

    if (decals_atlas_) {
        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, BIND_DECAL_TEX, decals_atlas_->tex_id(0));
    }

    if ((render_flags_ & (EnableZFill | EnableSSAO)) == (EnableZFill | EnableSSAO)) {
        // ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, BIND_SSAO_TEX_SLOT, ssao_tex_.id);
    } else {
        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, BIND_SSAO_TEX_SLOT, dummy_white.ref->id());
    }

    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, BIND_BRDF_LUT, brdf_lut_->id());

    if ((render_flags_ & EnableLightmap) && env_->lm_direct) {
        for (int sh_l = 0; sh_l < 4; sh_l++) {
            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, BIND_LMAP_SH + sh_l, env_->lm_indir_sh[sh_l]->id());
        }
    } else {
        for (int sh_l = 0; sh_l < 4; sh_l++) {
            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, BIND_LMAP_SH + sh_l, dummy_black_->id());
        }
    }

    Ren::Context &ctx = builder.ctx();
    BackendInfo backend_info;

    { // Draw alpha-blended surfaces
        Ren::DebugMarker _(ctx.current_cmd_buf(), "MOMENTS GENERATION PASS");

        const Ren::Program *cur_program = nullptr;
        const Ren::Material *cur_mat = nullptr;

        for (int j = int(main_batch_indices_.count) - 1; j >= (*alpha_blend_start_index_); j--) {
            const MainDrawBatch &batch = main_batches_.data[main_batch_indices_.data[j]];
            if (!batch.instance_count) {
                continue;
            }
            if (!batch.alpha_blend_bit) {
                break;
            }

            const Ren::Program *p = pipelines_[batch.pipe_id].prog().get();
            const Ren::Material &mat = materials_->at(batch.mat_id);

            if (cur_program != p) {
                glUseProgram(p->id());
                cur_program = p;
            }

            if (!ctx.capabilities.bindless_texture && cur_mat != &mat) {
                _bind_texture0_and_sampler0(builder.ctx(), mat, builder.temp_samplers);
                cur_mat = &mat;
            }

            glUniform2iv(REN_U_INSTANCES_LOC, batch.instance_count, &batch.instance_indices[0][0]);

            glDrawElementsInstancedBaseVertex(GL_TRIANGLES, batch.indices_count, GL_UNSIGNED_INT,
                                              (const GLvoid *)uintptr_t(batch.indices_offset * sizeof(uint32_t)),
                                              GLsizei(batch.instance_count), GLint(batch.base_vertex));

            backend_info.opaque_draw_calls_count += 2;
            backend_info.tris_rendered += (batch.indices_count / 3) * batch.instance_count;
        }
    }

    { // Change transparency draw mode
        glBindBuffer(GL_UNIFORM_BUFFER, (GLuint)unif_shared_data_buf.ref->id());
        const float transp_mode = view_state_->is_multisampled ? 4.0f : 3.0f;
        glBufferSubData(GL_UNIFORM_BUFFER, offsetof(SharedDataBlock, uTranspParamsAndTime) + 2 * sizeof(float),
                        sizeof(float), &transp_mode);
    }

    const uint32_t target_framebuf = view_state_->is_multisampled ? color_only_fb_.id() : resolved_fb_.id();

    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)target_framebuf);

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    /*if (view_state_->is_multisampled) {
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
    }*/

    { // Draw alpha-blended surfaces
        Ren::DebugMarker _(ctx.current_cmd_buf(), "COLOR PASS");

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

            const Ren::Program *p = pipelines_[batch.pipe_id].prog().get();
            const Ren::Material &mat = materials_->at(batch.mat_id);

            if (cur_program != p) {
                glUseProgram(p->id());
                cur_program = p;
            }

            if (!ctx.capabilities.bindless_texture && cur_mat != &mat) {
                _bind_texture0_and_sampler0(builder.ctx(), mat, builder.temp_samplers);
                cur_mat = &mat;
            }

            glUniform2iv(REN_U_INSTANCES_LOC, batch.instance_count, &batch.instance_indices[0][0]);

            glDrawElementsInstancedBaseVertex(GL_TRIANGLES, batch.indices_count, GL_UNSIGNED_INT,
                                              (const GLvoid *)uintptr_t(batch.indices_offset * sizeof(uint32_t)),
                                              GLsizei(batch.instance_count), GLint(batch.base_vertex));

            backend_info.opaque_draw_calls_count += 2;
            backend_info.tris_rendered += (batch.indices_count / 3) * batch.instance_count;
        }
    }
#endif

    Ren::GLUnbindSamplers(BIND_MAT_TEX0, 8);
}

void Eng::RpTransparent::DrawTransparent_OIT_WeightedBlended(RpBuilder &builder) {}

//
// This is needed for moment-based OIT
//

#if 0

#if (OIT_MODE == OIT_MOMENT_BASED)
{ // Buffer that holds moments (used for transparency)
    FrameBuf::ColorAttachmentDesc desc[3];
    { // b0
        desc[0].format = Ren::eTexFormat::RawR32F;
        desc[0].filter = Ren::eTexFilter::NoFilter;
        desc[0].repeat = Ren::eTexRepeat::ClampToEdge;
    }
    { // z and z^2
        desc[1].format = Ren::eTexFormat::RawRG16F;
        desc[1].filter = Ren::eTexFilter::NoFilter;
        desc[1].repeat = Ren::eTexRepeat::ClampToEdge;
    }
    { // z^3 and z^4
        desc[2].format = Ren::eTexFormat::RawRG16F;
        desc[2].filter = Ren::eTexFilter::NoFilter;
        desc[2].repeat = Ren::eTexRepeat::ClampToEdge;
    }
    moments_buf_ =
        FrameBuf("Moments buf", ctx_, cur_scr_w, cur_scr_h, desc, 3,
            { Ren::eTexFormat::None }, clean_buf_.sample_count, log);
}
#endif

#if (OIT_MODE == OIT_MOMENT_BASED)
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

#if (OIT_MODE != OIT_DISABLED)
if (list.render_flags & EnableOIT) {
    DebugMarker _("COMPOSE TRANSPARENT");

    glEnable(GL_BLEND);
#if (OIT_MODE == OIT_WEIGHTED_BLENDED) || (OIT_MODE == OIT_MOMENT_BASED && OIT_MOMENT_RENORMALIZE)
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
        //    GL_TEXTURE_2D_MULTISAMPLE, BIND_BASE0_TEX,
        //    clean_buf_.attachments[LOC_OUT_COLOR].tex->id());

        bindings[0] = { Ren::eBindTarget::Tex2DMs, BIND_BASE0_TEX,
                       clean_buf_.attachments[LOC_OUT_COLOR].tex->handle() };

#if OIT_MODE == OIT_MOMENT_BASED
        // ren_glBindTextureUnit_Comp(GL_TEXTURE_2D_MULTISAMPLE, BIND_BASE1_TEX,
        //                           moments_buf_.attachments[0].tex->id());

        bindings[1] = { Ren::eBindTarget::Tex2DMs, BIND_BASE1_TEX,
                       moments_buf_.attachments[0].tex->handle() };
#elif OIT_MODE == OIT_WEIGHTED_BLENDED
        // ren_glBindTextureUnit_Comp(GL_TEXTURE_2D_MULTISAMPLE, BIND_BASE1_TEX,
        //                           clean_buf_.attachments[LOC_OUT_NORM].tex);

        bindings[1] = { Ren::eBindTarget::Tex2DMs, BIND_BASE1_TEX,
                       clean_buf_.attachments[LOC_OUT_NORM].tex->handle() };
#endif
    } else {
        // ren_glBindTextureUnit_Comp(
        //   GL_TEXTURE_2D, BIND_BASE0_TEX,
        //    resolved_or_transparent_buf_.attachments[0].tex->id());

        bindings[0] = { Ren::eBindTarget::Tex2D, BIND_BASE0_TEX,
                       resolved_or_transparent_buf_.attachments[0].tex->handle() };
#if OIT_MODE == OIT_MOMENT_BASED
        // ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, BIND_BASE1_TEX,
        //                           moments_buf_.attachments[0].tex->id());

        bindings[1] = { Ren::eBindTarget::Tex2D, BIND_BASE1_TEX,
                       moments_buf_.attachments[0].tex->handle() };
#elif OIT_MODE == OIT_WEIGHTED_BLENDED
        // ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, BIND_BASE1_TEX,
        //                           clean_buf_.attachments[LOC_OUT_NORM].tex);

        bindings[1] = { Ren::eBindTarget::Tex2D, BIND_BASE1_TEX,
                       clean_buf_.attachments[LOC_OUT_NORM].tex->handle() };
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

Eng::RpTransparent::~RpTransparent() {}