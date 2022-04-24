#include "RpOpaque.h"

#include "../Renderer_Structs.h"

#include <Ren/Context.h>
#include <Ren/DebugMarker.h>
#include <Ren/RastState.h>

namespace RpSharedInternal {
void _bind_textures_and_samplers(Ren::Context &ctx, const Ren::Material &mat,
                                 Ren::SmallVectorImpl<Ren::SamplerRef> &temp_samplers) {
    assert(mat.textures.size() == mat.samplers.size());
    for (int j = 0; j < int(mat.textures.size()); ++j) {
        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_MAT_TEX0_SLOT + j, mat.textures[j]->id());
        glBindSampler(REN_MAT_TEX0_SLOT + j, mat.samplers[j]->id());
    }
}
uint32_t _draw_list_range_full(RpBuilder &builder, const Ren::MaterialStorage &materials,
                               const Ren::Pipeline pipelines[], Ren::Span<const CustomDrawBatch> main_batches,
                               Ren::Span<const uint32_t> main_batch_indices, uint32_t i, uint64_t mask,
                               uint64_t &cur_mat_id, uint64_t &cur_pipe_id, uint64_t &cur_prog_id,
                               BackendInfo &backend_info) {
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
        if ((batch.sort_key & CustomDrawBatch::FlagBits) != mask) {
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

        glUniform1ui(REN_U_BASE_INSTANCE_LOC, batch.instance_start);

        glDrawElementsInstancedBaseVertex(cur_primitive, batch.indices_count, GL_UNSIGNED_INT,
                                          (const GLvoid *)uintptr_t(batch.indices_offset * sizeof(uint32_t)),
                                          GLsizei(batch.instance_count), GLint(batch.base_vertex));
        backend_info.opaque_draw_calls_count++;
        backend_info.tris_rendered += (batch.indices_count / 3) * batch.instance_count;
    }

    return i;
}

