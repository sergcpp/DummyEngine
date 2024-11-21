#include "ExGBufferFill.h"

#include "../Renderer_Structs.h"

#include <Ren/Context.h>
#include <Ren/DebugMarker.h>
#include <Ren/GL.h>
#include <Ren/RastState.h>

namespace ExSharedInternal {
uint32_t _draw_range_ext2(Eng::FgBuilder &builder, const Ren::MaterialStorage &materials,
                          const Ren::Texture2D &white_tex, Ren::Span<const uint32_t> batch_indices,
                          Ren::Span<const Eng::BasicDrawBatch> batches, uint32_t i, uint64_t mask, uint32_t &cur_mat_id,
                          int *draws_count) {
    auto &ctx = builder.ctx();

    for (; i < batch_indices.size(); i++) {
        const auto &batch = batches[batch_indices[i]];
        if ((batch.sort_key & Eng::BasicDrawBatch::FlagBits) != mask) {
            break;
        }

        if (!batch.instance_count) {
            continue;
        }

        if (!ctx.capabilities.bindless_texture && batch.material_index != cur_mat_id) {
            const Ren::Material &mat = materials.at(batch.material_index);

            int j = 0;
            for (; j < int(mat.textures.size()); ++j) {
                ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, Eng::BIND_MAT_TEX0 + j, mat.textures[j]->id());
                glBindSampler(Eng::BIND_MAT_TEX0 + j, mat.samplers[j]->id());
            }
            for (; j < Eng::MAX_TEX_PER_MATERIAL; ++j) {
                ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, Eng::BIND_MAT_TEX0 + j, white_tex.id());
                glBindSampler(Eng::BIND_MAT_TEX0 + j, 0);
            }

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

uint32_t _skip_range(Ren::Span<const uint32_t> batch_indices, Ren::Span<const Eng::BasicDrawBatch> batches, uint32_t i,
                     uint32_t mask);
} // namespace ExSharedInternal

void Eng::ExGBufferFill::DrawOpaque(FgBuilder &builder) {
    using namespace ExSharedInternal;

    Ren::Context &ctx = builder.ctx();

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

    // Clear commands account for scissor region
    glDisable(GL_SCISSOR_TEST);
    builder.rast_state().scissor.enabled = false;

    // Bind main buffer for drawing
    glBindFramebuffer(GL_FRAMEBUFFER, main_draw_fb_[0][fb_to_use_].id());
    if (!main_draw_fb_[0][fb_to_use_].color_attachments.empty()) {
        const float black[] = {0.0f, 0.0f, 0.0f, 0.0f};
        glClearTexImage(main_draw_fb_[ctx.backend_frame()][fb_to_use_].color_attachments[0].handle.id, 0, GL_RGBA,
                        GL_FLOAT, black);
        const uint32_t zero[] = {0, 0, 0, 0};
        glClearTexImage(main_draw_fb_[ctx.backend_frame()][fb_to_use_].color_attachments[1].handle.id, 0,
                        GL_RED_INTEGER, GL_UNSIGNED_INT, zero);
        glClearTexImage(main_draw_fb_[ctx.backend_frame()][fb_to_use_].color_attachments[2].handle.id, 0,
                        GL_RED_INTEGER, GL_UNSIGNED_INT, zero);
    }

    rast_state.viewport[2] = view_state_->act_res[0];
    rast_state.viewport[3] = view_state_->act_res[1];

    rast_state.ApplyChanged(builder.rast_state());
    builder.rast_state() = rast_state;

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
    FgAllocBuf &decals_buf = builder.GetReadBuffer(decals_buf_);

    FgAllocTex &noise_tex = builder.GetReadTexture(noise_tex_);
    FgAllocTex &dummy_white = builder.GetReadTexture(dummy_white_);
    FgAllocTex &dummy_black = builder.GetReadTexture(dummy_black_);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BIND_MATERIALS_BUF, GLuint(materials_buf.ref->id()));
    if (ctx.capabilities.bindless_texture) {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BIND_BINDLESS_TEX, GLuint(textures_buf.ref->id()));
    }

    glBindBufferBase(GL_UNIFORM_BUFFER, BIND_UB_SHARED_DATA_BUF, unif_shared_data_buf.ref->id());

    if ((*p_list_)->decals_atlas) {
        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, BIND_DECAL_TEX, (*p_list_)->decals_atlas->tex_id(0));
    }

    ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, BIND_DECAL_BUF, GLuint(decals_buf.tbos[0]->id()));
    ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, BIND_CELLS_BUF, GLuint(cells_buf.tbos[0]->id()));
    ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, BIND_ITEMS_BUF, GLuint(items_buf.tbos[0]->id()));

    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, BIND_NOISE_TEX, noise_tex.ref->id());

    ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, BIND_INST_BUF, GLuint(instances_buf.tbos[0]->id()));
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BIND_INST_NDX_BUF, GLuint(instance_indices_buf.ref->id()));

    const Ren::Span<const BasicDrawBatch> batches = (*p_list_)->basic_batches;
    const Ren::Span<const uint32_t> batch_indices = (*p_list_)->basic_batch_indices;
    const auto &materials = *(*p_list_)->materials;

    int draws_count = 0;
    uint32_t i = 0;
    uint32_t cur_mat_id = 0xffffffff;

    using BDB = BasicDrawBatch;

    { // Simple meshes
        Ren::DebugMarker _m(ctx.api_ctx(), ctx.current_cmd_buf(), "SIMPLE");

        glBindVertexArray(vi_simple_.gl_vao());
        glUseProgram(pi_simple_[0].prog()->id());

        { // solid one-sided
            Ren::DebugMarker _mm(ctx.api_ctx(), ctx.current_cmd_buf(), "SOLID-ONE-SIDED");

            Ren::RastState rast_state = pi_simple_[0].rast_state();
            rast_state.viewport[2] = view_state_->act_res[0];
            rast_state.viewport[3] = view_state_->act_res[1];
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            i = _draw_range_ext2(builder, materials, *dummy_white.ref, batch_indices, batches, i, 0, cur_mat_id,
                                 &draws_count);

            rast_state = pi_simple_[1].rast_state();
            rast_state.viewport[2] = view_state_->act_res[0];
            rast_state.viewport[3] = view_state_->act_res[1];
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            i = _draw_range_ext2(builder, materials, *dummy_white.ref, batch_indices, batches, i, BDB::BitBackSided,
                                 cur_mat_id, &draws_count);
        }

        { // solid two-sided
            Ren::DebugMarker _mm(ctx.api_ctx(), ctx.current_cmd_buf(), "SOLID-TWO-SIDED");

            Ren::RastState rast_state = pi_simple_[2].rast_state();
            rast_state.viewport[2] = view_state_->act_res[0];
            rast_state.viewport[3] = view_state_->act_res[1];
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            i = _draw_range_ext2(builder, materials, *dummy_white.ref, batch_indices, batches, i, BDB::BitTwoSided,
                                 cur_mat_id, &draws_count);
        }

        { // moving solid one-sided
            Ren::DebugMarker _mm(ctx.api_ctx(), ctx.current_cmd_buf(), "MOVING-SOLID-ONE-SIDED");

            Ren::RastState rast_state = pi_simple_[0].rast_state();
            rast_state.viewport[2] = view_state_->act_res[0];
            rast_state.viewport[3] = view_state_->act_res[1];
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            i = _draw_range_ext2(builder, materials, *dummy_white.ref, batch_indices, batches, i, BDB::BitMoving,
                                 cur_mat_id, &draws_count);
        }

        { // moving solid two-sided
            Ren::DebugMarker _mm(ctx.api_ctx(), ctx.current_cmd_buf(), "MOVING-SOLID-TWO-SIDED");

            Ren::RastState rast_state = pi_simple_[2].rast_state();
            rast_state.viewport[2] = view_state_->act_res[0];
            rast_state.viewport[3] = view_state_->act_res[1];
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            const uint64_t DrawMask = BDB::BitMoving | BDB::BitTwoSided;
            i = _draw_range_ext2(builder, materials, *dummy_white.ref, batch_indices, batches, i, DrawMask, cur_mat_id,
                                 &draws_count);
        }

        { // alpha-tested one-sided
            Ren::DebugMarker _mm(ctx.api_ctx(), ctx.current_cmd_buf(), "ALPHA-ONE-SIDED");

            Ren::RastState rast_state = pi_simple_[0].rast_state();
            rast_state.viewport[2] = view_state_->act_res[0];
            rast_state.viewport[3] = view_state_->act_res[1];
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            i = _draw_range_ext2(builder, materials, *dummy_white.ref, batch_indices, batches, i, BDB::BitAlphaTest,
                                 cur_mat_id, &draws_count);
        }

        { // alpha-tested two-sided
            Ren::DebugMarker _mm(ctx.api_ctx(), ctx.current_cmd_buf(), "ALPHA-TWO-SIDED");

            Ren::RastState rast_state = pi_simple_[2].rast_state();
            rast_state.viewport[2] = view_state_->act_res[0];
            rast_state.viewport[3] = view_state_->act_res[1];
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            const uint64_t DrawMask = BDB::BitAlphaTest | BDB::BitTwoSided;
            i = _draw_range_ext2(builder, materials, *dummy_white.ref, batch_indices, batches, i, DrawMask, cur_mat_id,
                                 &draws_count);
        }

        { // moving alpha-tested one-sided
            Ren::DebugMarker _mm(ctx.api_ctx(), ctx.current_cmd_buf(), "MOVING-ALPHA-ONE-SIDED");

            Ren::RastState rast_state = pi_simple_[0].rast_state();
            rast_state.viewport[2] = view_state_->act_res[0];
            rast_state.viewport[3] = view_state_->act_res[1];
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            const uint64_t DrawMask = BDB::BitMoving | BDB::BitAlphaTest;
            i = _draw_range_ext2(builder, materials, *dummy_white.ref, batch_indices, batches, i, DrawMask, cur_mat_id,
                                 &draws_count);
        }

        { // moving alpha-tested two-sided
            Ren::DebugMarker _mm(ctx.api_ctx(), ctx.current_cmd_buf(), "MOVING-ALPHA-TWO-SIDED");

            Ren::RastState rast_state = pi_simple_[2].rast_state();
            rast_state.viewport[2] = view_state_->act_res[0];
            rast_state.viewport[3] = view_state_->act_res[1];
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            const uint64_t DrawMask = BDB::BitMoving | BDB::BitAlphaTest | BDB::BitTwoSided;
            i = _draw_range_ext2(builder, materials, *dummy_white.ref, batch_indices, batches, i, DrawMask, cur_mat_id,
                                 &draws_count);
        }
    }

    { // Vegetation meshes
        Ren::DebugMarker _m(ctx.api_ctx(), ctx.current_cmd_buf(), "VEGETATION");

        glBindVertexArray(vi_vegetation_.gl_vao());
        glUseProgram(pi_vegetation_[0].prog()->id());

        { // vegetation solid one-sided
            Ren::DebugMarker _mm(ctx.api_ctx(), ctx.current_cmd_buf(), "VEGE-SOLID-ONE-SIDED");

            Ren::RastState rast_state = pi_vegetation_[0].rast_state();
            rast_state.viewport[2] = view_state_->act_res[0];
            rast_state.viewport[3] = view_state_->act_res[1];
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            i = _draw_range_ext2(builder, materials, *dummy_white.ref, batch_indices, batches, i, BDB::BitsVege,
                                 cur_mat_id, &draws_count);
        }

        { // vegetation solid two-sided
            Ren::DebugMarker _mm(ctx.api_ctx(), ctx.current_cmd_buf(), "SOLID-TWO-SIDED");

            Ren::RastState rast_state = pi_vegetation_[1].rast_state();
            rast_state.viewport[2] = view_state_->act_res[0];
            rast_state.viewport[3] = view_state_->act_res[1];
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            const uint64_t DrawMask = BDB::BitsVege | BDB::BitTwoSided;
            i = _draw_range_ext2(builder, materials, *dummy_white.ref, batch_indices, batches, i, DrawMask, cur_mat_id,
                                 &draws_count);
        }

        { // vegetation moving solid one-sided
            Ren::DebugMarker _mm(ctx.api_ctx(), ctx.current_cmd_buf(), "VEGE-MOVING-SOLID-ONE-SIDED");

            Ren::RastState rast_state = pi_vegetation_[0].rast_state();
            rast_state.viewport[2] = view_state_->act_res[0];
            rast_state.viewport[3] = view_state_->act_res[1];
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            const uint64_t DrawMask = BDB::BitsVege | BDB::BitMoving;
            i = _draw_range_ext2(builder, materials, *dummy_white.ref, batch_indices, batches, i, DrawMask, cur_mat_id,
                                 &draws_count);
        }

        { // vegetation moving solid two-sided
            Ren::DebugMarker _mm(ctx.api_ctx(), ctx.current_cmd_buf(), "VEGE-MOVING-SOLID-TWO-SIDED");

            Ren::RastState rast_state = pi_vegetation_[1].rast_state();
            rast_state.viewport[2] = view_state_->act_res[0];
            rast_state.viewport[3] = view_state_->act_res[1];
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            const uint64_t DrawMask = BDB::BitsVege | BDB::BitMoving | BDB::BitTwoSided;
            i = _draw_range_ext2(builder, materials, *dummy_white.ref, batch_indices, batches, i, DrawMask, cur_mat_id,
                                 &draws_count);
        }

        { // vegetation alpha-tested one-sided
            Ren::DebugMarker _mm(ctx.api_ctx(), ctx.current_cmd_buf(), "VEGE-ALPHA-ONE-SIDED");

            Ren::RastState rast_state = pi_vegetation_[0].rast_state();
            rast_state.viewport[2] = view_state_->act_res[0];
            rast_state.viewport[3] = view_state_->act_res[1];
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            const uint64_t DrawMask = BDB::BitsVege | BDB::BitAlphaTest;
            i = _draw_range_ext2(builder, materials, *dummy_white.ref, batch_indices, batches, i, DrawMask, cur_mat_id,
                                 &draws_count);
        }

        { // vegetation alpha-tested two-sided
            Ren::DebugMarker _mm(ctx.api_ctx(), ctx.current_cmd_buf(), "VEGE-ALPHA-TWO-SIDED");

            Ren::RastState rast_state = pi_vegetation_[1].rast_state();
            rast_state.viewport[2] = view_state_->act_res[0];
            rast_state.viewport[3] = view_state_->act_res[1];
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            const uint64_t DrawMask = BDB::BitsVege | BDB::BitAlphaTest | BDB::BitTwoSided;
            i = _draw_range_ext2(builder, materials, *dummy_white.ref, batch_indices, batches, i, DrawMask, cur_mat_id,
                                 &draws_count);
        }

        { // vegetation moving alpha-tested one-sided
            Ren::DebugMarker _mm(ctx.api_ctx(), ctx.current_cmd_buf(), "VEGE-MOVING-ALPHA-ONE-SIDED");

            Ren::RastState rast_state = pi_vegetation_[0].rast_state();
            rast_state.viewport[2] = view_state_->act_res[0];
            rast_state.viewport[3] = view_state_->act_res[1];
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            const uint64_t DrawMask = BDB::BitsVege | BDB::BitMoving | BDB::BitAlphaTest;
            i = _draw_range_ext2(builder, materials, *dummy_white.ref, batch_indices, batches, i, DrawMask, cur_mat_id,
                                 &draws_count);
        }

        { // vegetation moving alpha-tested two-sided
            Ren::DebugMarker _mm(ctx.api_ctx(), ctx.current_cmd_buf(), "VEGE-MOVING-ALPHA-TWO-SIDED");

            Ren::RastState rast_state = pi_vegetation_[1].rast_state();
            rast_state.viewport[2] = view_state_->act_res[0];
            rast_state.viewport[3] = view_state_->act_res[1];
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            const uint64_t DrawMask = BDB::BitsVege | BDB::BitMoving | BDB::BitAlphaTest | BDB::BitTwoSided;
            i = _draw_range_ext2(builder, materials, *dummy_white.ref, batch_indices, batches, i, DrawMask, cur_mat_id,
                                 &draws_count);
        }
    }

    { // Skinned meshes
        Ren::DebugMarker _m(ctx.api_ctx(), ctx.current_cmd_buf(), "SKINNED");

        glBindVertexArray(vi_simple_.gl_vao());
        glUseProgram(pi_simple_[0].prog()->id());

        { // skinned solid one-sided
            Ren::DebugMarker _mm(ctx.api_ctx(), ctx.current_cmd_buf(), "SKIN-SOLID-ONE-SIDED");

            Ren::RastState rast_state = pi_simple_[0].rast_state();
            rast_state.viewport[2] = view_state_->act_res[0];
            rast_state.viewport[3] = view_state_->act_res[1];
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            i = _draw_range_ext2(builder, materials, *dummy_white.ref, batch_indices, batches, i, BDB::BitsSkinned,
                                 cur_mat_id, &draws_count);
        }

        { // skinned solid two-sided
            Ren::DebugMarker _mm(ctx.api_ctx(), ctx.current_cmd_buf(), "SKIN-SOLID-TWO-SIDED");

            Ren::RastState rast_state = pi_simple_[2].rast_state();
            rast_state.viewport[2] = view_state_->act_res[0];
            rast_state.viewport[3] = view_state_->act_res[1];
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            const uint64_t DrawMask = BDB::BitsSkinned | BDB::BitTwoSided;
            i = _draw_range_ext2(builder, materials, *dummy_white.ref, batch_indices, batches, i, DrawMask, cur_mat_id,
                                 &draws_count);
        }

        { // skinned moving solid one-sided
            Ren::DebugMarker _mm(ctx.api_ctx(), ctx.current_cmd_buf(), "SKIN-MOVING-SOLID-ONE-SIDED");

            Ren::RastState rast_state = pi_simple_[0].rast_state();
            rast_state.viewport[2] = view_state_->act_res[0];
            rast_state.viewport[3] = view_state_->act_res[1];
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            const uint64_t DrawMask = BDB::BitsSkinned | BDB::BitMoving;
            i = _draw_range_ext2(builder, materials, *dummy_white.ref, batch_indices, batches, i, DrawMask, cur_mat_id,
                                 &draws_count);
        }

        { // skinned moving solid two-sided
            Ren::DebugMarker _mm(ctx.api_ctx(), ctx.current_cmd_buf(), "SKIN-MOVING-SOLID-TWO-SIDED");

            Ren::RastState rast_state = pi_simple_[2].rast_state();
            rast_state.viewport[2] = view_state_->act_res[0];
            rast_state.viewport[3] = view_state_->act_res[1];
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            const uint64_t DrawMask = BDB::BitsSkinned | BDB::BitMoving | BDB::BitTwoSided;
            i = _draw_range_ext2(builder, materials, *dummy_white.ref, batch_indices, batches, i, DrawMask, cur_mat_id,
                                 &draws_count);
        }

        { // skinned alpha-tested one-sided
            Ren::DebugMarker _mm(ctx.api_ctx(), ctx.current_cmd_buf(), "SKIN-ALPHA-ONE-SIDED");

            Ren::RastState rast_state = pi_simple_[0].rast_state();
            rast_state.viewport[2] = view_state_->act_res[0];
            rast_state.viewport[3] = view_state_->act_res[1];
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            const uint64_t DrawMask = BDB::BitsSkinned | BDB::BitAlphaTest;
            i = _draw_range_ext2(builder, materials, *dummy_white.ref, batch_indices, batches, i, DrawMask, cur_mat_id,
                                 &draws_count);
        }

        { // skinned alpha-tested two-sided
            Ren::DebugMarker _mm(ctx.api_ctx(), ctx.current_cmd_buf(), "SKIN-ALPHA-TWO-SIDED");

            Ren::RastState rast_state = pi_simple_[2].rast_state();
            rast_state.viewport[2] = view_state_->act_res[0];
            rast_state.viewport[3] = view_state_->act_res[1];
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            const uint64_t DrawMask = BDB::BitsSkinned | BDB::BitAlphaTest | BDB::BitTwoSided;
            i = _draw_range_ext2(builder, materials, *dummy_white.ref, batch_indices, batches, i, DrawMask, cur_mat_id,
                                 &draws_count);
        }

        { // skinned moving alpha-tested one-sided
            Ren::DebugMarker _mm(ctx.api_ctx(), ctx.current_cmd_buf(), "SKIN-MOVING-ALPHA-ONE-SIDED");

            Ren::RastState rast_state = pi_simple_[0].rast_state();
            rast_state.viewport[2] = view_state_->act_res[0];
            rast_state.viewport[3] = view_state_->act_res[1];
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            const uint64_t DrawMask = BDB::BitsSkinned | BDB::BitMoving | BDB::BitAlphaTest;
            i = _draw_range_ext2(builder, materials, *dummy_white.ref, batch_indices, batches, i, DrawMask, cur_mat_id,
                                 &draws_count);
        }

        { // skinned moving alpha-tested two-sided
            Ren::DebugMarker _mm(ctx.api_ctx(), ctx.current_cmd_buf(), "SKIN-MOVING-ALPHA-TWO-SIDED");

            Ren::RastState rast_state = pi_simple_[2].rast_state();
            rast_state.viewport[2] = view_state_->act_res[0];
            rast_state.viewport[3] = view_state_->act_res[1];
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            const uint64_t DrawMask = BDB::BitsSkinned | BDB::BitMoving | BDB::BitAlphaTest | BDB::BitTwoSided;
            i = _draw_range_ext2(builder, materials, *dummy_white.ref, batch_indices, batches, i, DrawMask, cur_mat_id,
                                 &draws_count);
        }
    }

    Ren::GLUnbindSamplers(BIND_MAT_TEX0, 8);
}
