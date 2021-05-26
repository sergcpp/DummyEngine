#include "RpOpaque.h"

#include "../DebugMarker.h"
#include "../Renderer_Structs.h"

#include <Ren/Context.h>
#include <Ren/RastState.h>

namespace RpSharedInternal {
void _bind_textures_and_samplers(Ren::Context &ctx, const Ren::Material &mat,
                                 Ren::SmallVectorImpl<Ren::SamplerRef> &temp_samplers) {
    assert(mat.textures.size() == mat.samplers.size());
    for (int j = 0; j < int(mat.textures.size()); ++j) {
        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_MAT_TEX0_SLOT + j,
                                   mat.textures[j]->id());
        glBindSampler(REN_MAT_TEX0_SLOT + j, mat.samplers[j]->id());
    }
}
uint32_t _draw_list_range_full(RpBuilder &builder, const Ren::MaterialStorage *materials,
                               const DynArrayConstRef<MainDrawBatch> &main_batches,
                               const DynArrayConstRef<uint32_t> &main_batch_indices,
                               uint32_t i, uint64_t mask, uint64_t &cur_mat_id,
                               uint64_t &cur_prog_id, BackendInfo &backend_info) {
    auto &ctx = builder.ctx();

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

        if (!ctx.capabilities.bindless_texture && cur_mat_id != batch.mat_id) {
            const Ren::Material &mat = materials->at(batch.mat_id);
            _bind_textures_and_samplers(builder.ctx(), mat, builder.temp_samplers);
        }

        cur_prog_id = batch.prog_id;
        cur_mat_id = batch.mat_id;

        glUniform1ui(REN_U_MAT_INDEX_LOC, batch.mat_id);
        glUniform4iv(REN_U_INSTANCES_LOC, (batch.instance_count + 3) / 4,
                     &batch.instance_indices[0]);

        glDrawElementsInstancedBaseVertex(
            cur_primitive, batch.indices_count, GL_UNSIGNED_INT,
            (const GLvoid *)uintptr_t(batch.indices_offset * sizeof(uint32_t)),
            GLsizei(batch.instance_count), GLint(batch.base_vertex));
        backend_info.opaque_draw_calls_count++;
        backend_info.tris_rendered += (batch.indices_count / 3) * batch.instance_count;
    }

    return i;
}

uint32_t _draw_list_range_full_rev(RpBuilder &builder,
                                   const Ren::MaterialStorage *materials,
                                   const DynArrayConstRef<MainDrawBatch> &main_batches,
                                   const DynArrayConstRef<uint32_t> &main_batch_indices,
                                   uint32_t ndx, uint64_t mask, uint64_t &cur_mat_id,
                                   uint64_t &cur_prog_id, BackendInfo &backend_info) {
    auto &ctx = builder.ctx();

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

        if (!ctx.capabilities.bindless_texture && cur_mat_id != batch.mat_id) {
            const Ren::Material &mat = materials->at(batch.mat_id);
            _bind_textures_and_samplers(builder.ctx(), mat, builder.temp_samplers);
        }

        cur_prog_id = batch.prog_id;
        cur_mat_id = batch.mat_id;

        glUniform1ui(REN_U_MAT_INDEX_LOC, batch.mat_id);
        glUniform4iv(REN_U_INSTANCES_LOC, (batch.instance_count + 3) / 4,
                     &batch.instance_indices[0]);

        glDrawElementsInstancedBaseVertex(
            GL_TRIANGLES, batch.indices_count, GL_UNSIGNED_INT,
            (const GLvoid *)uintptr_t(batch.indices_offset * sizeof(uint32_t)),
            GLsizei(batch.instance_count), GLint(batch.base_vertex));
        backend_info.opaque_draw_calls_count++;
        backend_info.tris_rendered += (batch.indices_count / 3) * batch.instance_count;
    }

    return uint32_t(i);
}
} // namespace RpSharedInternal

