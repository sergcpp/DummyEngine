#include "ExDepthFill.h"

#include "../Renderer_Structs.h"

#include <Ren/Context.h>
#include <Ren/DebugMarker.h>
#include <Ren/Gl/GL.h>
#include <Ren/RastState.h>

#include "../Renderer_DrawList.h"
#include "../framegraph/FgBuilder.h"

namespace ExSharedInternal {
void _bind_texture0_and_sampler0(const Ren::StoragesRef &storages, const Ren::MaterialMain &mat) {
    assert(mat.textures.size() >= 1 && mat.samplers.size() >= 1);
    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, Eng::BIND_MAT_TEX0, storages.images[mat.textures[0]].first.img);
    glBindSampler(Eng::BIND_MAT_TEX0, storages.samplers[mat.samplers[0]].id);
}
void _bind_texture3_and_sampler3(const Ren::StoragesRef &storages, const Ren::MaterialMain &mat,
                                 const Ren::ImageMain &white_tex) {
    assert(mat.textures.size() >= 1 && mat.samplers.size() >= 1);
    if (mat.textures.size() > 3) {
        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, Eng::BIND_MAT_TEX3, storages.images[mat.textures[3]].first.img);
        glBindSampler(Eng::BIND_MAT_TEX3, storages.samplers[mat.samplers[3]].id);
    } else {
        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, Eng::BIND_MAT_TEX3, white_tex.img);
        glBindSampler(Eng::BIND_MAT_TEX3, 0);
    }
}
uint32_t _draw_range(Ren::Span<const uint32_t> zfill_batch_indices,
                     Ren::Span<const Eng::basic_draw_batch_t> zfill_batches, uint32_t i, uint64_t mask,
                     int *draws_count) {
    for (; i < zfill_batch_indices.size(); i++) {
        const auto &batch = zfill_batches[zfill_batch_indices[i]];
        if ((batch.sort_key & Eng::basic_draw_batch_t::FlagBits) != mask) {
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

uint32_t _draw_range_ext(const Eng::FgContext &fg, const Ren::ImageMain &white_tex,
                         Ren::Span<const uint32_t> batch_indices, Ren::Span<const Eng::basic_draw_batch_t> batches,
                         uint32_t i, uint64_t mask, uint32_t &cur_mat_id, int *draws_count) {
    auto &ren_ctx = fg.ren_ctx();
    const Ren::StoragesRef &storages = ren_ctx.storages();

    for (; i < batch_indices.size(); i++) {
        const auto &batch = batches[batch_indices[i]];
        if ((batch.sort_key & Eng::basic_draw_batch_t::FlagBits) != mask) {
            break;
        }

        if (!batch.instance_count) {
            continue;
        }

        if (!ren_ctx.capabilities.bindless_texture && batch.material_index != cur_mat_id) {
            const Ren::MaterialMain &mat = storages.materials.GetUnsafe(batch.material_index).first;
            _bind_texture3_and_sampler3(storages, mat, white_tex);
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

uint32_t _draw_range_ext3(const Eng::FgContext &fg, const Ren::ImageMain &white_tex,
                          Ren::Span<const uint32_t> batch_indices, Ren::Span<const Eng::basic_draw_batch_t> batches,
                          uint32_t i, const uint64_t mask, uint32_t &cur_mat_id, int *draws_count) {
    auto &ren_ctx = fg.ren_ctx();
    const Ren::StoragesRef &storages = ren_ctx.storages();

    for (; i < batch_indices.size(); i++) {
        const auto &batch = batches[batch_indices[i]];
        if ((batch.sort_key & Eng::basic_draw_batch_t::FlagBits) != mask) {
            break;
        }

        if (!batch.instance_count) {
            continue;
        }

        if (!ren_ctx.capabilities.bindless_texture && batch.material_index != cur_mat_id) {
            const Ren::MaterialMain &mat = storages.materials.GetUnsafe(batch.material_index).first;
            _bind_texture0_and_sampler0(storages, mat);
            _bind_texture3_and_sampler3(storages, mat, white_tex);
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
} // namespace ExSharedInternal

void Eng::ExDepthFill::DrawDepth(const FgContext &fg, const Ren::ImageRWHandle depth_tex,
                                 const Ren::ImageRWHandle velocity_tex) {
    using namespace ExSharedInternal;

    const Ren::BufferROHandle attrib_bufs[] = {fg.AccessROBuffer(vtx_buf1_), fg.AccessROBuffer(vtx_buf2_)};
    const Ren::BufferROHandle ndx_buf = fg.AccessROBuffer(ndx_buf_);

    const Ren::BufferROHandle unif_shared_data = fg.AccessROBuffer(shared_data_);
    const Ren::BufferROHandle instances = fg.AccessROBuffer(instances_);
    const Ren::BufferROHandle instance_indices = fg.AccessROBuffer(instance_indices_);
    const Ren::BufferROHandle materials = fg.AccessROBuffer(materials_);
    const Ren::ImageROHandle noise = fg.AccessROImage(noise_);
    const Ren::ImageROHandle dummy_white = fg.AccessROImage(dummy_white_);

    const Ren::StoragesRef &storages = fg.storages();

    const Ren::BufferMain &unif_shared_data_main = storages.buffers[unif_shared_data].first;
    glBindBufferBase(GL_UNIFORM_BUFFER, BIND_UB_SHARED_DATA_BUF, GLuint(unif_shared_data_main.buf));

    const Ren::BufferMain &instances_main = storages.buffers[instances].first;
    ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, BIND_INST_BUF, GLuint(instances_main.views[0].second));
    const Ren::BufferMain &instance_indices_main = storages.buffers[instance_indices].first;
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BIND_INST_NDX_BUF, GLuint(instance_indices_main.buf));

    const Ren::ImageMain &noise_main = storages.images[noise].first;
    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, BIND_NOISE_TEX, noise_main.img);

    const Ren::ImageMain &dummy_white_main = storages.images[dummy_white].first;

    const Ren::BufferMain &materials_main = storages.buffers[materials].first;
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BIND_MATERIALS_BUF, GLuint(materials_main.buf));
    if (fg.ren_ctx().capabilities.bindless_texture) {
        const Ren::BufferMain &buf_main = storages.buffers[bindless_tex_->rt_inline_textures.buf].first;
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BIND_BINDLESS_TEX, GLuint(buf_main.buf));
    }

    // Clear commands account for scissor region
    glDisable(GL_SCISSOR_TEST);
    fg.rast_state().scissor.enabled = false;

    const Ren::FramebufferHandle depth_fb = fg.FindOrCreateFramebuffer({}, depth_tex, depth_tex, {});

    const Ren::ImageRWHandle velocity_target[] = {velocity_tex};
    const Ren::FramebufferHandle depth_vel_fb = fg.FindOrCreateFramebuffer({}, depth_tex, depth_tex, velocity_target);

    glBindFramebuffer(GL_FRAMEBUFFER, storages.framebuffers[depth_fb].first.id);
    if (clear_depth_) {
        glClearDepthf(0.0f);
        glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        glClearDepthf(1.0f);
    } else {
        glClear(GL_STENCIL_BUFFER_BIT);
    }

    const Ren::Span<const basic_draw_batch_t> zfill_batches = {(*p_list_)->basic_batches};
    const Ren::Span<const uint32_t> zfill_batch_indices = {(*p_list_)->basic_batch_indices};

    int draws_count = 0;
    uint32_t i = 0;

    using BDB = basic_draw_batch_t;

    const Ren::ApiContext &api = fg.ren_ctx().api();

    { // solid meshes
        Ren::DebugMarker _m(api, fg.cmd_buf(), "STATIC-SOLID-SIMPLE");

        const Ren::PipelineMain *pi_static_solid_main[3] = {&storages.pipelines[pi_static_solid_[0]].first,
                                                            &storages.pipelines[pi_static_solid_[1]].first,
                                                            &storages.pipelines[pi_static_solid_[2]].first};

        const Ren::VertexInput &vi = storages.vtx_inputs[pi_static_solid_main[0]->vtx_input];
        VertexInput_BindBuffers(api, vi, storages.buffers, attrib_bufs, ndx_buf);
        glUseProgram(storages.programs[pi_static_solid_main[0]->prog].first.id);

        { // one-sided
            Ren::DebugMarker _mm(api, fg.cmd_buf(), "ONE-SIDED");

            Ren::RastState rast_state = pi_static_solid_main[0]->rast_state;
            rast_state.viewport[2] = view_state_->ren_res[0];
            rast_state.viewport[3] = view_state_->ren_res[1];
            rast_state.ApplyChanged(fg.rast_state());
            fg.rast_state() = rast_state;

            i = _draw_range(zfill_batch_indices, zfill_batches, i, 0u, &draws_count);

            rast_state = pi_static_solid_main[1]->rast_state;
            rast_state.viewport[2] = view_state_->ren_res[0];
            rast_state.viewport[3] = view_state_->ren_res[1];
            rast_state.ApplyChanged(fg.rast_state());
            fg.rast_state() = rast_state;

            i = _draw_range(zfill_batch_indices, zfill_batches, i, BDB::BitBackSided, &draws_count);
        }

        { // two-sided
            Ren::DebugMarker _mm(api, fg.cmd_buf(), "TWO-SIDED");

            Ren::RastState rast_state = pi_static_solid_main[2]->rast_state;
            rast_state.viewport[2] = view_state_->ren_res[0];
            rast_state.viewport[3] = view_state_->ren_res[1];
            rast_state.ApplyChanged(fg.rast_state());
            fg.rast_state() = rast_state;

            i = _draw_range(zfill_batch_indices, zfill_batches, i, BDB::BitTwoSided, &draws_count);
        }
    }

    // TODO: we can skip many things if TAA is disabled

    { // moving solid meshes (depth and velocity)
        Ren::DebugMarker _m(api, fg.cmd_buf(), "STATIC-SOLID-MOVING");

        const Ren::PipelineMain *pi_moving_solid_main[3] = {&storages.pipelines[pi_moving_solid_[0]].first,
                                                            &storages.pipelines[pi_moving_solid_[1]].first,
                                                            &storages.pipelines[pi_moving_solid_[2]].first};

        glBindFramebuffer(GL_FRAMEBUFFER, storages.framebuffers[depth_vel_fb].first.id);
        glUseProgram(storages.programs[pi_moving_solid_main[0]->prog].first.id);

        { // one-sided
            Ren::DebugMarker _mm(api, fg.cmd_buf(), "ONE-SIDED");

            Ren::RastState rast_state = pi_moving_solid_main[0]->rast_state;
            rast_state.viewport[2] = view_state_->ren_res[0];
            rast_state.viewport[3] = view_state_->ren_res[1];
            rast_state.ApplyChanged(fg.rast_state());
            fg.rast_state() = rast_state;

            i = _draw_range(zfill_batch_indices, zfill_batches, i, BDB::BitMoving, &draws_count);
        }

        { // two-sided
            Ren::DebugMarker _mm(api, fg.cmd_buf(), "TWO-SIDED");

            Ren::RastState rast_state = pi_moving_solid_main[2]->rast_state;
            rast_state.viewport[2] = view_state_->ren_res[0];
            rast_state.viewport[3] = view_state_->ren_res[1];
            rast_state.ApplyChanged(fg.rast_state());
            fg.rast_state() = rast_state;

            const uint64_t DrawMask = BDB::BitMoving | BDB::BitTwoSided;
            i = _draw_range(zfill_batch_indices, zfill_batches, i, DrawMask, &draws_count);
        }
    }

    { // alpha-tested objects
        uint32_t cur_mat_id = 0xffffffff;

        { // simple meshes (depth only)
            Ren::DebugMarker _m(api, fg.cmd_buf(), "STATIC-ALPHA-SIMPLE");

            const Ren::PipelineMain *pi_static_transp_main[3] = {&storages.pipelines[pi_static_transp_[0]].first,
                                                                 &storages.pipelines[pi_static_transp_[1]].first,
                                                                 &storages.pipelines[pi_static_transp_[2]].first};

            glBindFramebuffer(GL_FRAMEBUFFER, storages.framebuffers[depth_vel_fb].first.id);

            const Ren::VertexInput &vi = storages.vtx_inputs[pi_static_transp_main[0]->vtx_input];
            VertexInput_BindBuffers(api, vi, storages.buffers, attrib_bufs, ndx_buf);
            glUseProgram(storages.programs[pi_static_transp_main[0]->prog].first.id);

            { // one-sided
                Ren::DebugMarker _mm(api, fg.cmd_buf(), "ONE-SIDED");

                Ren::RastState rast_state = pi_static_transp_main[0]->rast_state;
                rast_state.viewport[2] = view_state_->ren_res[0];
                rast_state.viewport[3] = view_state_->ren_res[1];
                rast_state.ApplyChanged(fg.rast_state());
                fg.rast_state() = rast_state;

                i = _draw_range_ext(fg, dummy_white_main, zfill_batch_indices, zfill_batches, i, BDB::BitAlphaTest,
                                    cur_mat_id, &draws_count);
            }

            { // two-sided
                Ren::DebugMarker _mm(api, fg.cmd_buf(), "TWO-SIDED");

                Ren::RastState rast_state = pi_static_transp_main[2]->rast_state;
                rast_state.viewport[2] = view_state_->ren_res[0];
                rast_state.viewport[3] = view_state_->ren_res[1];
                rast_state.ApplyChanged(fg.rast_state());
                fg.rast_state() = rast_state;

                const uint64_t DrawMask = BDB::BitAlphaTest | BDB::BitTwoSided;
                i = _draw_range_ext(fg, dummy_white_main, zfill_batch_indices, zfill_batches, i, DrawMask, cur_mat_id,
                                    &draws_count);
            }
        }

        { // moving meshes (depth and velocity)
            Ren::DebugMarker _m(api, fg.cmd_buf(), "STATIC-ALPHA-MOVING");

            const Ren::PipelineMain *pi_moving_transp_main[3] = {&storages.pipelines[pi_moving_transp_[0]].first,
                                                                 &storages.pipelines[pi_moving_transp_[1]].first,
                                                                 &storages.pipelines[pi_moving_transp_[2]].first};

            glBindFramebuffer(GL_FRAMEBUFFER, storages.framebuffers[depth_vel_fb].first.id);
            glUseProgram(storages.programs[pi_moving_transp_main[0]->prog].first.id);

            { // one-sided
                Ren::DebugMarker _mm(api, fg.cmd_buf(), "ONE-SIDED");

                Ren::RastState rast_state = pi_moving_transp_main[0]->rast_state;
                rast_state.viewport[2] = view_state_->ren_res[0];
                rast_state.viewport[3] = view_state_->ren_res[1];
                rast_state.ApplyChanged(fg.rast_state());
                fg.rast_state() = rast_state;

                const uint64_t DrawMask = BDB::BitAlphaTest | BDB::BitMoving;
                i = _draw_range_ext(fg, dummy_white_main, zfill_batch_indices, zfill_batches, i, DrawMask, cur_mat_id,
                                    &draws_count);
            }

            { // two-sided
                Ren::DebugMarker _mm(api, fg.cmd_buf(), "TWO-SIDED");

                Ren::RastState rast_state = pi_moving_transp_main[2]->rast_state;
                rast_state.viewport[2] = view_state_->ren_res[0];
                rast_state.viewport[3] = view_state_->ren_res[1];
                rast_state.ApplyChanged(fg.rast_state());
                fg.rast_state() = rast_state;

                const uint64_t DrawMask = BDB::BitAlphaTest | BDB::BitMoving | BDB::BitTwoSided;
                i = _draw_range_ext(fg, dummy_white_main, zfill_batch_indices, zfill_batches, i, DrawMask, cur_mat_id,
                                    &draws_count);
            }
        }
    }

    { // static solid vegetation
        Ren::DebugMarker _m(api, fg.cmd_buf(), "VEGE-SOLID-SIMPLE");

        const Ren::PipelineMain *pi_vege_static_solid_main[2] = {&storages.pipelines[pi_vege_static_solid_[0]].first,
                                                                 &storages.pipelines[pi_vege_static_solid_[1]].first};

        glBindFramebuffer(GL_FRAMEBUFFER, storages.framebuffers[depth_vel_fb].first.id);

        const Ren::VertexInput &vi = storages.vtx_inputs[pi_vege_static_solid_main[0]->vtx_input];
        VertexInput_BindBuffers(api, vi, storages.buffers, attrib_bufs, ndx_buf);
        glUseProgram(storages.programs[pi_vege_static_solid_main[0]->prog].first.id);

        { // one-sided
            Ren::DebugMarker _mm(api, fg.cmd_buf(), "ONE-SIDED");

            Ren::RastState rast_state = pi_vege_static_solid_main[0]->rast_state;
            rast_state.viewport[2] = view_state_->ren_res[0];
            rast_state.viewport[3] = view_state_->ren_res[1];
            rast_state.ApplyChanged(fg.rast_state());
            fg.rast_state() = rast_state;

            i = _draw_range(zfill_batch_indices, zfill_batches, i, BDB::BitsVege, &draws_count);
        }

        { // two-sided
            Ren::DebugMarker _mm(api, fg.cmd_buf(), "TWO-SIDED");

            Ren::RastState rast_state = pi_vege_static_solid_main[1]->rast_state;
            rast_state.viewport[2] = view_state_->ren_res[0];
            rast_state.viewport[3] = view_state_->ren_res[1];
            rast_state.ApplyChanged(fg.rast_state());
            fg.rast_state() = rast_state;

            const uint64_t DrawMask = BDB::BitsVege | BDB::BitTwoSided;
            i = _draw_range(zfill_batch_indices, zfill_batches, i, DrawMask, &draws_count);
        }
    }

    { // moving solid vegetation (depth and velocity)
        Ren::DebugMarker _m(api, fg.cmd_buf(), "VEGE-SOLID-MOVING");

        const Ren::PipelineMain *pi_vege_moving_solid_main[2] = {&storages.pipelines[pi_vege_moving_solid_[0]].first,
                                                                 &storages.pipelines[pi_vege_moving_solid_[1]].first};

        glUseProgram(storages.programs[pi_vege_moving_solid_main[0]->prog].first.id);

        { // one-sided
            Ren::DebugMarker _mm(api, fg.cmd_buf(), "ONE-SIDED");

            Ren::RastState rast_state = pi_vege_moving_solid_main[0]->rast_state;
            rast_state.viewport[2] = view_state_->ren_res[0];
            rast_state.viewport[3] = view_state_->ren_res[1];
            rast_state.ApplyChanged(fg.rast_state());
            fg.rast_state() = rast_state;

            const uint64_t DrawMask = BDB::BitsVege | BDB::BitMoving;
            i = _draw_range(zfill_batch_indices, zfill_batches, i, DrawMask, &draws_count);
        }

        { // two-sided
            Ren::DebugMarker _mm(api, fg.cmd_buf(), "TWO-SIDED");

            Ren::RastState rast_state = pi_vege_moving_solid_main[1]->rast_state;
            rast_state.viewport[2] = view_state_->ren_res[0];
            rast_state.viewport[3] = view_state_->ren_res[1];
            rast_state.ApplyChanged(fg.rast_state());
            fg.rast_state() = rast_state;

            const uint64_t DrawMask = BDB::BitsVege | BDB::BitMoving | BDB::BitTwoSided;
            i = _draw_range(zfill_batch_indices, zfill_batches, i, DrawMask, &draws_count);
        }
    }

    { // static alpha-tested vegetation
        uint32_t cur_mat_id = 0xffffffff;

        { // static alpha-tested vegetation (depth and velocity)
            Ren::DebugMarker _m(api, fg.cmd_buf(), "VEGE-ALPHA-SIMPLE");

            const Ren::PipelineMain *pi_vege_static_transp_main[2] = {
                &storages.pipelines[pi_vege_static_transp_[0]].first,
                &storages.pipelines[pi_vege_static_transp_[1]].first};

            const Ren::VertexInput &vi = storages.vtx_inputs[pi_vege_static_transp_main[0]->vtx_input];
            VertexInput_BindBuffers(api, vi, storages.buffers, attrib_bufs, ndx_buf);
            glUseProgram(storages.programs[pi_vege_static_transp_main[0]->prog].first.id);

            { // one-sided
                Ren::DebugMarker _mm(api, fg.cmd_buf(), "ONE-SIDED");

                Ren::RastState rast_state = pi_vege_static_transp_main[0]->rast_state;
                rast_state.viewport[2] = view_state_->ren_res[0];
                rast_state.viewport[3] = view_state_->ren_res[1];
                rast_state.ApplyChanged(fg.rast_state());
                fg.rast_state() = rast_state;

                const uint64_t DrawMask = BDB::BitsVege | BDB::BitAlphaTest;
                i = _draw_range_ext(fg, dummy_white_main, zfill_batch_indices, zfill_batches, i, DrawMask, cur_mat_id,
                                    &draws_count);
            }

            { // two-sided
                Ren::DebugMarker _mm(api, fg.cmd_buf(), "TWO-SIDED");

                Ren::RastState rast_state = pi_vege_static_transp_main[1]->rast_state;
                rast_state.viewport[2] = view_state_->ren_res[0];
                rast_state.viewport[3] = view_state_->ren_res[1];
                rast_state.ApplyChanged(fg.rast_state());
                fg.rast_state() = rast_state;

                const uint64_t DrawMask = BDB::BitsVege | BDB::BitAlphaTest | BDB::BitTwoSided;
                i = _draw_range_ext(fg, dummy_white_main, zfill_batch_indices, zfill_batches, i, DrawMask, cur_mat_id,
                                    &draws_count);
            }
        }

        { // moving alpha-tested vegetation (depth and velocity)
            Ren::DebugMarker _m(api, fg.cmd_buf(), "VEGE-ALPHA-MOVING");

            const Ren::PipelineMain *pi_vege_moving_transp_main[2] = {
                &storages.pipelines[pi_vege_moving_transp_[0]].first,
                &storages.pipelines[pi_vege_moving_transp_[1]].first};

            glUseProgram(storages.programs[pi_vege_moving_transp_main[0]->prog].first.id);

            { // one-sided
                Ren::DebugMarker _mm(api, fg.cmd_buf(), "ONE-SIDED");

                Ren::RastState rast_state = pi_vege_moving_transp_main[0]->rast_state;
                rast_state.viewport[2] = view_state_->ren_res[0];
                rast_state.viewport[3] = view_state_->ren_res[1];
                rast_state.ApplyChanged(fg.rast_state());
                fg.rast_state() = rast_state;

                const uint64_t DrawMask = BDB::BitsVege | BDB::BitAlphaTest | BDB::BitMoving;
                i = _draw_range_ext(fg, dummy_white_main, zfill_batch_indices, zfill_batches, i, DrawMask, cur_mat_id,
                                    &draws_count);
            }

            { // two-sided
                Ren::DebugMarker _mm(api, fg.cmd_buf(), "TWO-SIDED");

                Ren::RastState rast_state = pi_vege_moving_transp_main[1]->rast_state;
                rast_state.viewport[2] = view_state_->ren_res[0];
                rast_state.viewport[3] = view_state_->ren_res[1];
                rast_state.ApplyChanged(fg.rast_state());
                fg.rast_state() = rast_state;

                const uint64_t DrawMask = BDB::BitsVege | BDB::BitAlphaTest | BDB::BitMoving | BDB::BitTwoSided;
                i = _draw_range_ext(fg, dummy_white_main, zfill_batch_indices, zfill_batches, i, DrawMask, cur_mat_id,
                                    &draws_count);
            }
        }
    }

    { // solid skinned meshes (depth and velocity)
        Ren::DebugMarker _m(api, fg.cmd_buf(), "SKIN-SOLID-SIMPLE");

        const Ren::PipelineMain *pi_skin_static_solid_main[2] = {&storages.pipelines[pi_skin_static_solid_[0]].first,
                                                                 &storages.pipelines[pi_skin_static_solid_[1]].first};

        const Ren::VertexInput &vi = storages.vtx_inputs[pi_skin_static_solid_main[0]->vtx_input];
        VertexInput_BindBuffers(api, vi, storages.buffers, attrib_bufs, ndx_buf);
        glUseProgram(storages.programs[pi_skin_static_solid_main[0]->prog].first.id);

        { // one-sided
            Ren::DebugMarker _mm(api, fg.cmd_buf(), "ONE-SIDED");

            Ren::RastState rast_state = pi_skin_static_solid_main[0]->rast_state;
            rast_state.viewport[2] = view_state_->ren_res[0];
            rast_state.viewport[3] = view_state_->ren_res[1];
            rast_state.ApplyChanged(fg.rast_state());
            fg.rast_state() = rast_state;

            i = _draw_range(zfill_batch_indices, zfill_batches, i, BDB::BitsSkinned, &draws_count);
        }

        { // two-sided
            Ren::DebugMarker _mm(api, fg.cmd_buf(), "TWO-SIDED");

            Ren::RastState rast_state = pi_skin_static_solid_main[1]->rast_state;
            rast_state.viewport[2] = view_state_->ren_res[0];
            rast_state.viewport[3] = view_state_->ren_res[1];
            rast_state.ApplyChanged(fg.rast_state());
            fg.rast_state() = rast_state;

            const uint64_t DrawMask = BDB::BitsSkinned | BDB::BitTwoSided;
            i = _draw_range(zfill_batch_indices, zfill_batches, i, DrawMask, &draws_count);
        }
    }

    { // moving solid skinned (depth and velocity)
        Ren::DebugMarker _m(api, fg.cmd_buf(), "SKIN-SOLID-MOVING");

        const Ren::PipelineMain *pi_skin_moving_solid_main[2] = {&storages.pipelines[pi_skin_moving_solid_[0]].first,
                                                                 &storages.pipelines[pi_skin_moving_solid_[1]].first};

        glUseProgram(storages.programs[pi_skin_moving_solid_main[0]->prog].first.id);

        { // one-sided
            Ren::DebugMarker _mm(api, fg.cmd_buf(), "ONE-SIDED");

            Ren::RastState rast_state = pi_skin_moving_solid_main[0]->rast_state;
            rast_state.viewport[2] = view_state_->ren_res[0];
            rast_state.viewport[3] = view_state_->ren_res[1];
            rast_state.ApplyChanged(fg.rast_state());
            fg.rast_state() = rast_state;

            const uint64_t DrawMask = BDB::BitsSkinned | BDB::BitMoving;
            i = _draw_range(zfill_batch_indices, zfill_batches, i, DrawMask, &draws_count);
        }

        { // two-sided
            Ren::DebugMarker _mm(api, fg.cmd_buf(), "TWO-SIDED");

            Ren::RastState rast_state = pi_skin_moving_solid_main[1]->rast_state;
            rast_state.viewport[2] = view_state_->ren_res[0];
            rast_state.viewport[3] = view_state_->ren_res[1];
            rast_state.ApplyChanged(fg.rast_state());
            fg.rast_state() = rast_state;

            const uint64_t DrawMask = BDB::BitsSkinned | BDB::BitMoving | BDB::BitTwoSided;
            i = _draw_range(zfill_batch_indices, zfill_batches, i, DrawMask, &draws_count);
        }
    }

    { // static alpha-tested skinned
        uint32_t cur_mat_id = 0xffffffff;

        { // simple alpha-tested skinned (depth and velocity)
            Ren::DebugMarker _m(api, fg.cmd_buf(), "SKIN-ALPHA-SIMPLE");

            const Ren::PipelineMain *pi_skin_static_transp_main[2] = {
                &storages.pipelines[pi_skin_static_transp_[0]].first,
                &storages.pipelines[pi_skin_static_transp_[1]].first};

            const Ren::VertexInput &vi = storages.vtx_inputs[pi_skin_static_transp_main[0]->vtx_input];
            VertexInput_BindBuffers(api, vi, storages.buffers, attrib_bufs, ndx_buf);
            glUseProgram(storages.programs[pi_skin_static_transp_main[0]->prog].first.id);

            { // one-sided
                Ren::DebugMarker _mm(api, fg.cmd_buf(), "ONE-SIDED");

                Ren::RastState rast_state = pi_skin_static_transp_main[0]->rast_state;
                rast_state.viewport[2] = view_state_->ren_res[0];
                rast_state.viewport[3] = view_state_->ren_res[1];
                rast_state.ApplyChanged(fg.rast_state());
                fg.rast_state() = rast_state;

                const uint64_t DrawMask = BDB::BitsSkinned | BDB::BitAlphaTest;
                i = _draw_range_ext(fg, dummy_white_main, zfill_batch_indices, zfill_batches, i, DrawMask, cur_mat_id,
                                    &draws_count);
            }

            { // two-sided
                Ren::DebugMarker _mm(api, fg.cmd_buf(), "TWO-SIDED");

                Ren::RastState rast_state = pi_skin_static_transp_main[1]->rast_state;
                rast_state.viewport[2] = view_state_->ren_res[0];
                rast_state.viewport[3] = view_state_->ren_res[1];
                rast_state.ApplyChanged(fg.rast_state());
                fg.rast_state() = rast_state;

                const uint64_t DrawMask = BDB::BitsSkinned | BDB::BitAlphaTest | BDB::BitTwoSided;
                i = _draw_range_ext(fg, dummy_white_main, zfill_batch_indices, zfill_batches, i, DrawMask, cur_mat_id,
                                    &draws_count);
            }
        }

        { // moving alpha-tested skinned (depth and velocity)
            Ren::DebugMarker _m(api, fg.cmd_buf(), "SKIN-ALPHA-MOVING");

            const Ren::PipelineMain *pi_skin_moving_transp_main[2] = {
                &storages.pipelines[pi_skin_moving_transp_[0]].first,
                &storages.pipelines[pi_skin_moving_transp_[1]].first};

            glUseProgram(storages.programs[pi_skin_moving_transp_main[0]->prog].first.id);

            { // one-sided
                Ren::DebugMarker _mm(api, fg.cmd_buf(), "ONE-SIDED");

                Ren::RastState rast_state = pi_skin_moving_transp_main[0]->rast_state;
                rast_state.viewport[2] = view_state_->ren_res[0];
                rast_state.viewport[3] = view_state_->ren_res[1];
                rast_state.ApplyChanged(fg.rast_state());
                fg.rast_state() = rast_state;

                const uint64_t DrawMask = BDB::BitsSkinned | BDB::BitAlphaTest | BDB::BitMoving;
                i = _draw_range_ext(fg, dummy_white_main, zfill_batch_indices, zfill_batches, i, DrawMask, cur_mat_id,
                                    &draws_count);
            }

            { // two-sided
                Ren::DebugMarker _mm(api, fg.cmd_buf(), "TWO-SIDED");

                Ren::RastState rast_state = pi_skin_moving_transp_main[1]->rast_state;
                rast_state.viewport[2] = view_state_->ren_res[0];
                rast_state.viewport[3] = view_state_->ren_res[1];
                rast_state.ApplyChanged(fg.rast_state());
                fg.rast_state() = rast_state;

                const uint64_t DrawMask = BDB::BitsSkinned | BDB::BitAlphaTest | BDB::BitMoving | BDB::BitTwoSided;
                i = _draw_range_ext(fg, dummy_white_main, zfill_batch_indices, zfill_batches, i, DrawMask, cur_mat_id,
                                    &draws_count);
            }
        }
    }

    Ren::GLUnbindSamplers(BIND_MAT_TEX0, 1);
}
