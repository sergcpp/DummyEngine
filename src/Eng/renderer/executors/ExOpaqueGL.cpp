#include "ExOpaque.h"

#include "../Renderer_Structs.h"

#include <Ren/Context.h>
#include <Ren/DebugMarker.h>
#include <Ren/GL.h>
#include <Ren/RastState.h>

namespace ExSharedInternal {
void _bind_textures_and_samplers(Ren::Context &ctx, const Ren::Material &mat,
                                 Ren::SmallVectorImpl<Ren::SamplerRef> &temp_samplers) {
    assert(mat.textures.size() == mat.samplers.size());
    for (int j = 0; j < int(mat.textures.size()); ++j) {
        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, Eng::BIND_MAT_TEX0 + j, mat.textures[j]->id());
        glBindSampler(Eng::BIND_MAT_TEX0 + j, mat.samplers[j]->id());
    }
}
uint32_t _draw_list_range_full(Eng::FgBuilder &builder, const Ren::MaterialStorage &materials,
                               const Ren::Pipeline pipelines[], Ren::Span<const Eng::custom_draw_batch_t> main_batches,
                               Ren::Span<const uint32_t> main_batch_indices, uint32_t i, uint64_t mask,
                               uint64_t &cur_mat_id, uint64_t &cur_pipe_id, uint64_t &cur_prog_id,
                               Eng::backend_info_t &backend_info) {
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

    for (; i < main_batch_indices.size(); i++) {
        const auto &batch = main_batches[main_batch_indices[i]];
        if ((batch.sort_key & Eng::custom_draw_batch_t::FlagBits) != mask) {
            break;
        }

        if (!batch.instance_count) {
            continue;
        }

        if (cur_pipe_id != batch.pipe_id) {
            if (cur_prog_id != pipelines[batch.pipe_id].prog().index()) {
                const Ren::Program *p = pipelines[batch.pipe_id].prog().get();
                glUseProgram(p->id());

                if (p->has_tessellation()) {
                    cur_primitive = GL_PATCHES;
                } else {
                    cur_primitive = GL_TRIANGLES;
                }
                cur_prog_id = pipelines[batch.pipe_id].prog().index();
            }
        }

        if (!ctx.capabilities.bindless_texture && cur_mat_id != batch.mat_id) {
            const Ren::Material &mat = materials.at(batch.mat_id);
            _bind_textures_and_samplers(builder.ctx(), mat, builder.temp_samplers);
        }

        cur_pipe_id = batch.pipe_id;
        cur_mat_id = batch.mat_id;

        glUniform1ui(Eng::REN_U_BASE_INSTANCE_LOC, batch.instance_start);

        glDrawElementsInstancedBaseVertex(cur_primitive, batch.indices_count, GL_UNSIGNED_INT,
                                          (const GLvoid *)uintptr_t(batch.indices_offset * sizeof(uint32_t)),
                                          GLsizei(batch.instance_count), GLint(batch.base_vertex));
        backend_info.opaque_draw_calls_count++;
        backend_info.tris_rendered += (batch.indices_count / 3) * batch.instance_count;
    }

    return i;
}

uint32_t _draw_list_range_full_rev(Eng::FgBuilder &builder, const Ren::MaterialStorage &materials,
                                   const Ren::Pipeline pipelines[],
                                   Ren::Span<const Eng::custom_draw_batch_t> main_batches,
                                   Ren::Span<const uint32_t> main_batch_indices, uint32_t ndx, uint64_t mask,
                                   uint64_t &cur_mat_id, uint64_t &cur_pipe_id, uint64_t &cur_prog_id,
                                   Eng::backend_info_t &backend_info) {
    auto &ctx = builder.ctx();

    int i = int(ndx);
    for (; i >= 0; i--) {
        const auto &batch = main_batches[main_batch_indices[i]];
        if ((batch.sort_key & Eng::custom_draw_batch_t::FlagBits) != mask) {
            break;
        }

        if (!batch.instance_count) {
            continue;
        }

        if (cur_pipe_id != batch.pipe_id) {
            if (cur_prog_id != pipelines[batch.pipe_id].prog().index()) {
                const Ren::Program *p = pipelines[batch.pipe_id].prog().get();
                glUseProgram(p->id());

                cur_prog_id = pipelines[batch.pipe_id].prog().index();
            }
        }

        if (!ctx.capabilities.bindless_texture && cur_mat_id != batch.mat_id) {
            const Ren::Material &mat = materials.at(batch.mat_id);
            _bind_textures_and_samplers(builder.ctx(), mat, builder.temp_samplers);
        }

        cur_pipe_id = batch.pipe_id;
        cur_mat_id = batch.mat_id;

        glUniform1ui(Eng::REN_U_BASE_INSTANCE_LOC, batch.instance_start);

        glDrawElementsInstancedBaseVertex(GL_TRIANGLES, batch.indices_count, GL_UNSIGNED_INT,
                                          (const GLvoid *)uintptr_t(batch.indices_offset * sizeof(uint32_t)),
                                          GLsizei(batch.instance_count), GLint(batch.base_vertex));
        backend_info.opaque_draw_calls_count++;
        backend_info.tris_rendered += (batch.indices_count / 3) * batch.instance_count;
    }

    return uint32_t(i);
}
} // namespace ExSharedInternal

