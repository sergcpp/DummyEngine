#include "ExOITDepthPeel.h"

#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

#include <Ren/Context.h>
#include <Ren/DebugMarker.h>
#include <Ren/GL.h>
#include <Ren/RastState.h>

#include "../shaders/depth_peel_interface.h"

namespace ExSharedInternal {
void _bind_textures_and_samplers(Ren::Context &ctx, const Ren::Material &mat,
                                 Ren::SmallVectorImpl<Ren::SamplerRef> &temp_samplers);
uint32_t _draw_list_range_full(Eng::FgBuilder &builder, const Ren::MaterialStorage *materials,
                               const Ren::Pipeline pipelines[], Ren::Span<const Eng::custom_draw_batch_t> main_batches,
                               Ren::Span<const uint32_t> main_batch_indices, uint32_t i, uint64_t mask,
                               uint64_t &cur_mat_id, uint64_t &cur_pipe_id, uint64_t &cur_prog_id,
                               Eng::backend_info_t &backend_info);
uint32_t _draw_list_range_full_rev(Eng::FgBuilder &builder, const Ren::MaterialStorage *materials,
                                   const Ren::Pipeline pipelines[],
                                   Ren::Span<const Eng::custom_draw_batch_t> main_batches,
                                   Ren::Span<const uint32_t> main_batch_indices, uint32_t ndx, uint64_t mask,
                                   uint64_t &cur_mat_id, uint64_t &cur_pipe_id, uint64_t &cur_prog_id,
                                   Eng::backend_info_t &backend_info);
uint32_t _draw_range_ext2(Eng::FgBuilder &builder, const Ren::MaterialStorage &materials, const Ren::Texture &white_tex,
                          Ren::Span<const uint32_t> batch_indices, Ren::Span<const Eng::basic_draw_batch_t> batches,
                          uint32_t i, uint64_t mask, uint32_t &cur_mat_id, int *draws_count);
} // namespace ExSharedInternal