uint32_t _draw_list_range_full_rev(RpBuilder &builder, const Ren::MaterialStorage &materials,
                                   const Ren::Pipeline pipelines[], Ren::Span<const CustomDrawBatch> main_batches,
                                   Ren::Span<const uint32_t> main_batch_indices, uint32_t ndx, uint64_t mask,
                                   uint64_t &cur_mat_id, uint64_t &cur_pipe_id, uint64_t &cur_prog_id,
                                   BackendInfo &backend_info) {
    auto &ctx = builder.ctx();

    int i = int(ndx);
    for (; i >= 0; i--) {
        const auto &batch = main_batches[main_batch_indices[i]];
        if ((batch.sort_key & CustomDrawBatch::FlagBits) != mask) {
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

        glUniform1ui(REN_U_BASE_INSTANCE_LOC, batch.instance_start);

        glDrawElementsInstancedBaseVertex(GL_TRIANGLES, batch.indices_count, GL_UNSIGNED_INT,
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
    rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

    if ((*p_list_)->render_flags & DebugWireframe) {
        rast_state.poly.mode = uint8_t(Ren::ePolygonMode::Line);
    } else {
        rast_state.poly.mode = uint8_t(Ren::ePolygonMode::Fill);
    }

    rast_state.depth.test_enabled = true;
    if (((*p_list_)->render_flags & (EnableZFill | DebugWireframe)) == EnableZFill) {
        rast_state.depth.compare_op = unsigned(Ren::eCompareOp::Equal);
    } else {
        rast_state.depth.compare_op = unsigned(Ren::eCompareOp::LEqual);
    }

    // Bind main buffer for drawing
#if defined(REN_DIRECT_DRAWING)
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    rast_state.viewport[2] = view_state_->scr_res[0];
    rast_state.viewport[3] = view_state_->scr_res[1];
#else
    glBindFramebuffer(GL_FRAMEBUFFER, opaque_draw_fb_[0][fb_to_use_].id());

    rast_state.viewport[2] = view_state_->act_res[0];
    rast_state.viewport[3] = view_state_->act_res[1];
#endif

    rast_state.ApplyChanged(builder.rast_state());
    builder.rast_state() = rast_state;

    glBindVertexArray(draw_pass_vi_.gl_vao());

    auto &ctx = builder.ctx();

    //
    // Bind resources (shadow atlas, lightmap, cells item data)
    //
    RpAllocBuf &instances_buf = builder.GetReadBuffer(instances_buf_);
    RpAllocBuf &instance_indices_buf = builder.GetReadBuffer(instance_indices_buf_);
    RpAllocBuf &unif_shared_data_buf = builder.GetReadBuffer(shared_data_buf_);
    RpAllocBuf &materials_buf = builder.GetReadBuffer(materials_buf_);
    RpAllocBuf &textures_buf = builder.GetReadBuffer(textures_buf_);
    RpAllocBuf &cells_buf = builder.GetReadBuffer(cells_buf_);
    RpAllocBuf &items_buf = builder.GetReadBuffer(items_buf_);
    RpAllocBuf &lights_buf = builder.GetReadBuffer(lights_buf_);
    RpAllocBuf &decals_buf = builder.GetReadBuffer(decals_buf_);

    RpAllocTex &shad_tex = builder.GetReadTexture(shad_tex_);
    RpAllocTex &ssao_tex = builder.GetReadTexture(ssao_tex_);
    RpAllocTex &brdf_lut = builder.GetReadTexture(brdf_lut_);
    RpAllocTex &noise_tex = builder.GetReadTexture(noise_tex_);
    RpAllocTex &cone_rt_lut = builder.GetReadTexture(cone_rt_lut_);

    RpAllocTex &dummy_black = builder.GetReadTexture(dummy_black_);
    RpAllocTex &dummy_white = builder.GetReadTexture(dummy_white_);

    RpAllocTex *lm_tex[4];
    for (int i = 0; i < 4; ++i) {
        if (lm_tex_[i]) {
            lm_tex[i] = &builder.GetReadTexture(lm_tex_[i]);
        } else {
            lm_tex[i] = &dummy_black;
        }
    }

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, REN_MATERIALS_SLOT, GLuint(materials_buf.ref->id()));
    if (ctx.capabilities.bindless_texture) {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, REN_BINDLESS_TEX_SLOT, GLuint(textures_buf.ref->id()));
    }

    glBindBufferBase(GL_UNIFORM_BUFFER, REN_UB_SHARED_DATA_LOC, unif_shared_data_buf.ref->id());

    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_SHAD_TEX_SLOT, shad_tex.ref->id());

    if ((*p_list_)->decals_atlas) {
        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_DECAL_TEX_SLOT, (*p_list_)->decals_atlas->tex_id(0));
    }

    if (((*p_list_)->render_flags & (EnableZFill | EnableSSAO)) == (EnableZFill | EnableSSAO)) {
        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_SSAO_TEX_SLOT, ssao_tex.ref->id());
    } else {
        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_SSAO_TEX_SLOT, dummy_white.ref->id());
    }

    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_BRDF_TEX_SLOT, brdf_lut.ref->id());

    for (int sh_l = 0; sh_l < 4; sh_l++) {
        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_LMAP_SH_SLOT + sh_l, lm_tex[sh_l]->ref->id());
    }

    ren_glBindTextureUnit_Comp(GL_TEXTURE_CUBE_MAP_ARRAY, REN_ENV_TEX_SLOT,
                               (*p_list_)->probe_storage ? (*p_list_)->probe_storage->handle().id : 0);

    ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, REN_LIGHT_BUF_SLOT, GLuint(lights_buf.tbos[0]->id()));
    ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, REN_DECAL_BUF_SLOT, GLuint(decals_buf.tbos[0]->id()));
    ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, REN_CELLS_BUF_SLOT, GLuint(cells_buf.tbos[0]->id()));
    ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, REN_ITEMS_BUF_SLOT, GLuint(items_buf.tbos[0]->id()));

    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_NOISE_TEX_SLOT, noise_tex.ref->id());
    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_CONE_RT_LUT_SLOT, cone_rt_lut.ref->id());

    ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, REN_INST_BUF_SLOT, GLuint(instances_buf.tbos[0]->id()));
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, REN_INST_INDICES_BUF_SLOT, GLuint(instance_indices_buf.ref->id()));

    const Ren::Span<const CustomDrawBatch> batches = {(*p_list_)->custom_batches.data,
                                                      (*p_list_)->custom_batches.count};
    const Ren::Span<const uint32_t> batch_indices = {(*p_list_)->custom_batch_indices.data,
                                                     (*p_list_)->custom_batch_indices.count};
    const auto &materials = *(*p_list_)->materials;

    BackendInfo _dummy = {};

    { // actual drawing
        using CDB = CustomDrawBatch;

        uint64_t cur_pipe_id = 0xffffffffffffffff;
        uint64_t cur_prog_id = 0xffffffffffffffff;
        uint64_t cur_mat_id = 0xffffffffffffffff;

        uint32_t i = 0;

        { // one-sided1
            Ren::DebugMarker _m(ctx.current_cmd_buf(), "ONE-SIDED-1");

            rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            i = _draw_list_range_full(builder, materials, pipelines_, batches, batch_indices, i, 0ull, cur_mat_id,
                                      cur_pipe_id, cur_prog_id, _dummy);
        }

        { // two-sided1
            Ren::DebugMarker _m(ctx.current_cmd_buf(), "TWO-SIDED-1");

            rast_state.poly.cull = uint8_t(Ren::eCullFace::None);
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            i = _draw_list_range_full(builder, materials, pipelines_, batches, batch_indices, i, CDB::BitTwoSided,
                                      cur_mat_id, cur_pipe_id, cur_prog_id, _dummy);
        }

        { // one-sided2
            Ren::DebugMarker _m(ctx.current_cmd_buf(), "ONE-SIDED-2");

            rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            i = _draw_list_range_full(builder, materials, pipelines_, batches, batch_indices, i, CDB::BitAlphaTest,
                                      cur_mat_id, cur_pipe_id, cur_prog_id, _dummy);
        }

        { // two-sided2
            Ren::DebugMarker _m(ctx.current_cmd_buf(), "TWO-SIDED-2");

            rast_state.poly.cull = uint8_t(Ren::eCullFace::None);
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            i = _draw_list_range_full(builder, materials, pipelines_, batches, batch_indices, i,
                                      CDB::BitAlphaTest | CDB::BitTwoSided, cur_mat_id, cur_pipe_id, cur_prog_id,
                                      _dummy);
        }

        { // two-sided-tested-blended
            Ren::DebugMarker _m(ctx.current_cmd_buf(), "TWO-SIDED-TESTED-BLENDED");

            rast_state.poly.cull = uint8_t(Ren::eCullFace::None);
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            i = _draw_list_range_full_rev(builder, materials, pipelines_, batches, batch_indices,
                                          uint32_t(batch_indices.size() - 1),
                                          CDB::BitAlphaBlend | CDB::BitAlphaTest | CDB::BitTwoSided, cur_mat_id,
                                          cur_pipe_id, cur_prog_id, _dummy);
        }

        { // one-sided-tested-blended
            Ren::DebugMarker _m(ctx.current_cmd_buf(), "ONE-SIDED-TESTED-BLENDED");

            rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            _draw_list_range_full_rev(builder, materials, pipelines_, batches, batch_indices, i,
                                      CDB::BitAlphaBlend | CDB::BitAlphaTest, cur_mat_id, cur_pipe_id, cur_prog_id,
                                      _dummy);
        }
    }

    Ren::GLUnbindSamplers(REN_MAT_TEX0_SLOT, 8);
}

RpOpaque::~RpOpaque() = default;