void Eng::ExOpaque::DrawOpaque(FgBuilder &builder) {
    using namespace ExSharedInternal;

    Ren::RastState rast_state;
    rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

    if ((*p_list_)->render_settings.debug_wireframe) {
        rast_state.poly.mode = uint8_t(Ren::ePolygonMode::Line);
    } else {
        rast_state.poly.mode = uint8_t(Ren::ePolygonMode::Fill);
    }

    rast_state.depth.test_enabled = true;
    if (!(*p_list_)->render_settings.debug_wireframe) {
        rast_state.depth.compare_op = unsigned(Ren::eCompareOp::Equal);
    } else {
        rast_state.depth.compare_op = unsigned(Ren::eCompareOp::LEqual);
    }

    // Bind main buffer for drawing
    glBindFramebuffer(GL_FRAMEBUFFER, opaque_draw_fb_[0][fb_to_use_].id());

    rast_state.viewport[2] = view_state_->act_res[0];
    rast_state.viewport[3] = view_state_->act_res[1];

    rast_state.ApplyChanged(builder.rast_state());
    builder.rast_state() = rast_state;

    glBindVertexArray(draw_pass_vi_->GetVAO());

    auto &ctx = builder.ctx();

    //
    // Bind resources (shadow atlas, lightmap, cells item data)
    //
    FgAllocBuf &instances_buf = builder.GetReadBuffer(instances_buf_);
    FgAllocBuf &instance_indices_buf = builder.GetReadBuffer(instance_indices_buf_);
    FgAllocBuf &unif_shared_data_buf = builder.GetReadBuffer(shared_data_buf_);
    FgAllocBuf &materials_buf = builder.GetReadBuffer(materials_buf_);
    FgAllocBuf &textures_buf = builder.GetReadBuffer(textures_buf_);
    FgAllocBuf &cells_buf = builder.GetReadBuffer(cells_buf_);
    FgAllocBuf &items_buf = builder.GetReadBuffer(items_buf_);
    FgAllocBuf &lights_buf = builder.GetReadBuffer(lights_buf_);
    FgAllocBuf &decals_buf = builder.GetReadBuffer(decals_buf_);

    FgAllocTex &shad_tex = builder.GetReadTexture(shad_tex_);
    FgAllocTex &ssao_tex = builder.GetReadTexture(ssao_tex_);
    FgAllocTex &brdf_lut = builder.GetReadTexture(brdf_lut_);
    FgAllocTex &noise_tex = builder.GetReadTexture(noise_tex_);
    FgAllocTex &cone_rt_lut = builder.GetReadTexture(cone_rt_lut_);

    FgAllocTex &dummy_black = builder.GetReadTexture(dummy_black_);

    FgAllocTex *lm_tex[4];
    for (int i = 0; i < 4; ++i) {
        if (lm_tex_[i]) {
            lm_tex[i] = &builder.GetReadTexture(lm_tex_[i]);
        } else {
            lm_tex[i] = &dummy_black;
        }
    }

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BIND_MATERIALS_BUF, GLuint(materials_buf.ref->id()));
    if (ctx.capabilities.bindless_texture) {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BIND_BINDLESS_TEX, GLuint(textures_buf.ref->id()));
    }

    glBindBufferBase(GL_UNIFORM_BUFFER, BIND_UB_SHARED_DATA_BUF, unif_shared_data_buf.ref->id());

    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, BIND_SHAD_TEX, shad_tex.ref->id());

    if ((*p_list_)->decals_atlas) {
        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, BIND_DECAL_TEX, (*p_list_)->decals_atlas->tex_id(0));
    }

    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, BIND_SSAO_TEX_SLOT, ssao_tex.ref->id());

    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, BIND_BRDF_LUT, brdf_lut.ref->id());

    for (int sh_l = 0; sh_l < 4; sh_l++) {
        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, BIND_LMAP_SH + sh_l, lm_tex[sh_l]->ref->id());
    }

    // ren_glBindTextureUnit_Comp(GL_TEXTURE_CUBE_MAP_ARRAY, BIND_ENV_TEX,
    //                            (*p_list_)->probe_storage ? (*p_list_)->probe_storage->handle().id : 0);

    ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, BIND_LIGHT_BUF, GLuint(lights_buf.ref->view(0).second));
    ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, BIND_DECAL_BUF, GLuint(decals_buf.ref->view(0).second));
    ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, BIND_CELLS_BUF, GLuint(cells_buf.ref->view(0).second));
    ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, BIND_ITEMS_BUF, GLuint(items_buf.ref->view(0).second));

    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, BIND_NOISE_TEX, noise_tex.ref->id());
    // ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, BIND_CONE_RT_LUT, cone_rt_lut.ref->id());

    ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, BIND_INST_BUF, GLuint(instances_buf.ref->view(0).second));
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BIND_INST_NDX_BUF, GLuint(instance_indices_buf.ref->id()));

    const Ren::Span<const custom_draw_batch_t> batches = {(*p_list_)->custom_batches};
    const Ren::Span<const uint32_t> batch_indices = {(*p_list_)->custom_batch_indices};
    const auto &materials = *(*p_list_)->materials;

    backend_info_t _dummy = {};

    { // actual drawing
        using CDB = custom_draw_batch_t;

        uint64_t cur_pipe_id = 0xffffffffffffffff;
        uint64_t cur_prog_id = 0xffffffffffffffff;
        uint64_t cur_mat_id = 0xffffffffffffffff;

        uint32_t i = 0;

        { // one-sided1
            Ren::DebugMarker _m(ctx.api_ctx(), ctx.current_cmd_buf(), "ONE-SIDED-1");

            rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            i = _draw_list_range_full(builder, materials, pipelines_, batches, batch_indices, i, 0ull, cur_mat_id,
                                      cur_pipe_id, cur_prog_id, _dummy);
        }

        { // two-sided1
            Ren::DebugMarker _m(ctx.api_ctx(), ctx.current_cmd_buf(), "TWO-SIDED-1");

            rast_state.poly.cull = uint8_t(Ren::eCullFace::None);
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            i = _draw_list_range_full(builder, materials, pipelines_, batches, batch_indices, i, CDB::BitTwoSided,
                                      cur_mat_id, cur_pipe_id, cur_prog_id, _dummy);
        }

        { // one-sided2
            Ren::DebugMarker _m(ctx.api_ctx(), ctx.current_cmd_buf(), "ONE-SIDED-2");

            rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            i = _draw_list_range_full(builder, materials, pipelines_, batches, batch_indices, i, CDB::BitAlphaTest,
                                      cur_mat_id, cur_pipe_id, cur_prog_id, _dummy);
        }

        { // two-sided2
            Ren::DebugMarker _m(ctx.api_ctx(), ctx.current_cmd_buf(), "TWO-SIDED-2");

            rast_state.poly.cull = uint8_t(Ren::eCullFace::None);
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            i = _draw_list_range_full(builder, materials, pipelines_, batches, batch_indices, i,
                                      CDB::BitAlphaTest | CDB::BitTwoSided, cur_mat_id, cur_pipe_id, cur_prog_id,
                                      _dummy);
        }

        { // two-sided-tested-blended
            Ren::DebugMarker _m(ctx.api_ctx(), ctx.current_cmd_buf(), "TWO-SIDED-TESTED-BLENDED");

            rast_state.poly.cull = uint8_t(Ren::eCullFace::None);
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            i = _draw_list_range_full_rev(builder, materials, pipelines_, batches, batch_indices,
                                          uint32_t(batch_indices.size() - 1),
                                          CDB::BitAlphaBlend | CDB::BitAlphaTest | CDB::BitTwoSided, cur_mat_id,
                                          cur_pipe_id, cur_prog_id, _dummy);
        }

        { // one-sided-tested-blended
            Ren::DebugMarker _m(ctx.api_ctx(), ctx.current_cmd_buf(), "ONE-SIDED-TESTED-BLENDED");

            rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            _draw_list_range_full_rev(builder, materials, pipelines_, batches, batch_indices, i,
                                      CDB::BitAlphaBlend | CDB::BitAlphaTest, cur_mat_id, cur_pipe_id, cur_prog_id,
                                      _dummy);
        }
    }

    Ren::GLUnbindSamplers(BIND_MAT_TEX0, 8);
}

Eng::ExOpaque::~ExOpaque() = default;