void Eng::ExOITDepthPeel::DrawTransparent(FgBuilder &builder) {
    using namespace ExSharedInternal;

    FgAllocBuf &instances_buf = builder.GetReadBuffer(instances_buf_);
    FgAllocBuf &instance_indices_buf = builder.GetReadBuffer(instance_indices_buf_);
    FgAllocBuf &unif_shared_data_buf = builder.GetReadBuffer(shared_data_buf_);
    FgAllocBuf &materials_buf = builder.GetReadBuffer(materials_buf_);
    FgAllocBuf &textures_buf = builder.GetReadBuffer(textures_buf_);
    FgAllocTex &dummy_white = builder.GetReadTexture(dummy_white_);

    FgAllocBuf &out_depth_buf = builder.GetWriteBuffer(out_depth_buf_);

    if (!out_depth_buf.tbos[0]) {
        out_depth_buf.tbos[0] = builder.ctx().CreateTextureBuffer("Depth Values Tex", out_depth_buf.ref,
                                                                  Ren::eTexFormat::R32UI, 0, out_depth_buf.ref->size());
    }

    if ((*p_list_)->alpha_blend_start_index == -1) {
        return;
    }

    Ren::Context &ctx = builder.ctx();

    Ren::RastState rast_state;
    rast_state.poly.cull = uint8_t(Ren::eCullFace::Front);

    if ((*p_list_)->render_settings.debug_wireframe) {
        rast_state.poly.mode = uint8_t(Ren::ePolygonMode::Line);
    } else {
        rast_state.poly.mode = uint8_t(Ren::ePolygonMode::Fill);
    }

    rast_state.depth.test_enabled = true;
    rast_state.depth.write_enabled = false;
    rast_state.depth.compare_op = unsigned(Ren::eCompareOp::Greater);

    // Bind main buffer for drawing
    glBindFramebuffer(GL_FRAMEBUFFER, main_draw_fb_[0][fb_to_use_].id());

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BIND_MATERIALS_BUF, GLuint(materials_buf.ref->id()));
    if (ctx.capabilities.bindless_texture) {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BIND_BINDLESS_TEX, GLuint(textures_buf.ref->id()));
    }

    glBindBufferBase(GL_UNIFORM_BUFFER, BIND_UB_SHARED_DATA_BUF, unif_shared_data_buf.ref->id());

    if ((*p_list_)->decals_atlas) {
        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, BIND_DECAL_TEX, (*p_list_)->decals_atlas->tex_id(0));
    }

    ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, BIND_INST_BUF, GLuint(instances_buf.tbos[0]->id()));
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BIND_INST_NDX_BUF, GLuint(instance_indices_buf.ref->id()));

    glBindImageTexture(GLuint(DepthPeel::OUT_IMG_BUF_SLOT), GLuint(out_depth_buf.tbos[0]->id()), 0, GL_FALSE, 0,
                       GL_READ_WRITE, GL_R32UI);

    const Ren::Span<const basic_draw_batch_t> batches = {(*p_list_)->basic_batches};
    const Ren::Span<const uint32_t> batch_indices = {(*p_list_)->basic_batch_indices};
    const auto &materials = *(*p_list_)->materials;

    int draws_count = 0;
    uint32_t i = (*p_list_)->alpha_blend_start_index;
    uint32_t cur_mat_id = 0xffffffff;

    using BDB = basic_draw_batch_t;

    { // Simple meshes
        Ren::DebugMarker _m(ctx.api_ctx(), ctx.current_cmd_buf(), "SIMPLE");

        glBindVertexArray(pi_simple_[0]->vtx_input()->GetVAO());
        glUseProgram(pi_simple_[0]->prog()->id());

        { // solid one-sided
            Ren::DebugMarker _mm(ctx.api_ctx(), ctx.current_cmd_buf(), "SOLID-ONE-SIDED");

            Ren::RastState rast_state = pi_simple_[0]->rast_state();
            rast_state.viewport[2] = view_state_->act_res[0];
            rast_state.viewport[3] = view_state_->act_res[1];
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            i = _draw_range_ext2(builder, materials, *dummy_white.ref, batch_indices, batches, i, BDB::BitAlphaBlend,
                                 cur_mat_id, &draws_count);

            rast_state = pi_simple_[1]->rast_state();
            rast_state.viewport[2] = view_state_->act_res[0];
            rast_state.viewport[3] = view_state_->act_res[1];
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            i = _draw_range_ext2(builder, materials, *dummy_white.ref, batch_indices, batches, i,
                                 BDB::BitAlphaBlend | BDB::BitBackSided, cur_mat_id, &draws_count);
        }
        { // solid two-sided
            Ren::DebugMarker _mm(ctx.api_ctx(), ctx.current_cmd_buf(), "SOLID-TWO-SIDED");

            Ren::RastState rast_state = pi_simple_[2]->rast_state();
            rast_state.viewport[2] = view_state_->act_res[0];
            rast_state.viewport[3] = view_state_->act_res[1];
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            i = _draw_range_ext2(builder, materials, *dummy_white.ref, batch_indices, batches, i,
                                 BDB::BitAlphaBlend | BDB::BitTwoSided, cur_mat_id, &draws_count);
        }
        { // moving solid one-sided
            Ren::DebugMarker _mm(ctx.api_ctx(), ctx.current_cmd_buf(), "MOVING-SOLID-ONE-SIDED");

            Ren::RastState rast_state = pi_simple_[0]->rast_state();
            rast_state.viewport[2] = view_state_->act_res[0];
            rast_state.viewport[3] = view_state_->act_res[1];
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            i = _draw_range_ext2(builder, materials, *dummy_white.ref, batch_indices, batches, i,
                                 BDB::BitAlphaBlend | BDB::BitMoving, cur_mat_id, &draws_count);
        }
        { // moving solid two-sided
            Ren::DebugMarker _mm(ctx.api_ctx(), ctx.current_cmd_buf(), "MOVING-SOLID-TWO-SIDED");

            Ren::RastState rast_state = pi_simple_[2]->rast_state();
            rast_state.viewport[2] = view_state_->act_res[0];
            rast_state.viewport[3] = view_state_->act_res[1];
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            const uint64_t DrawMask = BDB::BitAlphaBlend | BDB::BitMoving | BDB::BitTwoSided;
            i = _draw_range_ext2(builder, materials, *dummy_white.ref, batch_indices, batches, i, DrawMask, cur_mat_id,
                                 &draws_count);
        }
        { // alpha-tested one-sided
            Ren::DebugMarker _mm(ctx.api_ctx(), ctx.current_cmd_buf(), "ALPHA-ONE-SIDED");

            Ren::RastState rast_state = pi_simple_[0]->rast_state();
            rast_state.viewport[2] = view_state_->act_res[0];
            rast_state.viewport[3] = view_state_->act_res[1];
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            i = _draw_range_ext2(builder, materials, *dummy_white.ref, batch_indices, batches, i,
                                 BDB::BitAlphaBlend | BDB::BitAlphaTest, cur_mat_id, &draws_count);
        }
        { // alpha-tested two-sided
            Ren::DebugMarker _mm(ctx.api_ctx(), ctx.current_cmd_buf(), "ALPHA-TWO-SIDED");

            Ren::RastState rast_state = pi_simple_[2]->rast_state();
            rast_state.viewport[2] = view_state_->act_res[0];
            rast_state.viewport[3] = view_state_->act_res[1];
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            const uint64_t DrawMask = BDB::BitAlphaBlend | BDB::BitAlphaTest | BDB::BitTwoSided;
            i = _draw_range_ext2(builder, materials, *dummy_white.ref, batch_indices, batches, i, DrawMask, cur_mat_id,
                                 &draws_count);
        }
        { // moving alpha-tested one-sided
            Ren::DebugMarker _mm(ctx.api_ctx(), ctx.current_cmd_buf(), "MOVING-ALPHA-ONE-SIDED");

            Ren::RastState rast_state = pi_simple_[0]->rast_state();
            rast_state.viewport[2] = view_state_->act_res[0];
            rast_state.viewport[3] = view_state_->act_res[1];
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            const uint64_t DrawMask = BDB::BitAlphaBlend | BDB::BitMoving | BDB::BitAlphaTest;
            i = _draw_range_ext2(builder, materials, *dummy_white.ref, batch_indices, batches, i, DrawMask, cur_mat_id,
                                 &draws_count);
        }
        { // moving alpha-tested two-sided
            Ren::DebugMarker _mm(ctx.api_ctx(), ctx.current_cmd_buf(), "MOVING-ALPHA-TWO-SIDED");

            Ren::RastState rast_state = pi_simple_[2]->rast_state();
            rast_state.viewport[2] = view_state_->act_res[0];
            rast_state.viewport[3] = view_state_->act_res[1];
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            const uint64_t DrawMask = BDB::BitAlphaBlend | BDB::BitMoving | BDB::BitAlphaTest | BDB::BitTwoSided;
            i = _draw_range_ext2(builder, materials, *dummy_white.ref, batch_indices, batches, i, DrawMask, cur_mat_id,
                                 &draws_count);
        }
    }
}
