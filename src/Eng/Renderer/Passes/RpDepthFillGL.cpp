#include "RpDepthFill.h"

#include "../Renderer_Structs.h"

#include <Ren/Context.h>
#include <Ren/DebugMarker.h>
#include <Ren/RastState.h>

namespace RpSharedInternal {
void _bind_texture0_and_sampler0(Ren::Context &ctx, const Ren::Material &mat,
                                 Ren::SmallVectorImpl<Ren::SamplerRef> &temp_samplers) {
    assert(mat.textures.size() >= 1 && mat.samplers.size() >= 1);
    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_MAT_TEX0_SLOT, mat.textures[0]->id());
    glBindSampler(REN_MAT_TEX0_SLOT, mat.samplers[0]->id());
}
uint32_t _depth_draw_range(const DynArrayConstRef<uint32_t> &zfill_batch_indices,
                           const DynArrayConstRef<DepthDrawBatch> &zfill_batches, uint32_t i, uint32_t mask,
                           BackendInfo &backend_info) {
    for (; i < zfill_batch_indices.count; i++) {
        const DepthDrawBatch &batch = zfill_batches.data[zfill_batch_indices.data[i]];
        if ((batch.sort_key & DepthDrawBatch::FlagBits) != mask) {
            break;
        }

        if (!batch.instance_count) {
            continue;
        }

        glUniform2iv(REN_U_INSTANCES_LOC, batch.instance_count, &batch.instance_indices[0][0]);

        glDrawElementsInstancedBaseVertex(GL_TRIANGLES, batch.indices_count, GL_UNSIGNED_INT,
                                          (const GLvoid *)uintptr_t(batch.indices_offset * sizeof(uint32_t)),
                                          GLsizei(batch.instance_count), GLint(batch.base_vertex));
        backend_info.depth_fill_draw_calls_count++;
    }
    return i;
}

uint32_t _depth_draw_range_ext(RpBuilder &builder, const Ren::MaterialStorage *materials,
                               const DynArrayConstRef<uint32_t> &zfill_batch_indices,
                               const DynArrayConstRef<DepthDrawBatch> &zfill_batches, uint32_t i, uint32_t mask,
                               uint32_t &cur_mat_id, BackendInfo &backend_info) {
    auto &ctx = builder.ctx();

    for (; i < zfill_batch_indices.count; i++) {
        const DepthDrawBatch &batch = zfill_batches.data[zfill_batch_indices.data[i]];
        if ((batch.sort_key & DepthDrawBatch::FlagBits) != mask) {
            break;
        }

        if (!batch.instance_count) {
            continue;
        }

        if (!ctx.capabilities.bindless_texture && batch.instance_indices[0][1] != cur_mat_id) {
            const Ren::Material &mat = materials->at(batch.instance_indices[0][1]);
            _bind_texture0_and_sampler0(builder.ctx(), mat, builder.temp_samplers);
            cur_mat_id = batch.instance_indices[0][1];
        }

        glUniform2iv(REN_U_INSTANCES_LOC, batch.instance_count, &batch.instance_indices[0][0]);

        glDrawElementsInstancedBaseVertex(GL_TRIANGLES, batch.indices_count, GL_UNSIGNED_INT,
                                          (const GLvoid *)uintptr_t(batch.indices_offset * sizeof(uint32_t)),
                                          GLsizei(batch.instance_count), GLint(batch.base_vertex));
        backend_info.depth_fill_draw_calls_count++;
    }
    return i;
}
} // namespace RpSharedInternal

