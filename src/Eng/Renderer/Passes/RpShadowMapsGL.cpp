#include "RpShadowMaps.h"

#include <Ren/RastState.h>

#include "../DebugMarker.h"
#include "../Renderer_Structs.h"

#include <Ren/Context.h>

namespace RpSharedInternal {
void _bind_texture0_and_sampler0(Ren::Context &ctx, const Ren::Material &mat,
                                 Ren::SmallVectorImpl<Ren::SamplerRef> &temp_samplers);
}
namespace RpShadowMapsInternal {
using namespace RpSharedInternal;

void _adjust_bias_and_viewport(Ren::RastState &rast_state, const ShadowList &sh_list) {
    Ren::RastState new_rast_state = rast_state;

    new_rast_state.polygon_offset.factor = sh_list.bias[0];
    new_rast_state.polygon_offset.units = sh_list.bias[1];

    new_rast_state.viewport[0] = sh_list.shadow_map_pos[0];
    new_rast_state.viewport[1] = sh_list.shadow_map_pos[1];
    new_rast_state.viewport[2] = sh_list.shadow_map_size[0];
    new_rast_state.viewport[3] = sh_list.shadow_map_size[1];

    new_rast_state.scissor.rect[0] = sh_list.scissor_test_pos[0];
    new_rast_state.scissor.rect[1] = sh_list.scissor_test_pos[1];
    new_rast_state.scissor.rect[2] = sh_list.scissor_test_size[0];
    new_rast_state.scissor.rect[3] = sh_list.scissor_test_size[1];

    new_rast_state.ApplyChanged(rast_state);
    rast_state = new_rast_state;
}
} // namespace RpShadowMapsInternal

