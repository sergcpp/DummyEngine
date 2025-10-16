#include "ExShadowDepth.h"

#include <Ren/Context.h>
#include <Ren/DebugMarker.h>
#include <Ren/GL.h>
#include <Ren/RastState.h>

#include "../Renderer_Structs.h"
#include "../shaders/shadow_interface.h"

namespace ExSharedInternal {
uint32_t _draw_range(Ren::Span<const uint32_t> zfill_batch_indices,
                     Ren::Span<const Eng::basic_draw_batch_t> zfill_batches, uint32_t i, uint64_t mask,
                     int *draws_count);
uint32_t _draw_range_ext(Eng::FgContext &fg, const Ren::MaterialStorage *materials,
                         Ren::Span<const uint32_t> batch_indices, Ren::Span<const Eng::basic_draw_batch_t> batches,
                         uint32_t i, uint64_t mask, uint32_t &cur_mat_id, int *draws_count);
void _bind_texture4_and_sampler4(Ren::Context &ctx, const Ren::Material &mat,
                                 Ren::SmallVectorImpl<Ren::SamplerRef> &temp_samplers);
} // namespace ExSharedInternal
namespace ExShadowDepthInternal {
using namespace ExSharedInternal;

void _adjust_bias_and_viewport(Ren::RastState &rast_state, const Eng::shadow_list_t &sh_list) {
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
} // namespace ExShadowDepthInternal

void Eng::ExShadowDepth::DrawShadowMaps(FgContext &fg) {
    using namespace ExSharedInternal;
    using namespace ExShadowDepthInternal;

    using BDB = basic_draw_batch_t;

    Ren::RastState _rast_state;
    _rast_state.poly.cull = uint8_t(Ren::eCullFace::None);
    _rast_state.poly.depth_bias_mode = uint8_t(Ren::eDepthBiasMode::Dynamic);

    _rast_state.depth.test_enabled = true;
    _rast_state.depth.compare_op = unsigned(Ren::eCompareOp::Greater);
    _rast_state.scissor.enabled = true;
    _rast_state.blend.enabled = false;

    _rast_state.ApplyChanged(fg.rast_state());
    fg.rast_state() = _rast_state;

    Ren::ApiContext *api_ctx = fg.ren_ctx().api_ctx();

    FgAllocBuf &unif_shared_data_buf = fg.AccessROBuffer(shared_data_buf_);
    FgAllocBuf &instances_buf = fg.AccessROBuffer(instances_buf_);
    FgAllocBuf &instance_indices_buf = fg.AccessROBuffer(instance_indices_buf_);
    FgAllocBuf &materials_buf = fg.AccessROBuffer(materials_buf_);

    FgAllocTex &noise_tex = fg.AccessROTexture(noise_tex_);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BIND_MATERIALS_BUF, GLuint(materials_buf.ref->id()));
    if (fg.ren_ctx().capabilities.bindless_texture) {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BIND_BINDLESS_TEX,
                         GLuint(bindless_tex_->rt_inline_textures.buf->id()));
    }

    ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, BIND_INST_BUF, GLuint(instances_buf.ref->view(0).second));
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BIND_INST_NDX_BUF, GLuint(instance_indices_buf.ref->id()));

    glBindBufferBase(GL_UNIFORM_BUFFER, BIND_UB_SHARED_DATA_BUF, GLuint(unif_shared_data_buf.ref->id()));

    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, BIND_NOISE_TEX, noise_tex.ref->id());

    glBindFramebuffer(GL_FRAMEBUFFER, shadow_fb_.id());

    glClearDepthf(0.0f);

    bool region_cleared[MAX_SHADOWMAPS_TOTAL] = {};

    Ren::SmallVector<uint32_t, 32> batch_points((*p_list_)->shadow_lists.count, 0);

    [[maybe_unused]] int draw_calls_count = 0;

    { // draw opaque objects
        Ren::DebugMarker _(api_ctx, fg.cmd_buf(), "STATIC-SOLID");

        glBindVertexArray(pi_solid_[0]->vtx_input()->GetVAO());

        static const uint64_t BitFlags[] = {0, BDB::BitBackSided, BDB::BitTwoSided};
        for (int pi = 0; pi < 3; ++pi) {
            glUseProgram(pi_solid_[pi]->prog()->id());

            Ren::RastState rast_state = fg.rast_state();
            rast_state.poly.cull = pi_solid_[pi]->rast_state().poly.cull;
            rast_state.ApplyChanged(fg.rast_state());

            for (int i = 0; i < int((*p_list_)->shadow_lists.count); i++) {
                const shadow_list_t &sh_list = (*p_list_)->shadow_lists.data[i];
                if (!sh_list.shadow_batch_count) {
                    continue;
                }

                _adjust_bias_and_viewport(rast_state, sh_list);

                if (!std::exchange(region_cleared[i], true)) {
                    glClear(GL_DEPTH_BUFFER_BIT);
                }

                glUniformMatrix4fv(Shadow::U_M_MATRIX_LOC, 1, GL_FALSE,
                                   Ren::ValuePtr((*p_list_)->shadow_regions.data[i].clip_from_world));

                Ren::Span<const uint32_t> batch_indices = {
                    (*p_list_)->shadow_batch_indices.data() + sh_list.shadow_batch_start, sh_list.shadow_batch_count};

                uint32_t j = batch_points[i];
                j = _draw_range(batch_indices, (*p_list_)->shadow_batches, j, BitFlags[pi], &draw_calls_count);
                batch_points[i] = j;
            }

            fg.rast_state() = rast_state;
        }
    }

    // draw opaque vegetation
    /*glBindVertexArray(vi_depth_pass_vege_solid_->GetVAO());
    glUseProgram(pi_vege_solid_.prog()->id());

    for (int i = 0; i < int((*p_list_)->shadow_lists.count); i++) {
        const shadow_list_t &sh_list = (*p_list_)->shadow_lists.data[i];
        if (!sh_list.shadow_batch_count) {
            continue;
        }

        _adjust_bias_and_viewport(rast_state, sh_list);

        if (!region_cleared[i]) {
            glClear(GL_DEPTH_BUFFER_BIT);
            region_cleared[i] = true;
        }

        glUniformMatrix4fv(Shadow::U_M_MATRIX_LOC, 1, GL_FALSE,
                           Ren::ValuePtr((*p_list_)->shadow_regions.data[i].clip_from_world));

        for (uint32_t j = sh_list.shadow_batch_start; j < sh_list.shadow_batch_start + sh_list.shadow_batch_count;
             ++j) {
            const auto &batch = (*p_list_)->shadow_batches[(*p_list_)->shadow_batch_indices[j]];
            if (!batch.instance_count || batch.alpha_test_bit || batch.type_bits != basic_draw_batch_t::TypeVege) {
                continue;
            }

            glUniform1ui(REN_U_BASE_INSTANCE_LOC, batch.instance_start);

            glDrawElementsInstancedBaseVertex(GL_TRIANGLES, batch.indices_count, GL_UNSIGNED_INT,
                                              (const GLvoid *)uintptr_t(batch.indices_offset * sizeof(uint32_t)),
                                              (GLsizei)batch.instance_count, (GLint)batch.base_vertex);
            ++draw_calls_count;
        }
    }*/

    { // draw transparent (alpha-tested) objects
        Ren::DebugMarker _(api_ctx, fg.cmd_buf(), "STATIC-ALPHA");

        glBindVertexArray(pi_alpha_[0]->vtx_input()->GetVAO());

        static const uint64_t BitFlags[] = {BDB::BitAlphaTest, BDB::BitAlphaTest | BDB::BitBackSided,
                                            BDB::BitAlphaTest | BDB::BitTwoSided};
        for (int pi = 0; pi < 3; ++pi) {
            glUseProgram(pi_alpha_[pi]->prog()->id());

            Ren::RastState rast_state = fg.rast_state();
            rast_state.poly.cull = pi_alpha_[pi]->rast_state().poly.cull;
            rast_state.ApplyChanged(fg.rast_state());

            for (int i = 0; i < int((*p_list_)->shadow_lists.count); i++) {
                const shadow_list_t &sh_list = (*p_list_)->shadow_lists.data[i];
                if (!sh_list.shadow_batch_count) {
                    continue;
                }

                _adjust_bias_and_viewport(rast_state, sh_list);

                if (!std::exchange(region_cleared[i], true)) {
                    glClear(GL_DEPTH_BUFFER_BIT);
                }

                glUniformMatrix4fv(Shadow::U_M_MATRIX_LOC, 1, GL_FALSE,
                                   Ren::ValuePtr((*p_list_)->shadow_regions.data[i].clip_from_world));

                Ren::Span<const uint32_t> batch_indices = {
                    (*p_list_)->shadow_batch_indices.data() + sh_list.shadow_batch_start, sh_list.shadow_batch_count};

                uint32_t cur_mat_id = 0xffffffff;

                uint32_t j = batch_points[i];
                j = _draw_range_ext(fg, (*p_list_)->materials, batch_indices, (*p_list_)->shadow_batches, j,
                                    BitFlags[pi], cur_mat_id, &draw_calls_count);
                batch_points[i] = j;
            }

            fg.rast_state() = rast_state;
        }
    }

    // draw transparent (alpha-tested) vegetation
    /*glBindVertexArray(vi_depth_pass_vege_transp_->GetVAO());
    glUseProgram(pi_vege_transp_.prog()->id());

    for (int i = 0; i < int((*p_list_)->shadow_lists.count); i++) {
        const shadow_list_t &sh_list = (*p_list_)->shadow_lists.data[i];
        if (!sh_list.shadow_batch_count) {
            continue;
        }

        _adjust_bias_and_viewport(rast_state, sh_list);

        if (!region_cleared[i]) {
            glClear(GL_DEPTH_BUFFER_BIT);
            region_cleared[i] = true;
        }

        glUniformMatrix4fv(Shadow::U_M_MATRIX_LOC, 1, GL_FALSE,
                           Ren::ValuePtr((*p_list_)->shadow_regions.data[i].clip_from_world));

        uint32_t cur_mat_id = 0xffffffff;
        for (uint32_t j = sh_list.shadow_batch_start; j < sh_list.shadow_batch_start + sh_list.shadow_batch_count;
             ++j) {
            const auto &batch = (*p_list_)->shadow_batches[(*p_list_)->shadow_batch_indices[j]];
            if (!batch.instance_count || !batch.alpha_test_bit || batch.type_bits != basic_draw_batch_t::TypeVege) {
                continue;
            }

            if (!ctx.capabilities.bindless_texture && batch.material_index != cur_mat_id) {
                const Ren::Material &mat = (*p_list_)->materials->at(batch.material_index);
                _bind_texture4_and_sampler4(builder.ctx(), mat, builder.temp_samplers);
                cur_mat_id = batch.material_index;
            }

            glUniform1ui(REN_U_BASE_INSTANCE_LOC, batch.instance_start);

            glDrawElementsInstancedBaseVertex(GL_TRIANGLES, batch.indices_count, GL_UNSIGNED_INT,
                                              (const GLvoid *)uintptr_t(batch.indices_offset * sizeof(uint32_t)),
                                              GLsizei(batch.instance_count), GLint(batch.base_vertex));
            ++draw_calls_count;
        }
    }*/

    _rast_state.scissor.enabled = false;
    _rast_state.poly.depth_bias_mode = uint8_t(Ren::eDepthBiasMode::Disabled);
    _rast_state.depth_bias = {};
    _rast_state.ApplyChanged(fg.rast_state());
    fg.rast_state() = _rast_state;

    glClearDepthf(1.0f);
    glBindVertexArray(0);
    Ren::GLUnbindSamplers(0, 1);
}
