#include "RpOpaque.h"

#include "../DebugMarker.h"
#include "../Renderer_Structs.h"

#include <Ren/Context.h>
#include <Ren/RastState.h>

namespace RpSharedInternal {
uint32_t _draw_list_range_full(Ren::Context &ctx,
                               const DynArrayConstRef<MainDrawBatch> &main_batches,
                               const DynArrayConstRef<uint32_t> &main_batch_indices,
                               uint32_t i, uint64_t mask, uint64_t &cur_mat_id,
                               uint64_t &cur_prog_id, BackendInfo &backend_info) {
    GLenum cur_primitive;
    if (cur_prog_id != 0xffffffffffffffff) {
        const Ren::Program *p = ctx.GetProgram(uint32_t(cur_prog_id)).get();
        if (p->has_tessellation()) {
            cur_primitive = GL_PATCHES;
        } else {
            cur_primitive = GL_TRIANGLES;
        }
    }

    for (; i < main_batch_indices.count; i++) {
        const MainDrawBatch &batch = main_batches.data[main_batch_indices.data[i]];
        if ((batch.sort_key & MainDrawBatch::FlagBits) != mask) {
            break;
        }

        if (!batch.instance_count) {
            continue;
        }

        if (cur_prog_id != batch.prog_id) {
            const Ren::Program *p = ctx.GetProgram(batch.prog_id).get();
            glUseProgram(p->id());

            if (p->has_tessellation()) {
                cur_primitive = GL_PATCHES;
            } else {
                cur_primitive = GL_TRIANGLES;
            }
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
            cur_primitive, batch.indices_count, GL_UNSIGNED_INT,
            (const GLvoid *)uintptr_t(batch.indices_offset * sizeof(uint32_t)),
            (GLsizei)batch.instance_count, (GLint)batch.base_vertex);
        backend_info.opaque_draw_calls_count++;
        backend_info.tris_rendered += (batch.indices_count / 3) * batch.instance_count;
    }

    return i;
}

uint32_t _draw_list_range_full_rev(Ren::Context &ctx,
                                   const DynArrayConstRef<MainDrawBatch> &main_batches,
                                   const DynArrayConstRef<uint32_t> &main_batch_indices,
                                   uint32_t ndx, uint64_t mask, uint64_t &cur_mat_id,
                                   uint64_t &cur_prog_id, BackendInfo &backend_info) {
    int i = int(ndx);
    for (; i >= 0; i--) {
        const MainDrawBatch &batch = main_batches.data[main_batch_indices.data[i]];
        if ((batch.sort_key & MainDrawBatch::FlagBits) != mask) {
            break;
        }

        if (!batch.instance_count) {
            continue;
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
            (GLsizei)batch.instance_count, (GLint)batch.base_vertex);
        backend_info.opaque_draw_calls_count++;
        backend_info.tris_rendered += (batch.indices_count / 3) * batch.instance_count;
    }

    return uint32_t(i);
}
} // namespace RpSharedInternal

void RpOpaque::DrawOpaque(Graph::RpBuilder &builder) {
    using namespace RpSharedInternal;

    Ren::RastState rast_state;
    rast_state.depth_test.enabled = true;
    if (render_flags_ & EnableZFill) {
        rast_state.depth_test.func = Ren::eTestFunc::Equal;
    } else {
        rast_state.depth_test.func = Ren::eTestFunc::LEqual;
    }

    rast_state.cull_face.enabled = true;

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
    glBindFramebuffer(GL_FRAMEBUFFER, opaque_draw_fb_.id());

    rast_state.viewport[2] = view_state_->act_res[0];
    rast_state.viewport[3] = view_state_->act_res[1];
#endif

    rast_state.Apply();
    Ren::RastState applied_state = rast_state;

    glBindVertexArray(draw_pass_vao_.id());

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

    BackendInfo _dummy = {};

    { // actual drawing
        using MDB = MainDrawBatch;

        uint64_t cur_prog_id = 0xffffffffffffffff;
        uint64_t cur_mat_id = 0xffffffffffffffff;

        uint32_t i = 0;

        { // one-sided1
            DebugMarker _m("ONE-SIDED-1");

            rast_state.cull_face.enabled = true;
            rast_state.ApplyChanged(applied_state);
            applied_state = rast_state;

            i = _draw_list_range_full(builder.ctx(), main_batches_, main_batch_indices_,
                                      i, 0ull, cur_mat_id, cur_prog_id, _dummy);
        }

        { // two-sided1
            DebugMarker _m("TWO-SIDED-1");

            rast_state.cull_face.enabled = false;
            rast_state.ApplyChanged(applied_state);
            applied_state = rast_state;

            i = _draw_list_range_full(builder.ctx(), main_batches_, main_batch_indices_,
                                      i, MDB::BitTwoSided, cur_mat_id, cur_prog_id,
                                      _dummy);
        }

        { // one-sided2
            DebugMarker _m("ONE-SIDED-2");

            rast_state.cull_face.enabled = true;
            rast_state.ApplyChanged(applied_state);
            applied_state = rast_state;

            i = _draw_list_range_full(builder.ctx(), main_batches_, main_batch_indices_,
                                      i, MDB::BitAlphaTest, cur_mat_id, cur_prog_id,
                                      _dummy);
        }

        { // two-sided2
            DebugMarker _m("TWO-SIDED-2");

            rast_state.cull_face.enabled = false;
            rast_state.ApplyChanged(applied_state);
            applied_state = rast_state;

            i = _draw_list_range_full(builder.ctx(), main_batches_, main_batch_indices_,
                                      i, MDB::BitAlphaTest | MDB::BitTwoSided, cur_mat_id,
                                      cur_prog_id, _dummy);
        }

        alpha_blend_start_index_ = int(i);

        { // two-sided-tested-blended
            DebugMarker _m("TWO-SIDED-TESTED-BLENDED");

            rast_state.cull_face.enabled = false;
            rast_state.ApplyChanged(applied_state);
            applied_state = rast_state;

            i = _draw_list_range_full_rev(
                builder.ctx(), main_batches_, main_batch_indices_,
                main_batch_indices_.count - 1,
                MDB::BitAlphaBlend | MDB::BitAlphaTest | MDB::BitTwoSided, cur_mat_id,
                cur_prog_id, _dummy);
        }

        { // one-sided-tested-blended
            DebugMarker _m("ONE-SIDED-TESTED-BLENDED");

            rast_state.cull_face.enabled = true;
            rast_state.ApplyChanged(applied_state);
            applied_state = rast_state;

            _draw_list_range_full_rev(builder.ctx(), main_batches_, main_batch_indices_,
                                      i, MDB::BitAlphaBlend | MDB::BitAlphaTest,
                                      cur_mat_id, cur_prog_id, _dummy);
        }
    }
}
