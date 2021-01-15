#include "RpDepthFill.h"

#include "../DebugMarker.h"
#include "../Renderer_Structs.h"

#include <Ren/Context.h>
#include <Ren/RastState.h>

namespace RpSharedInternal {
uint32_t _depth_draw_range(const DynArrayConstRef<uint32_t> &zfill_batch_indices,
                           const DynArrayConstRef<DepthDrawBatch> &zfill_batches,
                           uint32_t i, uint32_t mask, BackendInfo &backend_info) {
    for (; i < zfill_batch_indices.count; i++) {
        const DepthDrawBatch &batch = zfill_batches.data[zfill_batch_indices.data[i]];
        if ((batch.sort_key & DepthDrawBatch::FlagBits) != mask) {
            break;
        }

        if (!batch.instance_count) {
            continue;
        }

        glUniform4iv(REN_U_INSTANCES_LOC, (batch.instance_count + 3) / 4,
                     &batch.instance_indices[0]);

        glDrawElementsInstancedBaseVertex(
            GL_TRIANGLES, batch.indices_count, GL_UNSIGNED_INT,
            (const GLvoid *)uintptr_t(batch.indices_offset * sizeof(uint32_t)),
            (GLsizei)batch.instance_count, (GLint)batch.base_vertex);
        backend_info.depth_fill_draw_calls_count++;
    }
    return i;
}

uint32_t _depth_draw_range_ext(Ren::Context &ctx,
                               const DynArrayConstRef<uint32_t> &zfill_batch_indices,
                               const DynArrayConstRef<DepthDrawBatch> &zfill_batches,
                               uint32_t i, uint32_t mask, uint32_t &cur_mat_id,
                               BackendInfo &backend_info) {
    for (; i < zfill_batch_indices.count; i++) {
        const DepthDrawBatch &batch = zfill_batches.data[zfill_batch_indices.data[i]];
        if ((batch.sort_key & DepthDrawBatch::FlagBits) != mask) {
            break;
        }

        if (!batch.instance_count) {
            continue;
        }

        if (batch.mat_id != cur_mat_id) {
            const Ren::Material *mat = ctx.GetMaterial(batch.mat_id).get();
            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_MAT_TEX0_SLOT,
                                       mat->textures[0]->id());
            cur_mat_id = batch.mat_id;
        }

        glUniform4iv(REN_U_INSTANCES_LOC, (batch.instance_count + 3) / 4,
                     &batch.instance_indices[0]);

        glDrawElementsInstancedBaseVertex(
            GL_TRIANGLES, batch.indices_count, GL_UNSIGNED_INT,
            (const GLvoid *)uintptr_t(batch.indices_offset * sizeof(uint32_t)),
            (GLsizei)batch.instance_count, (GLint)batch.base_vertex);
        backend_info.depth_fill_draw_calls_count++;
    }
    return i;
}
} // namespace RpSharedInternal

