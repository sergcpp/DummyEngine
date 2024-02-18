#include "RpDepthFill.h"

#include "../Renderer_Structs.h"

#include <Ren/Context.h>
#include <Ren/DebugMarker.h>
#include <Ren/RastState.h>

namespace RpSharedInternal {
void _bind_texture3_and_sampler3(Ren::Context &ctx, const Ren::Material &mat,
                                 Ren::SmallVectorImpl<Ren::SamplerRef> &temp_samplers) {
    assert(mat.textures.size() >= 1 && mat.samplers.size() >= 1);
    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, Eng::BIND_MAT_TEX3, mat.textures[3]->id());
    glBindSampler(Eng::BIND_MAT_TEX3, mat.samplers[3]->id());
}
uint32_t _draw_range(Ren::Span<const uint32_t> zfill_batch_indices, Ren::Span<const Eng::BasicDrawBatch> zfill_batches,
                     uint32_t i, uint32_t mask, int *draws_count) {
    for (; i < zfill_batch_indices.size(); i++) {
        const auto &batch = zfill_batches[zfill_batch_indices[i]];
        if ((batch.sort_key & Eng::BasicDrawBatch::FlagBits) != mask) {
            break;
        }

        if (!batch.instance_count) {
            continue;
        }

        glUniform1ui(Eng::REN_U_BASE_INSTANCE_LOC, batch.instance_start);

        glDrawElementsInstancedBaseVertex(GL_TRIANGLES, batch.indices_count, GL_UNSIGNED_INT,
                                          (const GLvoid *)uintptr_t(batch.indices_offset * sizeof(uint32_t)),
                                          GLsizei(batch.instance_count), GLint(batch.base_vertex));
        ++(*draws_count);
    }
    return i;
}

uint32_t _draw_range_ext(Eng::RpBuilder &builder, const Ren::MaterialStorage *materials,
                         Ren::Span<const uint32_t> batch_indices, Ren::Span<const Eng::BasicDrawBatch> batches,
                         uint32_t i, uint32_t mask, uint32_t &cur_mat_id, int *draws_count) {
    auto &ctx = builder.ctx();

    for (; i < batch_indices.size(); i++) {
        const auto &batch = batches[batch_indices[i]];
        if ((batch.sort_key & Eng::BasicDrawBatch::FlagBits) != mask) {
            break;
        }

        if (!batch.instance_count) {
            continue;
        }

        if (!ctx.capabilities.bindless_texture && batch.material_index != cur_mat_id) {
            const Ren::Material &mat = materials->at(batch.material_index);
            _bind_texture3_and_sampler3(builder.ctx(), mat, builder.temp_samplers);
            cur_mat_id = batch.material_index;
        }

        glUniform1ui(Eng::REN_U_BASE_INSTANCE_LOC, batch.instance_start);

        glDrawElementsInstancedBaseVertex(GL_TRIANGLES, batch.indices_count, GL_UNSIGNED_INT,
                                          (const GLvoid *)uintptr_t(batch.indices_offset * sizeof(uint32_t)),
                                          GLsizei(batch.instance_count), GLint(batch.base_vertex));
        ++(*draws_count);
    }
    return i;
}
} // namespace RpSharedInternal