void RpOpaque::DrawOpaque(RpBuilder &builder) {
    using namespace RpSharedInternal;

    Ren::RastState rast_state;
    rast_state.depth_test.enabled = true;
    if ((render_flags_ & (EnableZFill | DebugWireframe)) == EnableZFill) {
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

    auto &ctx = builder.ctx();

    //
    // Bind resources (shadow atlas, lightmap, cells item data)
    //
    RpAllocBuf &instances_buf = builder.GetReadBuffer(instances_buf_);
    RpAllocBuf &unif_shared_data_buf = builder.GetReadBuffer(shared_data_buf_);
    RpAllocBuf &cells_buf = builder.GetReadBuffer(cells_buf_);
    RpAllocBuf &items_buf = builder.GetReadBuffer(items_buf_);
    RpAllocBuf &lights_buf = builder.GetReadBuffer(lights_buf_);
    RpAllocBuf &decals_buf = builder.GetReadBuffer(decals_buf_);

    RpAllocTex &shadowmap_tex = builder.GetReadTexture(shadowmap_tex_);
    RpAllocTex &ssao_tex = builder.GetReadTexture(ssao_tex_);

    glBindBufferRange(GL_SHADER_STORAGE_BUFFER, REN_MATERIALS_SLOT,
                      GLuint(bufs_->materials_buf.id),
                      GLintptr(bufs_->materials_buf_range.first),
                      GLsizeiptr(bufs_->materials_buf_range.second));
    if (ctx.capabilities.bindless_texture) {
        glBindBufferRange(GL_SHADER_STORAGE_BUFFER, REN_BINDLESS_TEX_SLOT,
                          GLuint(bufs_->textures_buf.id),
                          GLintptr(bufs_->textures_buf_range.first),
                          GLsizeiptr(bufs_->textures_buf_range.second));
    }

    glBindBufferRange(GL_UNIFORM_BUFFER, REN_UB_SHARED_DATA_LOC,
                      unif_shared_data_buf.ref->id(), orphan_index_ * SharedDataBlockSize,
                      sizeof(SharedDataBlock));

    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_SHAD_TEX_SLOT, shadowmap_tex.ref->id());

    if (decals_atlas_) {
        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_DECAL_TEX_SLOT,
                                   decals_atlas_->tex_id(0));
    }

    if ((render_flags_ & (EnableZFill | EnableSSAO)) == (EnableZFill | EnableSSAO)) {
        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_SSAO_TEX_SLOT, ssao_tex.ref->id());
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

    ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, REN_LIGHT_BUF_SLOT,
                               GLuint(lights_buf.tbos[orphan_index_]->id()));
    ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, REN_DECAL_BUF_SLOT,
                               GLuint(decals_buf.tbos[orphan_index_]->id()));
    ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, REN_CELLS_BUF_SLOT,
                               GLuint(cells_buf.tbos[orphan_index_]->id()));
    ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, REN_ITEMS_BUF_SLOT,
                               GLuint(items_buf.tbos[orphan_index_]->id()));

    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_NOISE_TEX_SLOT, noise_tex_->id());
    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_CONE_RT_LUT_SLOT, cone_rt_lut_->id());

    ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, REN_INST_BUF_SLOT,
                               GLuint(instances_buf.tbos[orphan_index_]->id()));

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

            i = _draw_list_range_full(builder, materials_, main_batches_,
                                      main_batch_indices_, i, 0ull, cur_mat_id,
                                      cur_prog_id, _dummy);
        }

        { // two-sided1
            DebugMarker _m("TWO-SIDED-1");

            rast_state.cull_face.enabled = false;
            rast_state.ApplyChanged(applied_state);
            applied_state = rast_state;

            i = _draw_list_range_full(builder, materials_, main_batches_,
                                      main_batch_indices_, i, MDB::BitTwoSided,
                                      cur_mat_id, cur_prog_id, _dummy);
        }

        { // one-sided2
            DebugMarker _m("ONE-SIDED-2");

            rast_state.cull_face.enabled = true;
            rast_state.ApplyChanged(applied_state);
            applied_state = rast_state;

            i = _draw_list_range_full(builder, materials_, main_batches_,
                                      main_batch_indices_, i, MDB::BitAlphaTest,
                                      cur_mat_id, cur_prog_id, _dummy);
        }

        { // two-sided2
            DebugMarker _m("TWO-SIDED-2");

            rast_state.cull_face.enabled = false;
            rast_state.ApplyChanged(applied_state);
            applied_state = rast_state;

            i = _draw_list_range_full(
                builder, materials_, main_batches_, main_batch_indices_, i,
                MDB::BitAlphaTest | MDB::BitTwoSided, cur_mat_id, cur_prog_id, _dummy);
        }

        alpha_blend_start_index_ = int(i);

        { // two-sided-tested-blended
            DebugMarker _m("TWO-SIDED-TESTED-BLENDED");

            rast_state.cull_face.enabled = false;
            rast_state.ApplyChanged(applied_state);
            applied_state = rast_state;

            i = _draw_list_range_full_rev(
                builder, materials_, main_batches_, main_batch_indices_,
                main_batch_indices_.count - 1,
                MDB::BitAlphaBlend | MDB::BitAlphaTest | MDB::BitTwoSided, cur_mat_id,
                cur_prog_id, _dummy);
        }

        { // one-sided-tested-blended
            DebugMarker _m("ONE-SIDED-TESTED-BLENDED");

            rast_state.cull_face.enabled = true;
            rast_state.ApplyChanged(applied_state);
            applied_state = rast_state;

            _draw_list_range_full_rev(
                builder, materials_, main_batches_, main_batch_indices_, i,
                MDB::BitAlphaBlend | MDB::BitAlphaTest, cur_mat_id, cur_prog_id, _dummy);
        }
    }

    Ren::GLUnbindSamplers(REN_MAT_TEX0_SLOT, 8);
}