void RpDepthFill::DrawDepth(RpBuilder &builder) {
    using namespace RpSharedInternal;

    Ren::RastState rast_state;
    rast_state.depth_test.enabled = true;
    rast_state.depth_test.func = Ren::eTestFunc::Less;

    rast_state.stencil.enabled = true;
    rast_state.stencil.mask = 0xff;
    rast_state.stencil.pass = Ren::eStencilOp::Replace;

    rast_state.cull_face.enabled = true;

    rast_state.viewport[2] = view_state_->act_res[0];
    rast_state.viewport[3] = view_state_->act_res[1];

    rast_state.Apply();
    Ren::RastState applied_state = rast_state;

    uint32_t i = 0;

    RpAllocBuf &unif_shared_data_buf = builder.GetReadBuffer(input_[1]);
    glBindBufferRange(GL_UNIFORM_BUFFER, REN_UB_SHARED_DATA_LOC,
                      GLuint(unif_shared_data_buf.ref->id()),
                      orphan_index_ * SharedDataBlockSize, sizeof(SharedDataBlock));
    assert(orphan_index_ * SharedDataBlockSize %
               builder.ctx().capabilities.unif_buf_offset_alignment ==
           0);

    RpAllocBuf &instances_buf = builder.GetReadBuffer(input_[0]);
    assert(instances_buf.tbos[orphan_index_]);

    ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, REN_INST_BUF_SLOT,
                               GLuint(instances_buf.tbos[orphan_index_]->id()));

    glBindFramebuffer(GL_FRAMEBUFFER, GLuint(depth_fill_fb_.id()));
    glClear(GL_STENCIL_BUFFER_BIT);

    BackendInfo _dummy = {};

    using DDB = DepthDrawBatch;

    { // solid meshes
        DebugMarker _m("STATIC-SOLID-SIMPLE");

        glBindVertexArray(depth_pass_solid_vao_.id());
        glUseProgram(fillz_solid_prog_->id());

        // default value
        rast_state.stencil.test_ref = 0;

        { // one-sided
            DebugMarker _mm("ONE-SIDED");

            rast_state.cull_face.enabled = true;
            rast_state.ApplyChanged(applied_state);
            applied_state = rast_state;

            i = _depth_draw_range(zfill_batch_indices, zfill_batches, i, 0u, _dummy);
        }

        { // two-sided
            DebugMarker _mm("TWO-SIDED");

            rast_state.cull_face.enabled = false;
            rast_state.ApplyChanged(applied_state);
            applied_state = rast_state;

            i = _depth_draw_range(zfill_batch_indices, zfill_batches, i, DDB::BitTwoSided,
                                  _dummy);
        }
    }

    // TODO: we can skip many things if TAA is disabled

    { // moving solid meshes (depth and velocity)
        DebugMarker _m("STATIC-SOLID-MOVING");

        if ((render_flags_ & EnableTaa) != 0) {
            // Write depth and velocity
            glBindFramebuffer(GL_FRAMEBUFFER, depth_fill_vel_fb_.id());
            glUseProgram(fillz_solid_mov_prog_->id());
        } else {
            // Write depth only
            glBindFramebuffer(GL_FRAMEBUFFER, depth_fill_fb_.id());
        }

        // mark dynamic objects
        rast_state.stencil.test_ref = 1;

        { // one-sided
            DebugMarker _mm("ONE-SIDED");

            rast_state.cull_face.enabled = true;
            rast_state.ApplyChanged(applied_state);
            applied_state = rast_state;

            i = _depth_draw_range(zfill_batch_indices, zfill_batches, i, DDB::BitMoving,
                                  _dummy);
        }

        { // two-sided
            DebugMarker _mm("TWO-SIDED");

            rast_state.cull_face.enabled = false;
            rast_state.ApplyChanged(applied_state);
            applied_state = rast_state;

            i = _depth_draw_range(zfill_batch_indices, zfill_batches, i,
                                  DDB::BitMoving | DDB::BitTwoSided, _dummy);
        }
    }

    { // alpha-tested objects
        uint32_t cur_mat_id = 0xffffffff;

        { // simple meshes (depth only)
            DebugMarker _m("STATIC-ALPHA-SIMPLE");

            glBindFramebuffer(GL_FRAMEBUFFER, depth_fill_fb_.id());
            glBindVertexArray(depth_pass_transp_vao_.id());
            glUseProgram(fillz_transp_prog_->id());

            // default value
            rast_state.stencil.test_ref = 0;

            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_MAT_TEX0_SLOT,
                                       dummy_white_->id());

            { // one-sided
                DebugMarker _mm("ONE-SIDED");

                rast_state.cull_face.enabled = true;
                rast_state.ApplyChanged(applied_state);
                applied_state = rast_state;

                i = _depth_draw_range_ext(builder.ctx(), zfill_batch_indices,
                                          zfill_batches, i, DDB::BitAlphaTest, cur_mat_id,
                                          _dummy);
            }

            { // two-sided
                DebugMarker _mm("TWO-SIDED");

                rast_state.cull_face.enabled = false;
                rast_state.ApplyChanged(applied_state);
                applied_state = rast_state;

                i = _depth_draw_range_ext(
                    builder.ctx(), zfill_batch_indices, zfill_batches, i,
                    DDB::BitAlphaTest | DDB::BitTwoSided, cur_mat_id, _dummy);
            }
        }

        { // moving meshes (depth and velocity)
            DebugMarker _m("STATIC-ALPHA-MOVING");

            if ((render_flags_ & EnableTaa) != 0) {
                // Write depth and velocity
                glBindFramebuffer(GL_FRAMEBUFFER, depth_fill_vel_fb_.id());
                glUseProgram(fillz_transp_mov_prog_->id());
            } else {
                // Write depth only
                glBindFramebuffer(GL_FRAMEBUFFER, depth_fill_fb_.id());
            }

            // mark dynamic objects
            rast_state.stencil.test_ref = 1;

            { // one-sided
                DebugMarker _mm("ONE-SIDED");

                rast_state.cull_face.enabled = true;
                rast_state.ApplyChanged(applied_state);
                applied_state = rast_state;

                i = _depth_draw_range_ext(
                    builder.ctx(), zfill_batch_indices, zfill_batches, i,
                    DDB::BitAlphaTest | DDB::BitMoving, cur_mat_id, _dummy);
            }

            { // two-sided
                DebugMarker _mm("TWO-SIDED");

                rast_state.cull_face.enabled = false;
                rast_state.ApplyChanged(applied_state);
                applied_state = rast_state;

                i = _depth_draw_range_ext(
                    builder.ctx(), zfill_batch_indices, zfill_batches, i,
                    DDB::BitAlphaTest | DDB::BitMoving | DDB::BitTwoSided, cur_mat_id,
                    _dummy);
            }
        }
    }

    { // solid vegetation
        DebugMarker _m("VEGE-SOLID-SIMPLE");
        glBindVertexArray(depth_pass_vege_solid_vao_.id());
        if ((render_flags_ & EnableTaa) != 0) {
            // Write depth and velocity
            glBindFramebuffer(GL_FRAMEBUFFER, depth_fill_vel_fb_.id());
            glUseProgram(fillz_vege_solid_vel_prog_->id());
        } else {
            // Write depth only
            glBindFramebuffer(GL_FRAMEBUFFER, depth_fill_fb_.id());
            glUseProgram(fillz_vege_solid_prog_->id());
        }

        // mark dynamic objects
        rast_state.stencil.test_ref = 1;

        { // one-sided
            DebugMarker _mm("ONE-SIDED");

            rast_state.cull_face.enabled = true;
            rast_state.ApplyChanged(applied_state);
            applied_state = rast_state;

            i = _depth_draw_range(zfill_batch_indices, zfill_batches, i, DDB::BitsVege,
                                  _dummy);
        }

        { // two-sided
            DebugMarker _mm("TWO-SIDED");

            rast_state.cull_face.enabled = false;
            rast_state.ApplyChanged(applied_state);
            applied_state = rast_state;

            i = _depth_draw_range(zfill_batch_indices, zfill_batches, i,
                                  DDB::BitsVege | DDB::BitTwoSided, _dummy);
        }
    }

    { // moving solid vegetation (depth and velocity)
        DebugMarker _m("VEGE-SOLID-MOVING");
        if ((render_flags_ & EnableTaa) != 0) {
            glUseProgram(fillz_vege_solid_vel_mov_prog_->id());
        } else {
            glUseProgram(fillz_vege_solid_prog_->id());
        }

        // mark dynamic objects
        rast_state.stencil.test_ref = 1;

        { // one-sided
            DebugMarker _mm("ONE-SIDED");

            rast_state.cull_face.enabled = true;
            rast_state.ApplyChanged(applied_state);
            applied_state = rast_state;

            i = _depth_draw_range(zfill_batch_indices, zfill_batches, i,
                                  DDB::BitsVege | DDB::BitMoving, _dummy);
        }

        { // two-sided
            DebugMarker _mm("TWO-SIDED");

            rast_state.cull_face.enabled = false;
            rast_state.ApplyChanged(applied_state);
            applied_state = rast_state;

            i = _depth_draw_range(zfill_batch_indices, zfill_batches, i,
                                  DDB::BitsVege | DDB::BitMoving | DDB::BitTwoSided,
                                  _dummy);
        }
    }

    { // alpha-tested vegetation
        uint32_t cur_mat_id = 0xffffffff;

        { // moving alpha-tested vegetation (depth and velocity)
            DebugMarker _m("VEGE-ALPHA-SIMPLE");
            glBindVertexArray(depth_pass_vege_transp_vao_.id());
            if ((render_flags_ & EnableTaa) != 0) {
                glUseProgram(fillz_vege_transp_vel_prog_->id());
            } else {
                glUseProgram(fillz_vege_transp_prog_->id());
            }

            // mark dynamic objects
            rast_state.stencil.test_ref = 1;

            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_MAT_TEX0_SLOT,
                                       dummy_white_->id());

            { // one-sided
                DebugMarker _mm("ONE-SIDED");

                rast_state.cull_face.enabled = true;
                rast_state.ApplyChanged(applied_state);
                applied_state = rast_state;

                i = _depth_draw_range_ext(
                    builder.ctx(), zfill_batch_indices, zfill_batches, i,
                    DDB::BitsVege | DDB::BitAlphaTest, cur_mat_id, _dummy);
            }

            { // two-sided
                DebugMarker _mm("TWO-SIDED");

                rast_state.cull_face.enabled = false;
                rast_state.ApplyChanged(applied_state);
                applied_state = rast_state;

                i = _depth_draw_range_ext(
                    builder.ctx(), zfill_batch_indices, zfill_batches, i,
                    DDB::BitsVege | DDB::BitAlphaTest | DDB::BitTwoSided, cur_mat_id,
                    _dummy);
            }
        }

        { // moving alpha-tested vegetation (depth and velocity)
            DebugMarker _m("VEGE-ALPHA-MOVING");
            if ((render_flags_ & EnableTaa) != 0) {
                glUseProgram(fillz_vege_transp_vel_mov_prog_->id());
            } else {
                glUseProgram(fillz_vege_transp_prog_->id());
            }

            // mark dynamic objects
            rast_state.stencil.test_ref = 1;

            { // one-sided
                DebugMarker _mm("ONE-SIDED");

                rast_state.cull_face.enabled = true;
                rast_state.ApplyChanged(applied_state);
                applied_state = rast_state;

                i = _depth_draw_range_ext(
                    builder.ctx(), zfill_batch_indices, zfill_batches, i,
                    DDB::BitsVege | DDB::BitAlphaTest | DDB::BitMoving, cur_mat_id,
                    _dummy);
            }

            { // two-sided
                DebugMarker _mm("TWO-SIDED");

                rast_state.cull_face.enabled = false;
                rast_state.ApplyChanged(applied_state);
                applied_state = rast_state;

                i = _depth_draw_range_ext(
                    builder.ctx(), zfill_batch_indices, zfill_batches, i,
                    DDB::BitsVege | DDB::BitAlphaTest | DDB::BitMoving | DDB::BitTwoSided,
                    cur_mat_id, _dummy);
            }
        }
    }

    { // solid skinned meshes (depth and velocity)
        DebugMarker _m("SKIN-SOLID-SIMPLE");
        if ((render_flags_ & EnableTaa) != 0) {
            glBindVertexArray(depth_pass_skin_solid_vao_.id());
            glUseProgram(fillz_skin_solid_vel_prog_->id());
        } else {
            glBindVertexArray(depth_pass_solid_vao_.id());
            glUseProgram(fillz_solid_prog_->id());
        }

        // mark dynamic objects
        rast_state.stencil.test_ref = 1;

        { // one-sided
            DebugMarker _mm("ONE-SIDED");

            rast_state.cull_face.enabled = true;
            rast_state.ApplyChanged(applied_state);
            applied_state = rast_state;

            i = _depth_draw_range(zfill_batch_indices, zfill_batches, i, DDB::BitsSkinned,
                                  _dummy);
        }

        { // two-sided
            DebugMarker _mm("TWO-SIDED");
            glDisable(GL_CULL_FACE);

            i = _depth_draw_range(zfill_batch_indices, zfill_batches, i,
                                  DDB::BitsSkinned | DDB::BitTwoSided, _dummy);

            glEnable(GL_CULL_FACE);
        }
    }

    { // moving solid skinned (depth and velocity)
        DebugMarker _m("SKIN-SOLID-MOVING");
        if ((render_flags_ & EnableTaa) != 0) {
            glUseProgram(fillz_skin_solid_vel_mov_prog_->id());
        } else {
            glUseProgram(fillz_skin_solid_prog_->id());
        }

        // mark dynamic objects
        rast_state.stencil.test_ref = 1;

        { // one-sided
            DebugMarker _mm("ONE-SIDED");

            rast_state.cull_face.enabled = true;
            rast_state.ApplyChanged(applied_state);
            applied_state = rast_state;

            i = _depth_draw_range(zfill_batch_indices, zfill_batches, i,
                                  DDB::BitsSkinned | DDB::BitMoving, _dummy);
        }

        { // two-sided
            DebugMarker _mm("TWO-SIDED");

            rast_state.cull_face.enabled = false;
            rast_state.ApplyChanged(applied_state);
            applied_state = rast_state;

            i = _depth_draw_range(zfill_batch_indices, zfill_batches, i,
                                  DDB::BitsSkinned | DDB::BitMoving | DDB::BitTwoSided,
                                  _dummy);
        }
    }

    { // alpha-tested skinned
        uint32_t cur_mat_id = 0xffffffff;

        { // simple alpha-tested skinned (depth and velocity)
            DebugMarker _m("SKIN-ALPHA-SIMPLE");
            glBindVertexArray(depth_pass_skin_transp_vao_.id());
            if ((render_flags_ & EnableTaa) != 0) {
                glUseProgram(fillz_skin_transp_vel_prog_->id());
            } else {
                glUseProgram(fillz_skin_transp_prog_->id());
            }

            // mark dynamic objects
            rast_state.stencil.test_ref = 1;

            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_MAT_TEX0_SLOT,
                                       dummy_white_->id());

            { // one-sided
                DebugMarker _mm("ONE-SIDED");

                rast_state.cull_face.enabled = true;
                rast_state.ApplyChanged(applied_state);
                applied_state = rast_state;

                i = _depth_draw_range_ext(
                    builder.ctx(), zfill_batch_indices, zfill_batches, i,
                    DDB::BitsSkinned | DDB::BitAlphaTest, cur_mat_id, _dummy);
            }

            { // two-sided
                DebugMarker _mm("TWO-SIDED");

                rast_state.cull_face.enabled = false;
                rast_state.ApplyChanged(applied_state);
                applied_state = rast_state;

                i = _depth_draw_range_ext(
                    builder.ctx(), zfill_batch_indices, zfill_batches, i,
                    DDB::BitsSkinned | DDB::BitAlphaTest | DDB::BitTwoSided, cur_mat_id,
                    _dummy);
            }
        }

        { // moving alpha-tested skinned (depth and velocity)
            DebugMarker _m("SKIN-ALPHA-MOVING");
            if ((render_flags_ & EnableTaa) != 0) {
                glUseProgram(fillz_skin_transp_vel_mov_prog_->id());
            } else {
                glUseProgram(fillz_skin_transp_prog_->id());
            }

            // mark dynamic objects
            rast_state.stencil.test_ref = 1;

            { // one-sided
                DebugMarker _mm("ONE-SIDED");

                rast_state.cull_face.enabled = true;
                rast_state.ApplyChanged(applied_state);
                applied_state = rast_state;

                i = _depth_draw_range_ext(
                    builder.ctx(), zfill_batch_indices, zfill_batches, i,
                    DDB::BitsSkinned | DDB::BitAlphaTest | DDB::BitMoving, cur_mat_id,
                    _dummy);
            }

            { // two-sided
                DebugMarker _mm("TWO-SIDED");

                rast_state.cull_face.enabled = false;
                rast_state.ApplyChanged(applied_state);
                applied_state = rast_state;

                i = _depth_draw_range_ext(builder.ctx(), zfill_batch_indices,
                                          zfill_batches, i,
                                          DDB::BitsSkinned | DDB::BitAlphaTest |
                                              DDB::BitMoving | DDB::BitTwoSided,
                                          cur_mat_id, _dummy);
            }
        }
    }
}
