#include "RpShadowMaps.h"

#include <Ren/Context.h>
#include <Ren/GL.h>
#include <Ren/RastState.h>

#include "../Renderer_Structs.h"
#include "../assets/shaders/internal/shadow_interface.glsl"

namespace RpSharedInternal {
void _bind_texture0_and_sampler0(Ren::Context &ctx, const Ren::Material &mat,
                                 Ren::SmallVectorImpl<Ren::SamplerRef> &temp_samplers);
}
namespace RpShadowMapsInternal {
using namespace RpSharedInternal;

void _adjust_bias_and_viewport(Ren::RastState &rast_state, const ShadowList &sh_list) {
    Ren::RastState new_rast_state = rast_state;

    new_rast_state.depth_bias.slope_factor = sh_list.bias[0];
    new_rast_state.depth_bias.constant_offset = sh_list.bias[1];

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

void RpShadowMaps::DrawShadowMaps(RpBuilder &builder, RpAllocTex &shadowmap_tex) {
    using namespace RpShadowMapsInternal;

    Ren::RastState rast_state;
    rast_state.poly.cull = uint8_t(Ren::eCullFace::None);
    rast_state.poly.depth_bias_mode = uint8_t(Ren::eDepthBiasMode::Dynamic);

    rast_state.depth.test_enabled = true;
    rast_state.depth.compare_op = unsigned(Ren::eCompareOp::Less);
    rast_state.scissor.enabled = true;
    rast_state.blend.enabled = false;

    rast_state.ApplyChanged(builder.rast_state());
    builder.rast_state() = rast_state;

    Ren::Context &ctx = builder.ctx();
    Ren::ApiContext *api_ctx = ctx.api_ctx();

    RpAllocBuf &unif_shared_data_buf = builder.GetReadBuffer(shared_data_buf_);
    RpAllocBuf &instances_buf = builder.GetReadBuffer(instances_buf_);
    RpAllocBuf &instance_indices_buf = builder.GetReadBuffer(instance_indices_buf_);
    RpAllocBuf &materials_buf = builder.GetReadBuffer(materials_buf_);
    RpAllocBuf &textures_buf = builder.GetReadBuffer(textures_buf_);

    RpAllocTex &noise_tex = builder.GetReadTexture(noise_tex_);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, REN_MATERIALS_SLOT, GLuint(materials_buf.ref->id()));
    if (ctx.capabilities.bindless_texture) {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, REN_BINDLESS_TEX_SLOT, GLuint(textures_buf.ref->id()));
    }

    ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, REN_INST_BUF_SLOT, GLuint(instances_buf.tbos[0]->id()));
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, REN_INST_INDICES_BUF_SLOT, GLuint(instance_indices_buf.ref->id()));

    glBindBufferBase(GL_UNIFORM_BUFFER, REN_UB_SHARED_DATA_LOC, GLuint(unif_shared_data_buf.ref->id()));

    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_NOISE_TEX_SLOT, noise_tex.ref->id());

    glBindFramebuffer(GL_FRAMEBUFFER, shadow_fb_.id());

    bool region_cleared[REN_MAX_SHADOWMAPS_TOTAL] = {};

    // draw opaque objects
    glBindVertexArray(vi_depth_pass_solid_.gl_vao());
    glUseProgram(pi_solid_.prog()->id());

    int draw_calls_count = 0;

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

        glUniformMatrix4fv(Shadow::U_M_MATRIX_LOC, 1, GL_FALSE, Ren::ValuePtr(shadow_regions_.data[i].clip_from_world));

        for (uint32_t j = sh_list.shadow_batch_start; j < sh_list.shadow_batch_start + sh_list.shadow_batch_count;
             ++j) {
            const DepthDrawBatch &batch = shadow_batches_.data[shadow_batch_indices_.data[j]];
            if (!batch.instance_count || batch.alpha_test_bit || batch.type_bits == DepthDrawBatch::TypeVege) {
                continue;
            }

            glUniform1ui(REN_U_BASE_INSTANCE_LOC, batch.instance_start);

            glDrawElementsInstancedBaseVertex(GL_TRIANGLES, batch.indices_count, GL_UNSIGNED_INT,
                                              (const GLvoid *)uintptr_t(batch.indices_offset * sizeof(uint32_t)),
                                              (GLsizei)batch.instance_count, (GLint)batch.base_vertex);
            ++draw_calls_count;
        }
    }

    // draw opaque vegetation
    glBindVertexArray(vi_depth_pass_vege_solid_.gl_vao());
    glUseProgram(pi_vege_solid_.prog()->id());

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

        glUniformMatrix4fv(Shadow::U_M_MATRIX_LOC, 1, GL_FALSE, Ren::ValuePtr(shadow_regions_.data[i].clip_from_world));

        for (uint32_t j = sh_list.shadow_batch_start; j < sh_list.shadow_batch_start + sh_list.shadow_batch_count;
             ++j) {
            const DepthDrawBatch &batch = shadow_batches_.data[shadow_batch_indices_.data[j]];
            if (!batch.instance_count || batch.alpha_test_bit || batch.type_bits != DepthDrawBatch::TypeVege) {
                continue;
            }

            glUniform1ui(REN_U_BASE_INSTANCE_LOC, batch.instance_start);

            glDrawElementsInstancedBaseVertex(GL_TRIANGLES, batch.indices_count, GL_UNSIGNED_INT,
                                              (const GLvoid *)uintptr_t(batch.indices_offset * sizeof(uint32_t)),
                                              (GLsizei)batch.instance_count, (GLint)batch.base_vertex);
            ++draw_calls_count;
        }
    }

    // draw transparent (alpha-tested) objects
    glBindVertexArray(vi_depth_pass_transp_.gl_vao());
    glUseProgram(pi_transp_.prog()->id());

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

        glUniformMatrix4fv(Shadow::U_M_MATRIX_LOC, 1, GL_FALSE, Ren::ValuePtr(shadow_regions_.data[i].clip_from_world));

        uint32_t cur_mat_id = 0xffffffff;
        for (uint32_t j = sh_list.shadow_batch_start; j < sh_list.shadow_batch_start + sh_list.shadow_batch_count;
             ++j) {
            const DepthDrawBatch &batch = shadow_batches_.data[shadow_batch_indices_.data[j]];
            if (!batch.instance_count || !batch.alpha_test_bit || batch.type_bits == DepthDrawBatch::TypeVege) {
                continue;
            }

            if (!ctx.capabilities.bindless_texture && batch.material_index != cur_mat_id) {
                const Ren::Material &mat = materials_->at(batch.material_index);
                _bind_texture0_and_sampler0(builder.ctx(), mat, builder.temp_samplers);
                cur_mat_id = batch.material_index;
            }

            glUniform1ui(REN_U_BASE_INSTANCE_LOC, batch.instance_start);

            glDrawElementsInstancedBaseVertex(GL_TRIANGLES, batch.indices_count, GL_UNSIGNED_INT,
                                              (const GLvoid *)uintptr_t(batch.indices_offset * sizeof(uint32_t)),
                                              GLsizei(batch.instance_count), GLint(batch.base_vertex));
            ++draw_calls_count;
        }
    }

    // draw transparent (alpha-tested) vegetation
    glBindVertexArray(vi_depth_pass_vege_transp_.gl_vao());
    glUseProgram(pi_vege_transp_.prog()->id());

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

        glUniformMatrix4fv(Shadow::U_M_MATRIX_LOC, 1, GL_FALSE, Ren::ValuePtr(shadow_regions_.data[i].clip_from_world));

        uint32_t cur_mat_id = 0xffffffff;
        for (uint32_t j = sh_list.shadow_batch_start; j < sh_list.shadow_batch_start + sh_list.shadow_batch_count;
             ++j) {
            const DepthDrawBatch &batch = shadow_batches_.data[shadow_batch_indices_.data[j]];
            if (!batch.instance_count || !batch.alpha_test_bit || batch.type_bits != DepthDrawBatch::TypeVege) {
                continue;
            }

            if (!ctx.capabilities.bindless_texture && batch.material_index != cur_mat_id) {
                const Ren::Material &mat = materials_->at(batch.material_index);
                _bind_texture0_and_sampler0(builder.ctx(), mat, builder.temp_samplers);
                cur_mat_id = batch.material_index;
            }

            glUniform1ui(REN_U_BASE_INSTANCE_LOC, batch.instance_start);

            glDrawElementsInstancedBaseVertex(GL_TRIANGLES, batch.indices_count, GL_UNSIGNED_INT,
                                              (const GLvoid *)uintptr_t(batch.indices_offset * sizeof(uint32_t)),
                                              GLsizei(batch.instance_count), GLint(batch.base_vertex));
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