void RpShadowMaps::DrawShadowMaps(RpBuilder &builder) {
    using namespace RpShadowMapsInternal;

    Ren::RastState rast_state;
    rast_state.depth_test.enabled = true;
    rast_state.depth_test.func = Ren::eTestFunc::Less;

    rast_state.cull_face.enabled = false;
    rast_state.blend.enabled = false;

    rast_state.polygon_offset.enabled = true;

    rast_state.scissor.enabled = true;

    rast_state.Apply();

    Ren::Context &ctx = builder.ctx();

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

    RpAllocBuf &instances_buf = builder.GetReadBuffer(instances_buf_);
    assert(instances_buf.tbos[orphan_index_]);

    ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, REN_INST_BUF_SLOT,
                               GLuint(instances_buf.tbos[orphan_index_]->id()));

    RpAllocBuf &unif_shared_data_buf = builder.GetReadBuffer(shared_data_buf_);
    glBindBufferRange(GL_UNIFORM_BUFFER, REN_UB_SHARED_DATA_LOC,
                      GLuint(unif_shared_data_buf.ref->id()),
                      orphan_index_ * SharedDataBlockSize, sizeof(SharedDataBlock));
    assert(orphan_index_ * SharedDataBlockSize %
               builder.ctx().capabilities.unif_buf_offset_alignment ==
           0);

    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_NOISE_TEX_SLOT, noise_tex_.id);

    glBindFramebuffer(GL_FRAMEBUFFER, shadow_fb_.id());

    bool region_cleared[REN_MAX_SHADOWMAPS_TOTAL] = {};

    // draw opaque objects
    glBindVertexArray(depth_pass_solid_vao_.id());
    glUseProgram(shadow_solid_prog_->id());

    int draw_calls_count = 0;

    for (int i = 0; i < int(shadow_lists_.count); i++) {
        const ShadowList &sh_list = shadow_lists_.data[i];
        if (!sh_list.shadow_batch_count) {
            continue;
        }

        _adjust_bias_and_viewport(rast_state, sh_list);

        { // clear buffer region
            glClear(GL_DEPTH_BUFFER_BIT);
            region_cleared[i] = true;
        }

        glUniformMatrix4fv(REN_U_M_MATRIX_LOC, 1, GL_FALSE,
                           Ren::ValuePtr(shadow_regions_.data[i].clip_from_world));

        for (uint32_t j = sh_list.shadow_batch_start;
             j < sh_list.shadow_batch_start + sh_list.shadow_batch_count; j++) {
            const DepthDrawBatch &batch =
                shadow_batches_.data[shadow_batch_indices_.data[j]];
            if (!batch.instance_count || batch.alpha_test_bit ||
                batch.type_bits == DepthDrawBatch::TypeVege) {
                continue;
            }

            glUniform4iv(REN_U_INSTANCES_LOC, (batch.instance_count + 3) / 4,
                         &batch.instance_indices[0]);

            glDrawElementsInstancedBaseVertex(
                GL_TRIANGLES, batch.indices_count, GL_UNSIGNED_INT,
                (const GLvoid *)uintptr_t(batch.indices_offset * sizeof(uint32_t)),
                (GLsizei)batch.instance_count, (GLint)batch.base_vertex);
            ++draw_calls_count;
        }
    }

    // draw opaque vegetation
    glBindVertexArray(depth_pass_vege_solid_vao_.id());
    glUseProgram(shadow_vege_solid_prog_->id());

    for (int i = 0; i < int(shadow_lists_.count); i++) {
        const ShadowList &sh_list = shadow_lists_.data[i];
        if (!sh_list.shadow_batch_count) {
            continue;
        }

        _adjust_bias_and_viewport(rast_state, sh_list);

        if (!region_cleared[i]) {
            glClear(GL_DEPTH_BUFFER_BIT);
            region_cleared[i] = true;
        }

        glUniformMatrix4fv(REN_U_M_MATRIX_LOC, 1, GL_FALSE,
                           Ren::ValuePtr(shadow_regions_.data[i].clip_from_world));

        for (uint32_t j = sh_list.shadow_batch_start;
             j < sh_list.shadow_batch_start + sh_list.shadow_batch_count; j++) {
            const DepthDrawBatch &batch =
                shadow_batches_.data[shadow_batch_indices_.data[j]];
            if (!batch.instance_count || batch.alpha_test_bit ||
                batch.type_bits != DepthDrawBatch::TypeVege) {
                continue;
            }

            glUniform4iv(REN_U_INSTANCES_LOC, (batch.instance_count + 3) / 4,
                         &batch.instance_indices[0]);

            glDrawElementsInstancedBaseVertex(
                GL_TRIANGLES, batch.indices_count, GL_UNSIGNED_INT,
                (const GLvoid *)uintptr_t(batch.indices_offset * sizeof(uint32_t)),
                (GLsizei)batch.instance_count, (GLint)batch.base_vertex);
            ++draw_calls_count;
        }
    }

    // draw transparent (alpha-tested) objects
    glBindVertexArray(depth_pass_transp_vao_.id());
    glUseProgram(shadow_transp_prog_->id());

    for (int i = 0; i < int(shadow_lists_.count); i++) {
        const ShadowList &sh_list = shadow_lists_.data[i];
        if (!sh_list.shadow_batch_count) {
            continue;
        }

        _adjust_bias_and_viewport(rast_state, sh_list);

        if (!region_cleared[i]) {
            glClear(GL_DEPTH_BUFFER_BIT);
            region_cleared[i] = true;
        }

        glUniformMatrix4fv(REN_U_M_MATRIX_LOC, 1, GL_FALSE,
                           Ren::ValuePtr(shadow_regions_.data[i].clip_from_world));

        uint32_t cur_mat_id = 0xffffffff;

        for (uint32_t j = sh_list.shadow_batch_start;
             j < sh_list.shadow_batch_start + sh_list.shadow_batch_count; j++) {
            const DepthDrawBatch &batch =
                shadow_batches_.data[shadow_batch_indices_.data[j]];
            if (!batch.instance_count || !batch.alpha_test_bit ||
                batch.type_bits == DepthDrawBatch::TypeVege) {
                continue;
            }

            if (batch.mat_id != cur_mat_id) {
                const Ren::Material &mat = materials_->at(batch.mat_id);
                if (ctx.capabilities.bindless_texture) {
                    glUniform1ui(REN_U_MAT_INDEX_LOC, batch.mat_id);
                } else {
                    _bind_texture0_and_sampler0(builder.ctx(), mat,
                                                builder.temp_samplers);
                }
                cur_mat_id = batch.mat_id;
            }

            glUniform4iv(REN_U_INSTANCES_LOC, (batch.instance_count + 3) / 4,
                         &batch.instance_indices[0]);

            glDrawElementsInstancedBaseVertex(
                GL_TRIANGLES, batch.indices_count, GL_UNSIGNED_INT,
                (const GLvoid *)uintptr_t(batch.indices_offset * sizeof(uint32_t)),
                (GLsizei)batch.instance_count, (GLint)batch.base_vertex);
            ++draw_calls_count;
        }
    }

    // draw transparent (alpha-tested) vegetation
    glBindVertexArray(depth_pass_vege_transp_vao_.id());
    glUseProgram(shadow_vege_transp_prog_->id());

    for (int i = 0; i < int(shadow_lists_.count); i++) {
        const ShadowList &sh_list = shadow_lists_.data[i];
        if (!sh_list.shadow_batch_count) {
            continue;
        }

        _adjust_bias_and_viewport(rast_state, sh_list);

        if (!region_cleared[i]) {
            glClear(GL_DEPTH_BUFFER_BIT);
            region_cleared[i] = true;
        }

        glUniformMatrix4fv(REN_U_M_MATRIX_LOC, 1, GL_FALSE,
                           Ren::ValuePtr(shadow_regions_.data[i].clip_from_world));

        uint32_t cur_mat_id = 0xffffffff;

        for (uint32_t j = sh_list.shadow_batch_start;
             j < sh_list.shadow_batch_start + sh_list.shadow_batch_count; j++) {
            const DepthDrawBatch &batch =
                shadow_batches_.data[shadow_batch_indices_.data[j]];
            if (!batch.instance_count || !batch.alpha_test_bit ||
                batch.type_bits != DepthDrawBatch::TypeVege) {
                continue;
            }

            if (batch.mat_id != cur_mat_id) {
                const Ren::Material &mat = materials_->at(batch.mat_id);
                if (ctx.capabilities.bindless_texture) {
                    glUniform1ui(REN_U_MAT_INDEX_LOC, batch.mat_id);
                } else {
                    _bind_texture0_and_sampler0(builder.ctx(), mat,
                                                builder.temp_samplers);
                }
                cur_mat_id = batch.mat_id;
            }

            glUniform4iv(REN_U_INSTANCES_LOC, (batch.instance_count + 3) / 4,
                         &batch.instance_indices[0]);

            glDrawElementsInstancedBaseVertex(
                GL_TRIANGLES, batch.indices_count, GL_UNSIGNED_INT,
                (const GLvoid *)uintptr_t(batch.indices_offset * sizeof(uint32_t)),
                (GLsizei)batch.instance_count, (GLint)batch.base_vertex);
            ++draw_calls_count;
        }
    }

    glDisable(GL_SCISSOR_TEST);
    glPolygonOffset(0.0f, 0.0f);
    glDisable(GL_POLYGON_OFFSET_FILL);

    glBindVertexArray(0);
    Ren::GLUnbindSamplers(0, 1);

    (void)draw_calls_count;
}
