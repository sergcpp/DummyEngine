#include "ExShadowDepth.h"

#include <Ren/Context.h>
#include <Ren/DebugMarker.h>
#include <Ren/Gl/GL.h>
#include <Ren/RastState.h>

#include "../Renderer_DrawList.h"
#include "../framegraph/FgBuilder.h"
#include "../shaders/shadow_interface.h"

namespace ExSharedInternal {
uint32_t _draw_range(Ren::Span<const uint32_t> zfill_batch_indices,
                     Ren::Span<const Eng::basic_draw_batch_t> zfill_batches, uint32_t i, uint64_t mask,
                     int *draws_count);
uint32_t _draw_range_ext(const Eng::FgContext &fg, const Ren::ImageMain &white_tex,
                         Ren::Span<const uint32_t> batch_indices, Ren::Span<const Eng::basic_draw_batch_t> batches,
                         uint32_t i, uint64_t mask, uint32_t &cur_mat_id, int *draws_count);
void _bind_texture4_and_sampler4(Ren::Context &ctx, const Ren::MaterialMain &mat,
                                 Ren::SmallVectorImpl<Ren::SamplerHandle> &temp_samplers);
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

void Eng::ExShadowDepth::DrawShadowMaps(const FgContext &fg, const Ren::ImageRWHandle shadow_depth) {
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

    const Ren::ApiContext &api = fg.ren_ctx().api();

    const Ren::BufferROHandle attrib_bufs[] = {fg.AccessROBuffer(vtx_buf1_), fg.AccessROBuffer(vtx_buf2_)};
    const Ren::BufferROHandle ndx_buf = fg.AccessROBuffer(ndx_buf_);

    const Ren::BufferROHandle unif_shared_data = fg.AccessROBuffer(shared_data_);
    const Ren::BufferROHandle instances = fg.AccessROBuffer(instances_);
    const Ren::BufferROHandle instance_indices = fg.AccessROBuffer(instance_indices_);
    const Ren::BufferROHandle materials = fg.AccessROBuffer(materials_);

    const Ren::ImageROHandle noise = fg.AccessROImage(noise_);
    const Ren::ImageROHandle dummy_white = fg.AccessROImage(dummy_white_);

    const Ren::StoragesRef &storages = fg.storages();

    const Ren::BufferMain &materials_main = storages.buffers[materials].first;
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BIND_MATERIALS_BUF, GLuint(materials_main.buf));
    if (fg.ren_ctx().capabilities.bindless_texture) {
        const Ren::BufferMain &buf_main = storages.buffers[bindless_tex_->rt_inline_textures.buf].first;
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BIND_BINDLESS_TEX, GLuint(buf_main.buf));
    }

    const Ren::BufferMain &instances_main = storages.buffers[instances].first;
    ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, BIND_INST_BUF, GLuint(instances_main.views[0].second));
    const Ren::BufferMain &instance_indices_main = storages.buffers[instance_indices].first;
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BIND_INST_NDX_BUF, GLuint(instance_indices_main.buf));

    const Ren::BufferMain &unif_shared_data_main = storages.buffers[unif_shared_data].first;
    glBindBufferBase(GL_UNIFORM_BUFFER, BIND_UB_SHARED_DATA_BUF, GLuint(unif_shared_data_main.buf));

    const Ren::ImageMain &noise_main = storages.images[noise].first;
    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, BIND_NOISE_TEX, noise_main.img);

    const Ren::ImageMain &dummy_white_main = storages.images[dummy_white].first;

    const Ren::PipelineMain *pi_solid_main[3] = {&storages.pipelines[pi_solid_[0]].first,
                                                 &storages.pipelines[pi_solid_[1]].first,
                                                 &storages.pipelines[pi_solid_[2]].first};

    const Ren::FramebufferHandle fb_main =
        fg.FindOrCreateFramebuffer(pi_solid_main[0]->render_pass, shadow_depth, {}, {});
    glBindFramebuffer(GL_FRAMEBUFFER, storages.framebuffers[fb_main].first.id);

    glClearDepthf(0.0f);

    bool region_cleared[MAX_SHADOWMAPS_TOTAL] = {};

    Ren::SmallVector<uint32_t, 32> batch_points((*p_list_)->shadow_lists.count, 0);

    [[maybe_unused]] int draw_calls_count = 0;

    { // draw opaque objects
        Ren::DebugMarker _(api, fg.cmd_buf(), "STATIC-SOLID");

        const Ren::VertexInput &vi = storages.vtx_inputs[pi_solid_main[0]->vtx_input];
        VertexInput_BindBuffers(api, vi, storages.buffers, attrib_bufs, ndx_buf);

        static const uint64_t BitFlags[] = {0, BDB::BitBackSided, BDB::BitTwoSided};
        for (int pi = 0; pi < 3; ++pi) {
            glUseProgram(storages.programs[pi_solid_main[pi]->prog].first.id);

            Ren::RastState rast_state = fg.rast_state();
            rast_state.poly.cull = pi_solid_main[pi]->rast_state.poly.cull;
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

    const Ren::PipelineMain *pi_alpha_main[3] = {&storages.pipelines[pi_alpha_[0]].first,
                                                 &storages.pipelines[pi_alpha_[1]].first,
                                                 &storages.pipelines[pi_alpha_[2]].first};

    { // draw transparent (alpha-tested) objects
        Ren::DebugMarker _(api, fg.cmd_buf(), "STATIC-ALPHA");

        const Ren::VertexInput &vi = storages.vtx_inputs[pi_alpha_main[0]->vtx_input];
        VertexInput_BindBuffers(api, vi, storages.buffers, attrib_bufs, ndx_buf);

        static const uint64_t BitFlags[] = {BDB::BitAlphaTest, BDB::BitAlphaTest | BDB::BitBackSided,
                                            BDB::BitAlphaTest | BDB::BitTwoSided};
        for (int pi = 0; pi < 3; ++pi) {
            glUseProgram(storages.programs[pi_alpha_main[pi]->prog].first.id);

            Ren::RastState rast_state = fg.rast_state();
            rast_state.poly.cull = pi_alpha_main[pi]->rast_state.poly.cull;
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
                                   ValuePtr((*p_list_)->shadow_regions.data[i].clip_from_world));

                Ren::Span<const uint32_t> batch_indices = {
                    (*p_list_)->shadow_batch_indices.data() + sh_list.shadow_batch_start, sh_list.shadow_batch_count};

                uint32_t cur_mat_id = 0xffffffff;

                uint32_t j = batch_points[i];
                j = _draw_range_ext(fg, dummy_white_main, batch_indices, (*p_list_)->shadow_batches, j, BitFlags[pi],
                                    cur_mat_id, &draw_calls_count);
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
