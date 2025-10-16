#include "ExEmissive.h"

#include "../Renderer_Structs.h"

#include <Ren/Context.h>
#include <Ren/DebugMarker.h>
#include <Ren/GL.h>
#include <Ren/RastState.h>

namespace ExSharedInternal {
uint32_t _draw_range_ext2(Eng::FgContext &fg, const Ren::MaterialStorage &materials, const Ren::Texture &white_tex,
                          Ren::Span<const uint32_t> batch_indices, Ren::Span<const Eng::basic_draw_batch_t> batches,
                          uint32_t i, uint64_t mask, uint32_t &cur_mat_id, int *draws_count);
uint32_t _skip_range(Ren::Span<const uint32_t> batch_indices, Ren::Span<const Eng::basic_draw_batch_t> batches,
                     uint32_t i, uint64_t mask);
} // namespace ExSharedInternal

void Eng::ExEmissive::DrawOpaque(FgContext &fg) {
    using namespace ExSharedInternal;

    Ren::RastState _rast_state;
    _rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

    if ((*p_list_)->render_settings.debug_wireframe) {
        _rast_state.poly.mode = uint8_t(Ren::ePolygonMode::Line);
    } else {
        _rast_state.poly.mode = uint8_t(Ren::ePolygonMode::Fill);
    }

    _rast_state.depth.test_enabled = true;
    if (!(*p_list_)->render_settings.debug_wireframe) {
        _rast_state.depth.compare_op = unsigned(Ren::eCompareOp::Equal);
    } else {
        _rast_state.depth.compare_op = unsigned(Ren::eCompareOp::LEqual);
    }

    // Bind main buffer for drawing
    glBindFramebuffer(GL_FRAMEBUFFER, main_draw_fb_[0][fb_to_use_].id());

    _rast_state.viewport[2] = view_state_->ren_res[0];
    _rast_state.viewport[3] = view_state_->ren_res[1];

    _rast_state.ApplyChanged(fg.rast_state());
    fg.rast_state() = _rast_state;

    //
    // Bind resources (shadow atlas, lightmap, cells item data)
    //
    FgAllocBuf &instances_buf = fg.AccessROBuffer(instances_buf_);
    FgAllocBuf &instance_indices_buf = fg.AccessROBuffer(instance_indices_buf_);
    FgAllocBuf &unif_shared_data_buf = fg.AccessROBuffer(shared_data_buf_);
    FgAllocBuf &materials_buf = fg.AccessROBuffer(materials_buf_);

    FgAllocTex &noise_tex = fg.AccessROTexture(noise_tex_);
    FgAllocTex &dummy_white = fg.AccessROTexture(dummy_white_);

    if ((*p_list_)->emissive_start_index == -1) {
        return;
    }

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BIND_MATERIALS_BUF, GLuint(materials_buf.ref->id()));
    if (fg.ren_ctx().capabilities.bindless_texture) {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BIND_BINDLESS_TEX,
                         GLuint(bindless_tex_->rt_inline_textures.buf->id()));
    }

    glBindBufferBase(GL_UNIFORM_BUFFER, BIND_UB_SHARED_DATA_BUF, unif_shared_data_buf.ref->id());

    if ((*p_list_)->decals_atlas) {
        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, BIND_DECAL_TEX, (*p_list_)->decals_atlas->tex_id(0));
    }

    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, BIND_NOISE_TEX, noise_tex.ref->id());

    ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, BIND_INST_BUF, GLuint(instances_buf.ref->view(0).second));
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BIND_INST_NDX_BUF, GLuint(instance_indices_buf.ref->id()));

    const Ren::Span<const basic_draw_batch_t> batches = (*p_list_)->basic_batches;
    const Ren::Span<const uint32_t> batch_indices = (*p_list_)->basic_batch_indices;
    const auto &materials = *(*p_list_)->materials;

    int draws_count = 0;
    uint32_t i = (*p_list_)->emissive_start_index;
    uint32_t cur_mat_id = 0xffffffff;

    using BDB = basic_draw_batch_t;

    { // Simple meshes
        Ren::DebugMarker _m(fg.ren_ctx().api_ctx(), fg.cmd_buf(), "SIMPLE");

        glBindVertexArray(pi_simple_[0]->vtx_input()->GetVAO());
        glUseProgram(pi_simple_[0]->prog()->id());

        { // solid one-sided
            Ren::DebugMarker _mm(fg.ren_ctx().api_ctx(), fg.cmd_buf(), "SOLID-ONE-SIDED");

            Ren::RastState rast_state = pi_simple_[0]->rast_state();
            rast_state.viewport[2] = view_state_->ren_res[0];
            rast_state.viewport[3] = view_state_->ren_res[1];
            rast_state.ApplyChanged(fg.rast_state());
            fg.rast_state() = rast_state;

            i = _draw_range_ext2(fg, materials, *dummy_white.ref, batch_indices, batches, i, BDB::BitEmissive,
                                 cur_mat_id, &draws_count);

            rast_state = pi_simple_[1]->rast_state();
            rast_state.viewport[2] = view_state_->ren_res[0];
            rast_state.viewport[3] = view_state_->ren_res[1];
            rast_state.ApplyChanged(fg.rast_state());
            fg.rast_state() = rast_state;

            i = _draw_range_ext2(fg, materials, *dummy_white.ref, batch_indices, batches, i,
                                 BDB::BitEmissive | BDB::BitBackSided, cur_mat_id, &draws_count);
        }
        { // solid two-sided
            Ren::DebugMarker _mm(fg.ren_ctx().api_ctx(), fg.cmd_buf(), "SOLID-TWO-SIDED");

            Ren::RastState rast_state = pi_simple_[2]->rast_state();
            rast_state.viewport[2] = view_state_->ren_res[0];
            rast_state.viewport[3] = view_state_->ren_res[1];
            rast_state.ApplyChanged(fg.rast_state());
            fg.rast_state() = rast_state;

            i = _draw_range_ext2(fg, materials, *dummy_white.ref, batch_indices, batches, i,
                                 BDB::BitEmissive | BDB::BitTwoSided, cur_mat_id, &draws_count);
        }
        { // moving solid one-sided
            Ren::DebugMarker _mm(fg.ren_ctx().api_ctx(), fg.cmd_buf(), "MOVING-SOLID-ONE-SIDED");

            Ren::RastState rast_state = pi_simple_[0]->rast_state();
            rast_state.viewport[2] = view_state_->ren_res[0];
            rast_state.viewport[3] = view_state_->ren_res[1];
            rast_state.ApplyChanged(fg.rast_state());
            fg.rast_state() = rast_state;

            i = _draw_range_ext2(fg, materials, *dummy_white.ref, batch_indices, batches, i,
                                 BDB::BitEmissive | BDB::BitMoving, cur_mat_id, &draws_count);
        }
        { // moving solid two-sided
            Ren::DebugMarker _mm(fg.ren_ctx().api_ctx(), fg.cmd_buf(), "MOVING-SOLID-TWO-SIDED");

            Ren::RastState rast_state = pi_simple_[2]->rast_state();
            rast_state.viewport[2] = view_state_->ren_res[0];
            rast_state.viewport[3] = view_state_->ren_res[1];
            rast_state.ApplyChanged(fg.rast_state());
            fg.rast_state() = rast_state;

            const uint64_t DrawMask = BDB::BitEmissive | BDB::BitMoving | BDB::BitTwoSided;
            i = _draw_range_ext2(fg, materials, *dummy_white.ref, batch_indices, batches, i, DrawMask, cur_mat_id,
                                 &draws_count);
        }
        { // alpha-tested one-sided
            Ren::DebugMarker _mm(fg.ren_ctx().api_ctx(), fg.cmd_buf(), "ALPHA-ONE-SIDED");

            Ren::RastState rast_state = pi_simple_[0]->rast_state();
            rast_state.viewport[2] = view_state_->ren_res[0];
            rast_state.viewport[3] = view_state_->ren_res[1];
            rast_state.ApplyChanged(fg.rast_state());
            fg.rast_state() = rast_state;

            i = _draw_range_ext2(fg, materials, *dummy_white.ref, batch_indices, batches, i,
                                 BDB::BitEmissive | BDB::BitAlphaTest, cur_mat_id, &draws_count);
        }
        { // alpha-tested two-sided
            Ren::DebugMarker _mm(fg.ren_ctx().api_ctx(), fg.cmd_buf(), "ALPHA-TWO-SIDED");

            Ren::RastState rast_state = pi_simple_[2]->rast_state();
            rast_state.viewport[2] = view_state_->ren_res[0];
            rast_state.viewport[3] = view_state_->ren_res[1];
            rast_state.ApplyChanged(fg.rast_state());
            fg.rast_state() = rast_state;

            const uint64_t DrawMask = BDB::BitEmissive | BDB::BitAlphaTest | BDB::BitTwoSided;
            i = _draw_range_ext2(fg, materials, *dummy_white.ref, batch_indices, batches, i, DrawMask, cur_mat_id,
                                 &draws_count);
        }
        { // moving alpha-tested one-sided
            Ren::DebugMarker _mm(fg.ren_ctx().api_ctx(), fg.cmd_buf(), "MOVING-ALPHA-ONE-SIDED");

            Ren::RastState rast_state = pi_simple_[0]->rast_state();
            rast_state.viewport[2] = view_state_->ren_res[0];
            rast_state.viewport[3] = view_state_->ren_res[1];
            rast_state.ApplyChanged(fg.rast_state());
            fg.rast_state() = rast_state;

            const uint64_t DrawMask = BDB::BitEmissive | BDB::BitMoving | BDB::BitAlphaTest;
            i = _draw_range_ext2(fg, materials, *dummy_white.ref, batch_indices, batches, i, DrawMask, cur_mat_id,
                                 &draws_count);
        }
        { // moving alpha-tested two-sided
            Ren::DebugMarker _mm(fg.ren_ctx().api_ctx(), fg.cmd_buf(), "MOVING-ALPHA-TWO-SIDED");

            Ren::RastState rast_state = pi_simple_[2]->rast_state();
            rast_state.viewport[2] = view_state_->ren_res[0];
            rast_state.viewport[3] = view_state_->ren_res[1];
            rast_state.ApplyChanged(fg.rast_state());
            fg.rast_state() = rast_state;

            const uint64_t DrawMask = BDB::BitEmissive | BDB::BitMoving | BDB::BitAlphaTest | BDB::BitTwoSided;
            i = _draw_range_ext2(fg, materials, *dummy_white.ref, batch_indices, batches, i, DrawMask, cur_mat_id,
                                 &draws_count);
        }
    }
    { // Vegetation meshes
        Ren::DebugMarker _m(fg.ren_ctx().api_ctx(), fg.cmd_buf(), "VEGETATION");

        glBindVertexArray(pi_vegetation_[0]->vtx_input()->GetVAO());
        glUseProgram(pi_vegetation_[0]->prog()->id());

        { // vegetation solid one-sided
            Ren::DebugMarker _mm(fg.ren_ctx().api_ctx(), fg.cmd_buf(), "VEGE-SOLID-ONE-SIDED");

            Ren::RastState rast_state = pi_vegetation_[0]->rast_state();
            rast_state.viewport[2] = view_state_->ren_res[0];
            rast_state.viewport[3] = view_state_->ren_res[1];
            rast_state.ApplyChanged(fg.rast_state());
            fg.rast_state() = rast_state;

            i = _draw_range_ext2(fg, materials, *dummy_white.ref, batch_indices, batches, i,
                                 BDB::BitEmissive | BDB::BitsVege, cur_mat_id, &draws_count);
        }
        { // vegetation solid two-sided
            Ren::DebugMarker _mm(fg.ren_ctx().api_ctx(), fg.cmd_buf(), "SOLID-TWO-SIDED");

            Ren::RastState rast_state = pi_vegetation_[1]->rast_state();
            rast_state.viewport[2] = view_state_->ren_res[0];
            rast_state.viewport[3] = view_state_->ren_res[1];
            rast_state.ApplyChanged(fg.rast_state());
            fg.rast_state() = rast_state;

            const uint64_t DrawMask = BDB::BitEmissive | BDB::BitsVege | BDB::BitTwoSided;
            i = _draw_range_ext2(fg, materials, *dummy_white.ref, batch_indices, batches, i, DrawMask, cur_mat_id,
                                 &draws_count);
        }
        { // vegetation moving solid one-sided
            Ren::DebugMarker _mm(fg.ren_ctx().api_ctx(), fg.cmd_buf(), "VEGE-MOVING-SOLID-ONE-SIDED");

            Ren::RastState rast_state = pi_vegetation_[0]->rast_state();
            rast_state.viewport[2] = view_state_->ren_res[0];
            rast_state.viewport[3] = view_state_->ren_res[1];
            rast_state.ApplyChanged(fg.rast_state());
            fg.rast_state() = rast_state;

            const uint64_t DrawMask = BDB::BitEmissive | BDB::BitsVege | BDB::BitMoving;
            i = _draw_range_ext2(fg, materials, *dummy_white.ref, batch_indices, batches, i, DrawMask, cur_mat_id,
                                 &draws_count);
        }
        { // vegetation moving solid two-sided
            Ren::DebugMarker _mm(fg.ren_ctx().api_ctx(), fg.cmd_buf(), "VEGE-MOVING-SOLID-TWO-SIDED");

            Ren::RastState rast_state = pi_vegetation_[1]->rast_state();
            rast_state.viewport[2] = view_state_->ren_res[0];
            rast_state.viewport[3] = view_state_->ren_res[1];
            rast_state.ApplyChanged(fg.rast_state());
            fg.rast_state() = rast_state;

            const uint64_t DrawMask = BDB::BitEmissive | BDB::BitsVege | BDB::BitMoving | BDB::BitTwoSided;
            i = _draw_range_ext2(fg, materials, *dummy_white.ref, batch_indices, batches, i, DrawMask, cur_mat_id,
                                 &draws_count);
        }
        { // vegetation alpha-tested one-sided
            Ren::DebugMarker _mm(fg.ren_ctx().api_ctx(), fg.cmd_buf(), "VEGE-ALPHA-ONE-SIDED");

            Ren::RastState rast_state = pi_vegetation_[0]->rast_state();
            rast_state.viewport[2] = view_state_->ren_res[0];
            rast_state.viewport[3] = view_state_->ren_res[1];
            rast_state.ApplyChanged(fg.rast_state());
            fg.rast_state() = rast_state;

            const uint64_t DrawMask = BDB::BitEmissive | BDB::BitsVege | BDB::BitAlphaTest;
            i = _draw_range_ext2(fg, materials, *dummy_white.ref, batch_indices, batches, i, DrawMask, cur_mat_id,
                                 &draws_count);
        }
        { // vegetation alpha-tested two-sided
            Ren::DebugMarker _mm(fg.ren_ctx().api_ctx(), fg.cmd_buf(), "VEGE-ALPHA-TWO-SIDED");

            Ren::RastState rast_state = pi_vegetation_[1]->rast_state();
            rast_state.viewport[2] = view_state_->ren_res[0];
            rast_state.viewport[3] = view_state_->ren_res[1];
            rast_state.ApplyChanged(fg.rast_state());
            fg.rast_state() = rast_state;

            const uint64_t DrawMask = BDB::BitEmissive | BDB::BitsVege | BDB::BitAlphaTest | BDB::BitTwoSided;
            i = _draw_range_ext2(fg, materials, *dummy_white.ref, batch_indices, batches, i, DrawMask, cur_mat_id,
                                 &draws_count);
        }
        { // vegetation moving alpha-tested one-sided
            Ren::DebugMarker _mm(fg.ren_ctx().api_ctx(), fg.cmd_buf(), "VEGE-MOVING-ALPHA-ONE-SIDED");

            Ren::RastState rast_state = pi_vegetation_[0]->rast_state();
            rast_state.viewport[2] = view_state_->ren_res[0];
            rast_state.viewport[3] = view_state_->ren_res[1];
            rast_state.ApplyChanged(fg.rast_state());
            fg.rast_state() = rast_state;

            const uint64_t DrawMask = BDB::BitEmissive | BDB::BitsVege | BDB::BitMoving | BDB::BitAlphaTest;
            i = _draw_range_ext2(fg, materials, *dummy_white.ref, batch_indices, batches, i, DrawMask, cur_mat_id,
                                 &draws_count);
        }
        { // vegetation moving alpha-tested two-sided
            Ren::DebugMarker _mm(fg.ren_ctx().api_ctx(), fg.cmd_buf(), "VEGE-MOVING-ALPHA-TWO-SIDED");

            Ren::RastState rast_state = pi_vegetation_[1]->rast_state();
            rast_state.viewport[2] = view_state_->ren_res[0];
            rast_state.viewport[3] = view_state_->ren_res[1];
            rast_state.ApplyChanged(fg.rast_state());
            fg.rast_state() = rast_state;

            const uint64_t DrawMask =
                BDB::BitEmissive | BDB::BitsVege | BDB::BitMoving | BDB::BitAlphaTest | BDB::BitTwoSided;
            i = _draw_range_ext2(fg, materials, *dummy_white.ref, batch_indices, batches, i, DrawMask, cur_mat_id,
                                 &draws_count);
        }
    }

    { // Skinned meshes
        Ren::DebugMarker _m(fg.ren_ctx().api_ctx(), fg.cmd_buf(), "SKINNED");

        glBindVertexArray(pi_simple_[0]->vtx_input()->GetVAO());
        glUseProgram(pi_simple_[0]->prog()->id());

        { // skinned solid one-sided
            Ren::DebugMarker _mm(fg.ren_ctx().api_ctx(), fg.cmd_buf(), "SKIN-SOLID-ONE-SIDED");

            Ren::RastState rast_state = pi_simple_[0]->rast_state();
            rast_state.viewport[2] = view_state_->ren_res[0];
            rast_state.viewport[3] = view_state_->ren_res[1];
            rast_state.ApplyChanged(fg.rast_state());
            fg.rast_state() = rast_state;

            i = _draw_range_ext2(fg, materials, *dummy_white.ref, batch_indices, batches, i, BDB::BitsSkinned,
                                 cur_mat_id, &draws_count);
        }
        { // skinned solid two-sided
            Ren::DebugMarker _mm(fg.ren_ctx().api_ctx(), fg.cmd_buf(), "SKIN-SOLID-TWO-SIDED");

            Ren::RastState rast_state = pi_simple_[2]->rast_state();
            rast_state.viewport[2] = view_state_->ren_res[0];
            rast_state.viewport[3] = view_state_->ren_res[1];
            rast_state.ApplyChanged(fg.rast_state());
            fg.rast_state() = rast_state;

            const uint64_t DrawMask = BDB::BitsSkinned | BDB::BitTwoSided;
            i = _draw_range_ext2(fg, materials, *dummy_white.ref, batch_indices, batches, i, DrawMask, cur_mat_id,
                                 &draws_count);
        }
        { // skinned moving solid one-sided
            Ren::DebugMarker _mm(fg.ren_ctx().api_ctx(), fg.cmd_buf(), "SKIN-MOVING-SOLID-ONE-SIDED");

            Ren::RastState rast_state = pi_simple_[0]->rast_state();
            rast_state.viewport[2] = view_state_->ren_res[0];
            rast_state.viewport[3] = view_state_->ren_res[1];
            rast_state.ApplyChanged(fg.rast_state());
            fg.rast_state() = rast_state;

            const uint64_t DrawMask = BDB::BitsSkinned | BDB::BitMoving;
            i = _draw_range_ext2(fg, materials, *dummy_white.ref, batch_indices, batches, i, DrawMask, cur_mat_id,
                                 &draws_count);
        }
        { // skinned moving solid two-sided
            Ren::DebugMarker _mm(fg.ren_ctx().api_ctx(), fg.cmd_buf(), "SKIN-MOVING-SOLID-TWO-SIDED");

            Ren::RastState rast_state = pi_simple_[2]->rast_state();
            rast_state.viewport[2] = view_state_->ren_res[0];
            rast_state.viewport[3] = view_state_->ren_res[1];
            rast_state.ApplyChanged(fg.rast_state());
            fg.rast_state() = rast_state;

            const uint64_t DrawMask = BDB::BitsSkinned | BDB::BitMoving | BDB::BitTwoSided;
            i = _draw_range_ext2(fg, materials, *dummy_white.ref, batch_indices, batches, i, DrawMask, cur_mat_id,
                                 &draws_count);
        }
        { // skinned alpha-tested one-sided
            Ren::DebugMarker _mm(fg.ren_ctx().api_ctx(), fg.cmd_buf(), "SKIN-ALPHA-ONE-SIDED");

            Ren::RastState rast_state = pi_simple_[0]->rast_state();
            rast_state.viewport[2] = view_state_->ren_res[0];
            rast_state.viewport[3] = view_state_->ren_res[1];
            rast_state.ApplyChanged(fg.rast_state());
            fg.rast_state() = rast_state;

            const uint64_t DrawMask = BDB::BitsSkinned | BDB::BitAlphaTest;
            i = _draw_range_ext2(fg, materials, *dummy_white.ref, batch_indices, batches, i, DrawMask, cur_mat_id,
                                 &draws_count);
        }
        { // skinned alpha-tested two-sided
            Ren::DebugMarker _mm(fg.ren_ctx().api_ctx(), fg.cmd_buf(), "SKIN-ALPHA-TWO-SIDED");

            Ren::RastState rast_state = pi_simple_[2]->rast_state();
            rast_state.viewport[2] = view_state_->ren_res[0];
            rast_state.viewport[3] = view_state_->ren_res[1];
            rast_state.ApplyChanged(fg.rast_state());
            fg.rast_state() = rast_state;

            const uint64_t DrawMask = BDB::BitsSkinned | BDB::BitAlphaTest | BDB::BitTwoSided;
            i = _draw_range_ext2(fg, materials, *dummy_white.ref, batch_indices, batches, i, DrawMask, cur_mat_id,
                                 &draws_count);
        }
        { // skinned moving alpha-tested one-sided
            Ren::DebugMarker _mm(fg.ren_ctx().api_ctx(), fg.cmd_buf(), "SKIN-MOVING-ALPHA-ONE-SIDED");

            Ren::RastState rast_state = pi_simple_[0]->rast_state();
            rast_state.viewport[2] = view_state_->ren_res[0];
            rast_state.viewport[3] = view_state_->ren_res[1];
            rast_state.ApplyChanged(fg.rast_state());
            fg.rast_state() = rast_state;

            const uint64_t DrawMask = BDB::BitsSkinned | BDB::BitMoving | BDB::BitAlphaTest;
            i = _draw_range_ext2(fg, materials, *dummy_white.ref, batch_indices, batches, i, DrawMask, cur_mat_id,
                                 &draws_count);
        }
        { // skinned moving alpha-tested two-sided
            Ren::DebugMarker _mm(fg.ren_ctx().api_ctx(), fg.cmd_buf(), "SKIN-MOVING-ALPHA-TWO-SIDED");

            Ren::RastState rast_state = pi_simple_[2]->rast_state();
            rast_state.viewport[2] = view_state_->ren_res[0];
            rast_state.viewport[3] = view_state_->ren_res[1];
            rast_state.ApplyChanged(fg.rast_state());
            fg.rast_state() = rast_state;

            const uint64_t DrawMask = BDB::BitsSkinned | BDB::BitMoving | BDB::BitAlphaTest | BDB::BitTwoSided;
            i = _draw_range_ext2(fg, materials, *dummy_white.ref, batch_indices, batches, i, DrawMask, cur_mat_id,
                                 &draws_count);
        }
    }

    Ren::GLUnbindSamplers(BIND_MAT_TEX0, 8);
}