void RpDepthFill::DrawDepth(RpBuilder &builder, RpAllocBuf &vtx_buf1, RpAllocBuf &vtx_buf2, RpAllocBuf &ndx_buf) {
    using namespace RpSharedInternal;

    auto &ctx = builder.ctx();

    RpAllocBuf &unif_shared_data_buf = builder.GetReadBuffer(shared_data_buf_);
    RpAllocBuf &instances_buf = builder.GetReadBuffer(instances_buf_);
    RpAllocBuf &materials_buf = builder.GetReadBuffer(materials_buf_);
    RpAllocBuf &textures_buf = builder.GetReadBuffer(textures_buf_);
    RpAllocTex &noise_tex = builder.GetReadTexture(noise_tex_);

    glBindBufferBase(GL_UNIFORM_BUFFER, REN_UB_SHARED_DATA_LOC, GLuint(unif_shared_data_buf.ref->id()));

    assert(instances_buf.tbos[0]);
    ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, REN_INST_BUF_SLOT, GLuint(instances_buf.tbos[0]->id()));

    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_NOISE_TEX_SLOT, noise_tex.ref->id());

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, REN_MATERIALS_SLOT, GLuint(materials_buf.ref->id()));
    if (ctx.capabilities.bindless_texture) {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, REN_BINDLESS_TEX_SLOT, GLuint(textures_buf.ref->id()));
    }

    glBindFramebuffer(GL_FRAMEBUFFER, GLuint(depth_fill_fb_[ctx.backend_frame()].id()));
    glClear(GL_STENCIL_BUFFER_BIT);

    BackendInfo _dummy = {};
    uint32_t i = 0;

    using DDB = DepthDrawBatch;

    { // solid meshes
        Ren::DebugMarker _m(ctx.current_cmd_buf(), "STATIC-SOLID-SIMPLE");

        glBindVertexArray(vi_depth_pass_solid_.gl_vao());
        glUseProgram(pi_static_solid_[0].prog()->id());

        { // one-sided
            Ren::DebugMarker _mm(ctx.current_cmd_buf(), "ONE-SIDED");

            Ren::RastState rast_state = pi_static_solid_[0].rast_state();
            rast_state.viewport[2] = view_state_->act_res[0];
            rast_state.viewport[3] = view_state_->act_res[1];
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            i = _depth_draw_range(zfill_batch_indices, zfill_batches, i, 0u, _dummy);
        }

        { // two-sided
            Ren::DebugMarker _mm(ctx.current_cmd_buf(), "TWO-SIDED");

            Ren::RastState rast_state = pi_static_solid_[1].rast_state();
            rast_state.viewport[2] = view_state_->act_res[0];
            rast_state.viewport[3] = view_state_->act_res[1];
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            i = _depth_draw_range(zfill_batch_indices, zfill_batches, i, DDB::BitTwoSided, _dummy);
        }
    }

    // TODO: we can skip many things if TAA is disabled

    { // moving solid meshes (depth and velocity)
        Ren::DebugMarker _m(ctx.current_cmd_buf(), "STATIC-SOLID-MOVING");

        if ((render_flags_ & EnableTaa) != 0) {
            // Write depth and velocity
            glBindFramebuffer(GL_FRAMEBUFFER, depth_fill_vel_fb_[ctx.backend_frame()].id());
            glUseProgram(pi_moving_solid_[0].prog()->id());
        } else {
            // Write depth only
            glBindFramebuffer(GL_FRAMEBUFFER, depth_fill_fb_[ctx.backend_frame()].id());
        }

        { // one-sided
            Ren::DebugMarker _mm(ctx.current_cmd_buf(), "ONE-SIDED");

            Ren::RastState rast_state = pi_moving_solid_[0].rast_state();
            rast_state.viewport[2] = view_state_->act_res[0];
            rast_state.viewport[3] = view_state_->act_res[1];
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            i = _depth_draw_range(zfill_batch_indices, zfill_batches, i, DDB::BitMoving, _dummy);
        }

        { // two-sided
            Ren::DebugMarker _mm(ctx.current_cmd_buf(), "TWO-SIDED");

            Ren::RastState rast_state = pi_moving_solid_[1].rast_state();
            rast_state.viewport[2] = view_state_->act_res[0];
            rast_state.viewport[3] = view_state_->act_res[1];
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            i = _depth_draw_range(zfill_batch_indices, zfill_batches, i, DDB::BitMoving | DDB::BitTwoSided, _dummy);
        }
    }

    { // alpha-tested objects
        uint32_t cur_mat_id = 0xffffffff;

        { // simple meshes (depth only)
            Ren::DebugMarker _m(ctx.current_cmd_buf(), "STATIC-ALPHA-SIMPLE");

            glBindFramebuffer(GL_FRAMEBUFFER, depth_fill_fb_[ctx.backend_frame()].id());
            glBindVertexArray(vi_depth_pass_transp_.gl_vao());
            glUseProgram(pi_static_transp_[0].prog()->id());

            { // one-sided
                Ren::DebugMarker _mm(ctx.current_cmd_buf(), "ONE-SIDED");

                Ren::RastState rast_state = pi_static_transp_[0].rast_state();
                rast_state.viewport[2] = view_state_->act_res[0];
                rast_state.viewport[3] = view_state_->act_res[1];
                rast_state.ApplyChanged(builder.rast_state());
                builder.rast_state() = rast_state;

                i = _depth_draw_range_ext(builder, materials_, zfill_batch_indices, zfill_batches, i, DDB::BitAlphaTest,
                                          cur_mat_id, _dummy);
            }

            { // two-sided
                Ren::DebugMarker _mm(ctx.current_cmd_buf(), "TWO-SIDED");

                Ren::RastState rast_state = pi_static_transp_[1].rast_state();
                rast_state.viewport[2] = view_state_->act_res[0];
                rast_state.viewport[3] = view_state_->act_res[1];
                rast_state.ApplyChanged(builder.rast_state());
                builder.rast_state() = rast_state;

                i = _depth_draw_range_ext(builder, materials_, zfill_batch_indices, zfill_batches, i,
                                          DDB::BitAlphaTest | DDB::BitTwoSided, cur_mat_id, _dummy);
            }
        }

        { // moving meshes (depth and velocity)
            Ren::DebugMarker _m(ctx.current_cmd_buf(), "STATIC-ALPHA-MOVING");

            if ((render_flags_ & EnableTaa) != 0) {
                // Write depth and velocity
                glBindFramebuffer(GL_FRAMEBUFFER, depth_fill_vel_fb_[ctx.backend_frame()].id());
                glUseProgram(pi_moving_transp_[0].prog()->id());
            } else {
                // Write depth only
                glBindFramebuffer(GL_FRAMEBUFFER, depth_fill_fb_[ctx.backend_frame()].id());
            }

            { // one-sided
                Ren::DebugMarker _mm(ctx.current_cmd_buf(), "ONE-SIDED");

                Ren::RastState rast_state = pi_moving_transp_[0].rast_state();
                rast_state.viewport[2] = view_state_->act_res[0];
                rast_state.viewport[3] = view_state_->act_res[1];
                rast_state.ApplyChanged(builder.rast_state());
                builder.rast_state() = rast_state;

                i = _depth_draw_range_ext(builder, materials_, zfill_batch_indices, zfill_batches, i,
                                          DDB::BitAlphaTest | DDB::BitMoving, cur_mat_id, _dummy);
            }

            { // two-sided
                Ren::DebugMarker _mm(ctx.current_cmd_buf(), "TWO-SIDED");

                Ren::RastState rast_state = pi_moving_transp_[1].rast_state();
                rast_state.viewport[2] = view_state_->act_res[0];
                rast_state.viewport[3] = view_state_->act_res[1];
                rast_state.ApplyChanged(builder.rast_state());
                builder.rast_state() = rast_state;

                i = _depth_draw_range_ext(builder, materials_, zfill_batch_indices, zfill_batches, i,
                                          DDB::BitAlphaTest | DDB::BitMoving | DDB::BitTwoSided, cur_mat_id, _dummy);
            }
        }
    }

    { // static solid vegetation
        Ren::DebugMarker _m(ctx.current_cmd_buf(), "VEGE-SOLID-SIMPLE");
        glBindVertexArray(vi_depth_pass_vege_solid_.gl_vao());
        if ((render_flags_ & EnableTaa) != 0) {
            // Write depth and velocity
            glBindFramebuffer(GL_FRAMEBUFFER, depth_fill_vel_fb_[ctx.backend_frame()].id());
            glUseProgram(pi_vege_static_solid_vel_[0].prog()->id());
        } else {
            // Write depth only
            glBindFramebuffer(GL_FRAMEBUFFER, depth_fill_fb_[ctx.backend_frame()].id());
            glUseProgram(pi_vege_static_solid_[0].prog()->id());
        }

        { // one-sided
            Ren::DebugMarker _mm(ctx.current_cmd_buf(), "ONE-SIDED");

            Ren::RastState rast_state = pi_vege_static_solid_vel_[0].rast_state();
            rast_state.viewport[2] = view_state_->act_res[0];
            rast_state.viewport[3] = view_state_->act_res[1];
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            i = _depth_draw_range(zfill_batch_indices, zfill_batches, i, DDB::BitsVege, _dummy);
        }

        { // two-sided
            Ren::DebugMarker _mm(ctx.current_cmd_buf(), "TWO-SIDED");

            Ren::RastState rast_state = pi_vege_static_solid_vel_[1].rast_state();
            rast_state.viewport[2] = view_state_->act_res[0];
            rast_state.viewport[3] = view_state_->act_res[1];
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            i = _depth_draw_range(zfill_batch_indices, zfill_batches, i, DDB::BitsVege | DDB::BitTwoSided, _dummy);
        }
    }

    { // moving solid vegetation (depth and velocity)
        Ren::DebugMarker _m(ctx.current_cmd_buf(), "VEGE-SOLID-MOVING");
        if ((render_flags_ & EnableTaa) != 0) {
            glUseProgram(pi_vege_moving_solid_vel_[0].prog()->id());
        } else {
            glUseProgram(pi_vege_static_solid_[0].prog()->id());
        }

        { // one-sided
            Ren::DebugMarker _mm(ctx.current_cmd_buf(), "ONE-SIDED");

            Ren::RastState rast_state = pi_vege_moving_solid_vel_[0].rast_state();
            rast_state.viewport[2] = view_state_->act_res[0];
            rast_state.viewport[3] = view_state_->act_res[1];
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            i = _depth_draw_range(zfill_batch_indices, zfill_batches, i, DDB::BitsVege | DDB::BitMoving, _dummy);
        }

        { // two-sided
            Ren::DebugMarker _mm(ctx.current_cmd_buf(), "TWO-SIDED");

            Ren::RastState rast_state = pi_vege_moving_solid_vel_[1].rast_state();
            rast_state.viewport[2] = view_state_->act_res[0];
            rast_state.viewport[3] = view_state_->act_res[1];
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            i = _depth_draw_range(zfill_batch_indices, zfill_batches, i,
                                  DDB::BitsVege | DDB::BitMoving | DDB::BitTwoSided, _dummy);
        }
    }

    { // static alpha-tested vegetation
        uint32_t cur_mat_id = 0xffffffff;

        { // static alpha-tested vegetation (depth and velocity)
            Ren::DebugMarker _m(ctx.current_cmd_buf(), "VEGE-ALPHA-SIMPLE");
            glBindVertexArray(vi_depth_pass_vege_transp_.gl_vao());
            if ((render_flags_ & EnableTaa) != 0) {
                glUseProgram(pi_vege_static_transp_vel_[0].prog()->id());
            } else {
                glUseProgram(pi_vege_static_transp_[0].prog()->id());
            }

            { // one-sided
                Ren::DebugMarker _mm(ctx.current_cmd_buf(), "ONE-SIDED");

                Ren::RastState rast_state = pi_vege_static_transp_vel_[0].rast_state();
                rast_state.viewport[2] = view_state_->act_res[0];
                rast_state.viewport[3] = view_state_->act_res[1];
                rast_state.ApplyChanged(builder.rast_state());
                builder.rast_state() = rast_state;

                i = _depth_draw_range_ext(builder, materials_, zfill_batch_indices, zfill_batches, i,
                                          DDB::BitsVege | DDB::BitAlphaTest, cur_mat_id, _dummy);
            }

            { // two-sided
                Ren::DebugMarker _mm(ctx.current_cmd_buf(), "TWO-SIDED");

                Ren::RastState rast_state = pi_vege_static_transp_vel_[1].rast_state();
                rast_state.viewport[2] = view_state_->act_res[0];
                rast_state.viewport[3] = view_state_->act_res[1];
                rast_state.ApplyChanged(builder.rast_state());
                builder.rast_state() = rast_state;

                i = _depth_draw_range_ext(builder, materials_, zfill_batch_indices, zfill_batches, i,
                                          DDB::BitsVege | DDB::BitAlphaTest | DDB::BitTwoSided, cur_mat_id, _dummy);
            }
        }

        { // moving alpha-tested vegetation (depth and velocity)
            Ren::DebugMarker _m(ctx.current_cmd_buf(), "VEGE-ALPHA-MOVING");
            if ((render_flags_ & EnableTaa) != 0) {
                glUseProgram(pi_vege_moving_transp_vel_[0].prog()->id());
            } else {
                glUseProgram(pi_vege_static_transp_[0].prog()->id());
            }

            { // one-sided
                Ren::DebugMarker _mm(ctx.current_cmd_buf(), "ONE-SIDED");

                Ren::RastState rast_state = pi_vege_moving_transp_vel_[0].rast_state();
                rast_state.viewport[2] = view_state_->act_res[0];
                rast_state.viewport[3] = view_state_->act_res[1];
                rast_state.ApplyChanged(builder.rast_state());
                builder.rast_state() = rast_state;

                i = _depth_draw_range_ext(builder, materials_, zfill_batch_indices, zfill_batches, i,
                                          DDB::BitsVege | DDB::BitAlphaTest | DDB::BitMoving, cur_mat_id, _dummy);
            }

            { // two-sided
                Ren::DebugMarker _mm(ctx.current_cmd_buf(), "TWO-SIDED");

                Ren::RastState rast_state = pi_vege_moving_transp_vel_[1].rast_state();
                rast_state.viewport[2] = view_state_->act_res[0];
                rast_state.viewport[3] = view_state_->act_res[1];
                rast_state.ApplyChanged(builder.rast_state());
                builder.rast_state() = rast_state;

                i = _depth_draw_range_ext(builder, materials_, zfill_batch_indices, zfill_batches, i,
                                          DDB::BitsVege | DDB::BitAlphaTest | DDB::BitMoving | DDB::BitTwoSided,
                                          cur_mat_id, _dummy);
            }
        }
    }

    { // solid skinned meshes (depth and velocity)
        Ren::DebugMarker _m(ctx.current_cmd_buf(), "SKIN-SOLID-SIMPLE");
        if ((render_flags_ & EnableTaa) != 0) {
            glBindVertexArray(vi_depth_pass_skin_solid_.gl_vao());
            glUseProgram(pi_skin_static_solid_vel_[0].prog()->id());
        } else {
            glBindVertexArray(vi_depth_pass_solid_.gl_vao());
            glUseProgram(pi_static_solid_[0].prog()->id());
        }

        { // one-sided
            Ren::DebugMarker _mm(ctx.current_cmd_buf(), "ONE-SIDED");

            Ren::RastState rast_state = pi_skin_static_solid_vel_[0].rast_state();
            rast_state.viewport[2] = view_state_->act_res[0];
            rast_state.viewport[3] = view_state_->act_res[1];
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            i = _depth_draw_range(zfill_batch_indices, zfill_batches, i, DDB::BitsSkinned, _dummy);
        }

        { // two-sided
            Ren::DebugMarker _mm(ctx.current_cmd_buf(), "TWO-SIDED");

            Ren::RastState rast_state = pi_skin_static_solid_vel_[1].rast_state();
            rast_state.viewport[2] = view_state_->act_res[0];
            rast_state.viewport[3] = view_state_->act_res[1];
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            i = _depth_draw_range(zfill_batch_indices, zfill_batches, i, DDB::BitsSkinned | DDB::BitTwoSided, _dummy);
        }
    }

    { // moving solid skinned (depth and velocity)
        Ren::DebugMarker _m(ctx.current_cmd_buf(), "SKIN-SOLID-MOVING");
        if ((render_flags_ & EnableTaa) != 0) {
            glUseProgram(pi_skin_moving_solid_vel_[0].prog()->id());
        } else {
            glUseProgram(pi_skin_static_solid_[0].prog()->id());
        }

        { // one-sided
            Ren::DebugMarker _mm(ctx.current_cmd_buf(), "ONE-SIDED");

            Ren::RastState rast_state = pi_skin_moving_solid_vel_[0].rast_state();
            rast_state.viewport[2] = view_state_->act_res[0];
            rast_state.viewport[3] = view_state_->act_res[1];
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            i = _depth_draw_range(zfill_batch_indices, zfill_batches, i, DDB::BitsSkinned | DDB::BitMoving, _dummy);
        }

        { // two-sided
            Ren::DebugMarker _mm(ctx.current_cmd_buf(), "TWO-SIDED");

            Ren::RastState rast_state = pi_skin_moving_solid_vel_[1].rast_state();
            rast_state.viewport[2] = view_state_->act_res[0];
            rast_state.viewport[3] = view_state_->act_res[1];
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            i = _depth_draw_range(zfill_batch_indices, zfill_batches, i,
                                  DDB::BitsSkinned | DDB::BitMoving | DDB::BitTwoSided, _dummy);
        }
    }

    { // static alpha-tested skinned
        uint32_t cur_mat_id = 0xffffffff;

        { // simple alpha-tested skinned (depth and velocity)
            Ren::DebugMarker _m(ctx.current_cmd_buf(), "SKIN-ALPHA-SIMPLE");
            glBindVertexArray(vi_depth_pass_skin_transp_.gl_vao());
            if ((render_flags_ & EnableTaa) != 0) {
                glUseProgram(pi_skin_static_transp_vel_[0].prog()->id());
            } else {
                glUseProgram(pi_skin_static_transp_[0].prog()->id());
            }

            { // one-sided
                Ren::DebugMarker _mm(ctx.current_cmd_buf(), "ONE-SIDED");

                Ren::RastState rast_state = pi_skin_static_transp_vel_[0].rast_state();
                rast_state.viewport[2] = view_state_->act_res[0];
                rast_state.viewport[3] = view_state_->act_res[1];
                rast_state.ApplyChanged(builder.rast_state());
                builder.rast_state() = rast_state;

                i = _depth_draw_range_ext(builder, materials_, zfill_batch_indices, zfill_batches, i,
                                          DDB::BitsSkinned | DDB::BitAlphaTest, cur_mat_id, _dummy);
            }

            { // two-sided
                Ren::DebugMarker _mm(ctx.current_cmd_buf(), "TWO-SIDED");

                Ren::RastState rast_state = pi_skin_static_transp_vel_[1].rast_state();
                rast_state.viewport[2] = view_state_->act_res[0];
                rast_state.viewport[3] = view_state_->act_res[1];
                rast_state.ApplyChanged(builder.rast_state());
                builder.rast_state() = rast_state;

                i = _depth_draw_range_ext(builder, materials_, zfill_batch_indices, zfill_batches, i,
                                          DDB::BitsSkinned | DDB::BitAlphaTest | DDB::BitTwoSided, cur_mat_id, _dummy);
            }
        }

        { // moving alpha-tested skinned (depth and velocity)
            Ren::DebugMarker _m(ctx.current_cmd_buf(), "SKIN-ALPHA-MOVING");
            if ((render_flags_ & EnableTaa) != 0) {
                glUseProgram(pi_skin_moving_transp_vel_[0].prog()->id());
            } else {
                glUseProgram(pi_skin_static_transp_[0].prog()->id());
            }

            { // one-sided
                Ren::DebugMarker _mm(ctx.current_cmd_buf(), "ONE-SIDED");

                Ren::RastState rast_state = pi_skin_moving_transp_vel_[0].rast_state();
                rast_state.viewport[2] = view_state_->act_res[0];
                rast_state.viewport[3] = view_state_->act_res[1];
                rast_state.ApplyChanged(builder.rast_state());
                builder.rast_state() = rast_state;

                i = _depth_draw_range_ext(builder, materials_, zfill_batch_indices, zfill_batches, i,
                                          DDB::BitsSkinned | DDB::BitAlphaTest | DDB::BitMoving, cur_mat_id, _dummy);
            }

            { // two-sided
                Ren::DebugMarker _mm(ctx.current_cmd_buf(), "TWO-SIDED");

                Ren::RastState rast_state = pi_skin_moving_transp_vel_[1].rast_state();
                rast_state.viewport[2] = view_state_->act_res[0];
                rast_state.viewport[3] = view_state_->act_res[1];
                rast_state.ApplyChanged(builder.rast_state());
                builder.rast_state() = rast_state;

                i = _depth_draw_range_ext(builder, materials_, zfill_batch_indices, zfill_batches, i,
                                          DDB::BitsSkinned | DDB::BitAlphaTest | DDB::BitMoving | DDB::BitTwoSided,
                                          cur_mat_id, _dummy);
            }
        }
    }

    Ren::GLUnbindSamplers(REN_MAT_TEX0_SLOT, 1);
}