void Eng::RpDepthFill::DrawDepth(RpBuilder &builder, RpAllocBuf &vtx_buf1, RpAllocBuf &vtx_buf2, RpAllocBuf &ndx_buf) {
    using namespace RpSharedInternal;

    auto &ctx = builder.ctx();

    RpAllocBuf &unif_shared_data_buf = builder.GetReadBuffer(shared_data_buf_);
    RpAllocBuf &instances_buf = builder.GetReadBuffer(instances_buf_);
    RpAllocBuf &instance_indices_buf = builder.GetReadBuffer(instance_indices_buf_);
    RpAllocBuf &materials_buf = builder.GetReadBuffer(materials_buf_);
    RpAllocBuf &textures_buf = builder.GetReadBuffer(textures_buf_);
    RpAllocTex &noise_tex = builder.GetReadTexture(noise_tex_);

    glBindBufferBase(GL_UNIFORM_BUFFER, BIND_UB_SHARED_DATA_BUF, GLuint(unif_shared_data_buf.ref->id()));

    assert(instances_buf.tbos[0]);
    ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, BIND_INST_BUF, GLuint(instances_buf.tbos[0]->id()));
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BIND_INST_NDX_BUF, GLuint(instance_indices_buf.ref->id()));

    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, BIND_NOISE_TEX, noise_tex.ref->id());

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BIND_MATERIALS_BUF, GLuint(materials_buf.ref->id()));
    if (ctx.capabilities.bindless_texture) {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BIND_BINDLESS_TEX, GLuint(textures_buf.ref->id()));
    }

    glBindFramebuffer(GL_FRAMEBUFFER, GLuint(depth_fill_fb_[ctx.backend_frame()][fb_to_use_].id()));
    if (clear_depth_) {
        glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    } else {
        glClear(GL_STENCIL_BUFFER_BIT);
    }

    const Ren::Span<const BasicDrawBatch> zfill_batches = {(*p_list_)->basic_batches.data,
                                                           (*p_list_)->basic_batches.count};
    const Ren::Span<const uint32_t> zfill_batch_indices = {(*p_list_)->basic_batch_indices.data,
                                                           (*p_list_)->basic_batch_indices.count};

    int draws_count = 0;
    uint32_t i = 0;

    using BDB = BasicDrawBatch;

    { // solid meshes
        Ren::DebugMarker _m(ctx.api_ctx(), ctx.current_cmd_buf(), "STATIC-SOLID-SIMPLE");

        glBindVertexArray(vi_solid_.gl_vao());
        glUseProgram(pi_static_solid_[0].prog()->id());

        { // one-sided
            Ren::DebugMarker _mm(ctx.api_ctx(), ctx.current_cmd_buf(), "ONE-SIDED");

            Ren::RastState rast_state = pi_static_solid_[0].rast_state();
            rast_state.viewport[2] = view_state_->act_res[0];
            rast_state.viewport[3] = view_state_->act_res[1];
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            i = _draw_range(zfill_batch_indices, zfill_batches, i, 0u, &draws_count);
            i = _draw_range(zfill_batch_indices, zfill_batches, i, BDB::BitCustomShaded, &draws_count);
        }

        { // two-sided
            Ren::DebugMarker _mm(ctx.api_ctx(), ctx.current_cmd_buf(), "TWO-SIDED");

            Ren::RastState rast_state = pi_static_solid_[1].rast_state();
            rast_state.viewport[2] = view_state_->act_res[0];
            rast_state.viewport[3] = view_state_->act_res[1];
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            i = _draw_range(zfill_batch_indices, zfill_batches, i, BDB::BitTwoSided, &draws_count);
            i = _draw_range(zfill_batch_indices, zfill_batches, i, BDB::BitTwoSided | BDB::BitCustomShaded,
                            &draws_count);
        }
    }

    // TODO: we can skip many things if TAA is disabled

    { // moving solid meshes (depth and velocity)
        Ren::DebugMarker _m(ctx.api_ctx(), ctx.current_cmd_buf(), "STATIC-SOLID-MOVING");

        if (((*p_list_)->render_flags & EnableTaa) != 0) {
            // Write depth and velocity
            glBindFramebuffer(GL_FRAMEBUFFER, depth_fill_vel_fb_[ctx.backend_frame()][fb_to_use_].id());
            glUseProgram(pi_moving_solid_[0].prog()->id());
        } else {
            // Write depth only
            glBindFramebuffer(GL_FRAMEBUFFER, depth_fill_fb_[ctx.backend_frame()][fb_to_use_].id());
        }

        { // one-sided
            Ren::DebugMarker _mm(ctx.api_ctx(), ctx.current_cmd_buf(), "ONE-SIDED");

            Ren::RastState rast_state = pi_moving_solid_[0].rast_state();
            rast_state.viewport[2] = view_state_->act_res[0];
            rast_state.viewport[3] = view_state_->act_res[1];
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            i = _draw_range(zfill_batch_indices, zfill_batches, i, BDB::BitMoving, &draws_count);
            i = _draw_range(zfill_batch_indices, zfill_batches, i, BDB::BitMoving | BDB::BitCustomShaded, &draws_count);
        }

        { // two-sided
            Ren::DebugMarker _mm(ctx.api_ctx(), ctx.current_cmd_buf(), "TWO-SIDED");

            Ren::RastState rast_state = pi_moving_solid_[1].rast_state();
            rast_state.viewport[2] = view_state_->act_res[0];
            rast_state.viewport[3] = view_state_->act_res[1];
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            const uint32_t DrawMask = BDB::BitMoving | BDB::BitTwoSided;
            i = _draw_range(zfill_batch_indices, zfill_batches, i, DrawMask, &draws_count);
            i = _draw_range(zfill_batch_indices, zfill_batches, i, DrawMask | BDB::BitCustomShaded, &draws_count);
        }
    }

    { // alpha-tested objects
        uint32_t cur_mat_id = 0xffffffff;

        { // simple meshes (depth only)
            Ren::DebugMarker _m(ctx.api_ctx(), ctx.current_cmd_buf(), "STATIC-ALPHA-SIMPLE");

            glBindFramebuffer(GL_FRAMEBUFFER, depth_fill_fb_[ctx.backend_frame()][fb_to_use_].id());
            glBindVertexArray(vi_transp_.gl_vao());
            glUseProgram(pi_static_transp_[0].prog()->id());

            { // one-sided
                Ren::DebugMarker _mm(ctx.api_ctx(), ctx.current_cmd_buf(), "ONE-SIDED");

                Ren::RastState rast_state = pi_static_transp_[0].rast_state();
                rast_state.viewport[2] = view_state_->act_res[0];
                rast_state.viewport[3] = view_state_->act_res[1];
                rast_state.ApplyChanged(builder.rast_state());
                builder.rast_state() = rast_state;

                i = _draw_range_ext(builder, (*p_list_)->materials, zfill_batch_indices, zfill_batches, i,
                                    BDB::BitAlphaTest, cur_mat_id, &draws_count);
                i = _draw_range_ext(builder, (*p_list_)->materials, zfill_batch_indices, zfill_batches, i,
                                    BDB::BitAlphaTest | BDB::BitCustomShaded, cur_mat_id, &draws_count);
            }

            { // two-sided
                Ren::DebugMarker _mm(ctx.api_ctx(), ctx.current_cmd_buf(), "TWO-SIDED");

                Ren::RastState rast_state = pi_static_transp_[1].rast_state();
                rast_state.viewport[2] = view_state_->act_res[0];
                rast_state.viewport[3] = view_state_->act_res[1];
                rast_state.ApplyChanged(builder.rast_state());
                builder.rast_state() = rast_state;

                const uint32_t DrawMask = BDB::BitAlphaTest | BDB::BitTwoSided;
                i = _draw_range_ext(builder, (*p_list_)->materials, zfill_batch_indices, zfill_batches, i, DrawMask,
                                    cur_mat_id, &draws_count);
                i = _draw_range_ext(builder, (*p_list_)->materials, zfill_batch_indices, zfill_batches, i,
                                    DrawMask | BDB::BitCustomShaded, cur_mat_id, &draws_count);
            }
        }

        { // moving meshes (depth and velocity)
            Ren::DebugMarker _m(ctx.api_ctx(), ctx.current_cmd_buf(), "STATIC-ALPHA-MOVING");

            if (((*p_list_)->render_flags & EnableTaa) != 0) {
                // Write depth and velocity
                glBindFramebuffer(GL_FRAMEBUFFER, depth_fill_vel_fb_[ctx.backend_frame()][fb_to_use_].id());
                glUseProgram(pi_moving_transp_[0].prog()->id());
            } else {
                // Write depth only
                glBindFramebuffer(GL_FRAMEBUFFER, depth_fill_fb_[ctx.backend_frame()][fb_to_use_].id());
            }

            { // one-sided
                Ren::DebugMarker _mm(ctx.api_ctx(), ctx.current_cmd_buf(), "ONE-SIDED");

                Ren::RastState rast_state = pi_moving_transp_[0].rast_state();
                rast_state.viewport[2] = view_state_->act_res[0];
                rast_state.viewport[3] = view_state_->act_res[1];
                rast_state.ApplyChanged(builder.rast_state());
                builder.rast_state() = rast_state;

                const uint32_t DrawMask = BDB::BitAlphaTest | BDB::BitMoving;
                i = _draw_range_ext(builder, (*p_list_)->materials, zfill_batch_indices, zfill_batches, i, DrawMask,
                                    cur_mat_id, &draws_count);
                i = _draw_range_ext(builder, (*p_list_)->materials, zfill_batch_indices, zfill_batches, i,
                                    DrawMask | BDB::BitCustomShaded, cur_mat_id, &draws_count);
            }

            { // two-sided
                Ren::DebugMarker _mm(ctx.api_ctx(), ctx.current_cmd_buf(), "TWO-SIDED");

                Ren::RastState rast_state = pi_moving_transp_[1].rast_state();
                rast_state.viewport[2] = view_state_->act_res[0];
                rast_state.viewport[3] = view_state_->act_res[1];
                rast_state.ApplyChanged(builder.rast_state());
                builder.rast_state() = rast_state;

                const uint32_t DrawMask = BDB::BitAlphaTest | BDB::BitMoving | BDB::BitTwoSided;
                i = _draw_range_ext(builder, (*p_list_)->materials, zfill_batch_indices, zfill_batches, i, DrawMask,
                                    cur_mat_id, &draws_count);
                i = _draw_range_ext(builder, (*p_list_)->materials, zfill_batch_indices, zfill_batches, i,
                                    DrawMask | BDB::BitCustomShaded, cur_mat_id, &draws_count);
            }
        }
    }

    { // static solid vegetation
        Ren::DebugMarker _m(ctx.api_ctx(), ctx.current_cmd_buf(), "VEGE-SOLID-SIMPLE");
        glBindVertexArray(vi_vege_solid_.gl_vao());
        if (((*p_list_)->render_flags & EnableTaa) != 0) {
            // Write depth and velocity
            glBindFramebuffer(GL_FRAMEBUFFER, depth_fill_vel_fb_[ctx.backend_frame()][fb_to_use_].id());
            glUseProgram(pi_vege_static_solid_vel_[0].prog()->id());
        } else {
            // Write depth only
            glBindFramebuffer(GL_FRAMEBUFFER, depth_fill_fb_[ctx.backend_frame()][fb_to_use_].id());
            glUseProgram(pi_vege_static_solid_[0].prog()->id());
        }

        { // one-sided
            Ren::DebugMarker _mm(ctx.api_ctx(), ctx.current_cmd_buf(), "ONE-SIDED");

            Ren::RastState rast_state = pi_vege_static_solid_vel_[0].rast_state();
            rast_state.viewport[2] = view_state_->act_res[0];
            rast_state.viewport[3] = view_state_->act_res[1];
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            i = _draw_range(zfill_batch_indices, zfill_batches, i, BDB::BitsVege, &draws_count);
            i = _draw_range(zfill_batch_indices, zfill_batches, i, BDB::BitsVege | BDB::BitCustomShaded, &draws_count);
        }

        { // two-sided
            Ren::DebugMarker _mm(ctx.api_ctx(), ctx.current_cmd_buf(), "TWO-SIDED");

            Ren::RastState rast_state = pi_vege_static_solid_vel_[1].rast_state();
            rast_state.viewport[2] = view_state_->act_res[0];
            rast_state.viewport[3] = view_state_->act_res[1];
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            const uint32_t DrawMask = BDB::BitsVege | BDB::BitTwoSided;
            i = _draw_range(zfill_batch_indices, zfill_batches, i, DrawMask, &draws_count);
            i = _draw_range(zfill_batch_indices, zfill_batches, i, DrawMask | BDB::BitCustomShaded, &draws_count);
        }
    }

    { // moving solid vegetation (depth and velocity)
        Ren::DebugMarker _m(ctx.api_ctx(), ctx.current_cmd_buf(), "VEGE-SOLID-MOVING");
        if (((*p_list_)->render_flags & EnableTaa) != 0) {
            glUseProgram(pi_vege_moving_solid_vel_[0].prog()->id());
        } else {
            glUseProgram(pi_vege_static_solid_[0].prog()->id());
        }

        { // one-sided
            Ren::DebugMarker _mm(ctx.api_ctx(), ctx.current_cmd_buf(), "ONE-SIDED");

            Ren::RastState rast_state = pi_vege_moving_solid_vel_[0].rast_state();
            rast_state.viewport[2] = view_state_->act_res[0];
            rast_state.viewport[3] = view_state_->act_res[1];
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            const uint32_t DrawMask = BDB::BitsVege | BDB::BitMoving;
            i = _draw_range(zfill_batch_indices, zfill_batches, i, DrawMask, &draws_count);
            i = _draw_range(zfill_batch_indices, zfill_batches, i, DrawMask | BDB::BitCustomShaded, &draws_count);
        }

        { // two-sided
            Ren::DebugMarker _mm(ctx.api_ctx(), ctx.current_cmd_buf(), "TWO-SIDED");

            Ren::RastState rast_state = pi_vege_moving_solid_vel_[1].rast_state();
            rast_state.viewport[2] = view_state_->act_res[0];
            rast_state.viewport[3] = view_state_->act_res[1];
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            const uint32_t DrawMask = BDB::BitsVege | BDB::BitMoving | BDB::BitTwoSided;
            i = _draw_range(zfill_batch_indices, zfill_batches, i, DrawMask, &draws_count);
            i = _draw_range(zfill_batch_indices, zfill_batches, i, DrawMask | BDB::BitCustomShaded, &draws_count);
        }
    }

    { // static alpha-tested vegetation
        uint32_t cur_mat_id = 0xffffffff;

        { // static alpha-tested vegetation (depth and velocity)
            Ren::DebugMarker _m(ctx.api_ctx(), ctx.current_cmd_buf(), "VEGE-ALPHA-SIMPLE");
            glBindVertexArray(vi_vege_transp_.gl_vao());
            if (((*p_list_)->render_flags & EnableTaa) != 0) {
                glUseProgram(pi_vege_static_transp_vel_[0].prog()->id());
            } else {
                glUseProgram(pi_vege_static_transp_[0].prog()->id());
            }

            { // one-sided
                Ren::DebugMarker _mm(ctx.api_ctx(), ctx.current_cmd_buf(), "ONE-SIDED");

                Ren::RastState rast_state = pi_vege_static_transp_vel_[0].rast_state();
                rast_state.viewport[2] = view_state_->act_res[0];
                rast_state.viewport[3] = view_state_->act_res[1];
                rast_state.ApplyChanged(builder.rast_state());
                builder.rast_state() = rast_state;

                const uint32_t DrawMask = BDB::BitsVege | BDB::BitAlphaTest;
                i = _draw_range_ext(builder, (*p_list_)->materials, zfill_batch_indices, zfill_batches, i, DrawMask,
                                    cur_mat_id, &draws_count);
                i = _draw_range_ext(builder, (*p_list_)->materials, zfill_batch_indices, zfill_batches, i,
                                    DrawMask | BDB::BitCustomShaded, cur_mat_id, &draws_count);
            }

            { // two-sided
                Ren::DebugMarker _mm(ctx.api_ctx(), ctx.current_cmd_buf(), "TWO-SIDED");

                Ren::RastState rast_state = pi_vege_static_transp_vel_[1].rast_state();
                rast_state.viewport[2] = view_state_->act_res[0];
                rast_state.viewport[3] = view_state_->act_res[1];
                rast_state.ApplyChanged(builder.rast_state());
                builder.rast_state() = rast_state;

                const uint32_t DrawMask = BDB::BitsVege | BDB::BitAlphaTest | BDB::BitTwoSided;
                i = _draw_range_ext(builder, (*p_list_)->materials, zfill_batch_indices, zfill_batches, i, DrawMask,
                                    cur_mat_id, &draws_count);
                i = _draw_range_ext(builder, (*p_list_)->materials, zfill_batch_indices, zfill_batches, i,
                                    DrawMask | BDB::BitCustomShaded, cur_mat_id, &draws_count);
            }
        }

        { // moving alpha-tested vegetation (depth and velocity)
            Ren::DebugMarker _m(ctx.api_ctx(), ctx.current_cmd_buf(), "VEGE-ALPHA-MOVING");
            if (((*p_list_)->render_flags & EnableTaa) != 0) {
                glUseProgram(pi_vege_moving_transp_vel_[0].prog()->id());
            } else {
                glUseProgram(pi_vege_static_transp_[0].prog()->id());
            }

            { // one-sided
                Ren::DebugMarker _mm(ctx.api_ctx(), ctx.current_cmd_buf(), "ONE-SIDED");

                Ren::RastState rast_state = pi_vege_moving_transp_vel_[0].rast_state();
                rast_state.viewport[2] = view_state_->act_res[0];
                rast_state.viewport[3] = view_state_->act_res[1];
                rast_state.ApplyChanged(builder.rast_state());
                builder.rast_state() = rast_state;

                const uint32_t DrawMask = BDB::BitsVege | BDB::BitAlphaTest | BDB::BitMoving;
                i = _draw_range_ext(builder, (*p_list_)->materials, zfill_batch_indices, zfill_batches, i, DrawMask,
                                    cur_mat_id, &draws_count);
                i = _draw_range_ext(builder, (*p_list_)->materials, zfill_batch_indices, zfill_batches, i,
                                    DrawMask | BDB::BitCustomShaded, cur_mat_id, &draws_count);
            }

            { // two-sided
                Ren::DebugMarker _mm(ctx.api_ctx(), ctx.current_cmd_buf(), "TWO-SIDED");

                Ren::RastState rast_state = pi_vege_moving_transp_vel_[1].rast_state();
                rast_state.viewport[2] = view_state_->act_res[0];
                rast_state.viewport[3] = view_state_->act_res[1];
                rast_state.ApplyChanged(builder.rast_state());
                builder.rast_state() = rast_state;

                const uint32_t DrawMask = BDB::BitsVege | BDB::BitAlphaTest | BDB::BitMoving | BDB::BitTwoSided;
                i = _draw_range_ext(builder, (*p_list_)->materials, zfill_batch_indices, zfill_batches, i, DrawMask,
                                    cur_mat_id, &draws_count);
                i = _draw_range_ext(builder, (*p_list_)->materials, zfill_batch_indices, zfill_batches, i,
                                    DrawMask | BDB::BitCustomShaded, cur_mat_id, &draws_count);
            }
        }
    }

    { // solid skinned meshes (depth and velocity)
        Ren::DebugMarker _m(ctx.api_ctx(), ctx.current_cmd_buf(), "SKIN-SOLID-SIMPLE");
        if (((*p_list_)->render_flags & EnableTaa) != 0) {
            glBindVertexArray(vi_skin_solid_.gl_vao());
            glUseProgram(pi_skin_static_solid_vel_[0].prog()->id());
        } else {
            glBindVertexArray(vi_solid_.gl_vao());
            glUseProgram(pi_static_solid_[0].prog()->id());
        }

        { // one-sided
            Ren::DebugMarker _mm(ctx.api_ctx(), ctx.current_cmd_buf(), "ONE-SIDED");

            Ren::RastState rast_state = pi_skin_static_solid_vel_[0].rast_state();
            rast_state.viewport[2] = view_state_->act_res[0];
            rast_state.viewport[3] = view_state_->act_res[1];
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            i = _draw_range(zfill_batch_indices, zfill_batches, i, BDB::BitsSkinned, &draws_count);
            i = _draw_range(zfill_batch_indices, zfill_batches, i, BDB::BitsSkinned | BDB::BitCustomShaded,
                            &draws_count);
        }

        { // two-sided
            Ren::DebugMarker _mm(ctx.api_ctx(), ctx.current_cmd_buf(), "TWO-SIDED");

            Ren::RastState rast_state = pi_skin_static_solid_vel_[1].rast_state();
            rast_state.viewport[2] = view_state_->act_res[0];
            rast_state.viewport[3] = view_state_->act_res[1];
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            const uint32_t DrawMask = BDB::BitsSkinned | BDB::BitTwoSided;
            i = _draw_range(zfill_batch_indices, zfill_batches, i, DrawMask, &draws_count);
            i = _draw_range(zfill_batch_indices, zfill_batches, i, DrawMask | BDB::BitCustomShaded, &draws_count);
        }
    }

    { // moving solid skinned (depth and velocity)
        Ren::DebugMarker _m(ctx.api_ctx(), ctx.current_cmd_buf(), "SKIN-SOLID-MOVING");
        if (((*p_list_)->render_flags & EnableTaa) != 0) {
            glUseProgram(pi_skin_moving_solid_vel_[0].prog()->id());
        } else {
            glUseProgram(pi_skin_static_solid_[0].prog()->id());
        }

        { // one-sided
            Ren::DebugMarker _mm(ctx.api_ctx(), ctx.current_cmd_buf(), "ONE-SIDED");

            Ren::RastState rast_state = pi_skin_moving_solid_vel_[0].rast_state();
            rast_state.viewport[2] = view_state_->act_res[0];
            rast_state.viewport[3] = view_state_->act_res[1];
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            const uint32_t DrawMask = BDB::BitsSkinned | BDB::BitMoving;
            i = _draw_range(zfill_batch_indices, zfill_batches, i, DrawMask, &draws_count);
            i = _draw_range(zfill_batch_indices, zfill_batches, i, DrawMask | BDB::BitCustomShaded, &draws_count);
        }

        { // two-sided
            Ren::DebugMarker _mm(ctx.api_ctx(), ctx.current_cmd_buf(), "TWO-SIDED");

            Ren::RastState rast_state = pi_skin_moving_solid_vel_[1].rast_state();
            rast_state.viewport[2] = view_state_->act_res[0];
            rast_state.viewport[3] = view_state_->act_res[1];
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            const uint32_t DrawMask = BDB::BitsSkinned | BDB::BitMoving | BDB::BitTwoSided;
            i = _draw_range(zfill_batch_indices, zfill_batches, i, DrawMask, &draws_count);
            i = _draw_range(zfill_batch_indices, zfill_batches, i, DrawMask | BDB::BitCustomShaded, &draws_count);
        }
    }

    { // static alpha-tested skinned
        uint32_t cur_mat_id = 0xffffffff;

        { // simple alpha-tested skinned (depth and velocity)
            Ren::DebugMarker _m(ctx.api_ctx(), ctx.current_cmd_buf(), "SKIN-ALPHA-SIMPLE");
            glBindVertexArray(vi_skin_transp_.gl_vao());
            if (((*p_list_)->render_flags & EnableTaa) != 0) {
                glUseProgram(pi_skin_static_transp_vel_[0].prog()->id());
            } else {
                glUseProgram(pi_skin_static_transp_[0].prog()->id());
            }

            { // one-sided
                Ren::DebugMarker _mm(ctx.api_ctx(), ctx.current_cmd_buf(), "ONE-SIDED");

                Ren::RastState rast_state = pi_skin_static_transp_vel_[0].rast_state();
                rast_state.viewport[2] = view_state_->act_res[0];
                rast_state.viewport[3] = view_state_->act_res[1];
                rast_state.ApplyChanged(builder.rast_state());
                builder.rast_state() = rast_state;

                const uint32_t DrawMask = BDB::BitsSkinned | BDB::BitAlphaTest;
                i = _draw_range_ext(builder, (*p_list_)->materials, zfill_batch_indices, zfill_batches, i, DrawMask,
                                    cur_mat_id, &draws_count);
                i = _draw_range_ext(builder, (*p_list_)->materials, zfill_batch_indices, zfill_batches, i,
                                    DrawMask | BDB::BitCustomShaded, cur_mat_id, &draws_count);
            }

            { // two-sided
                Ren::DebugMarker _mm(ctx.api_ctx(), ctx.current_cmd_buf(), "TWO-SIDED");

                Ren::RastState rast_state = pi_skin_static_transp_vel_[1].rast_state();
                rast_state.viewport[2] = view_state_->act_res[0];
                rast_state.viewport[3] = view_state_->act_res[1];
                rast_state.ApplyChanged(builder.rast_state());
                builder.rast_state() = rast_state;

                const uint32_t DrawMask = BDB::BitsSkinned | BDB::BitAlphaTest | BDB::BitTwoSided;
                i = _draw_range_ext(builder, (*p_list_)->materials, zfill_batch_indices, zfill_batches, i, DrawMask,
                                    cur_mat_id, &draws_count);
                i = _draw_range_ext(builder, (*p_list_)->materials, zfill_batch_indices, zfill_batches, i,
                                    DrawMask | BDB::BitCustomShaded, cur_mat_id, &draws_count);
            }
        }

        { // moving alpha-tested skinned (depth and velocity)
            Ren::DebugMarker _m(ctx.api_ctx(), ctx.current_cmd_buf(), "SKIN-ALPHA-MOVING");
            if (((*p_list_)->render_flags & EnableTaa) != 0) {
                glUseProgram(pi_skin_moving_transp_vel_[0].prog()->id());
            } else {
                glUseProgram(pi_skin_static_transp_[0].prog()->id());
            }

            { // one-sided
                Ren::DebugMarker _mm(ctx.api_ctx(), ctx.current_cmd_buf(), "ONE-SIDED");

                Ren::RastState rast_state = pi_skin_moving_transp_vel_[0].rast_state();
                rast_state.viewport[2] = view_state_->act_res[0];
                rast_state.viewport[3] = view_state_->act_res[1];
                rast_state.ApplyChanged(builder.rast_state());
                builder.rast_state() = rast_state;

                const uint32_t DrawMask = BDB::BitsSkinned | BDB::BitAlphaTest | BDB::BitMoving;
                i = _draw_range_ext(builder, (*p_list_)->materials, zfill_batch_indices, zfill_batches, i, DrawMask,
                                    cur_mat_id, &draws_count);
                i = _draw_range_ext(builder, (*p_list_)->materials, zfill_batch_indices, zfill_batches, i,
                                    DrawMask | BDB::BitCustomShaded, cur_mat_id, &draws_count);
            }

            { // two-sided
                Ren::DebugMarker _mm(ctx.api_ctx(), ctx.current_cmd_buf(), "TWO-SIDED");

                Ren::RastState rast_state = pi_skin_moving_transp_vel_[1].rast_state();
                rast_state.viewport[2] = view_state_->act_res[0];
                rast_state.viewport[3] = view_state_->act_res[1];
                rast_state.ApplyChanged(builder.rast_state());
                builder.rast_state() = rast_state;

                const uint32_t DrawMask = BDB::BitsSkinned | BDB::BitAlphaTest | BDB::BitMoving | BDB::BitTwoSided;
                i = _draw_range_ext(builder, (*p_list_)->materials, zfill_batch_indices, zfill_batches, i, DrawMask,
                                    cur_mat_id, &draws_count);
                i = _draw_range_ext(builder, (*p_list_)->materials, zfill_batch_indices, zfill_batches, i,
                                    DrawMask | BDB::BitCustomShaded, cur_mat_id, &draws_count);
            }
        }
    }

    Ren::GLUnbindSamplers(BIND_MAT_TEX0, 1);
}
