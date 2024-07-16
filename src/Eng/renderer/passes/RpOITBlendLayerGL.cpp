#include "RpOITBlendLayer.h"

#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

#include <Ren/Context.h>
#include <Ren/DebugMarker.h>
#include <Ren/GL.h>
#include <Ren/RastState.h>

#include "../shaders/blit_oit_depth_interface.h"
#include "../shaders/oit_blend_layer_interface.h"

namespace RpSharedInternal {
void _bind_textures_and_samplers(Ren::Context &ctx, const Ren::Material &mat,
                                 Ren::SmallVectorImpl<Ren::SamplerRef> &temp_samplers);
uint32_t _draw_list_range_full(Eng::RpBuilder &builder, const Ren::MaterialStorage *materials,
                               const Ren::Pipeline pipelines[], Ren::Span<const Eng::CustomDrawBatch> main_batches,
                               Ren::Span<const uint32_t> main_batch_indices, uint32_t i, uint64_t mask,
                               uint64_t &cur_mat_id, uint64_t &cur_pipe_id, uint64_t &cur_prog_id,
                               Eng::BackendInfo &backend_info);

uint32_t _draw_list_range_full_rev(Eng::RpBuilder &builder, const Ren::MaterialStorage *materials,
                                   const Ren::Pipeline pipelines[], Ren::Span<const Eng::CustomDrawBatch> main_batches,
                                   Ren::Span<const uint32_t> main_batch_indices, uint32_t ndx, uint64_t mask,
                                   uint64_t &cur_mat_id, uint64_t &cur_pipe_id, uint64_t &cur_prog_id,
                                   Eng::BackendInfo &backend_info);
uint32_t _draw_range_ext2(Eng::RpBuilder &builder, const Ren::MaterialStorage &materials,
                          const Ren::Texture2D &white_tex, Ren::Span<const uint32_t> batch_indices,
                          Ren::Span<const Eng::BasicDrawBatch> batches, uint32_t i, uint32_t mask, uint32_t &cur_mat_id,
                          int *draws_count);
} // namespace RpSharedInternal

void Eng::RpOITBlendLayer::DrawTransparent(RpBuilder &builder, RpAllocTex &depth_tex) {
    using namespace RpSharedInternal;

    RpAllocTex &noise_tex = builder.GetReadTexture(noise_tex_);
    RpAllocTex &dummy_white = builder.GetReadTexture(dummy_white_);
    RpAllocTex &shadow_map_tex = builder.GetReadTexture(shadow_map_);
    RpAllocTex &ltc_luts_tex = builder.GetReadTexture(ltc_luts_tex_);
    RpAllocTex &env_tex = builder.GetReadTexture(env_tex_);
    RpAllocBuf &instances_buf = builder.GetReadBuffer(instances_buf_);
    RpAllocBuf &instance_indices_buf = builder.GetReadBuffer(instance_indices_buf_);
    RpAllocBuf &unif_shared_data_buf = builder.GetReadBuffer(shared_data_buf_);
    RpAllocBuf &materials_buf = builder.GetReadBuffer(materials_buf_);
    RpAllocBuf &textures_buf = builder.GetReadBuffer(textures_buf_);
    RpAllocBuf &cells_buf = builder.GetReadBuffer(cells_buf_);
    RpAllocBuf &items_buf = builder.GetReadBuffer(items_buf_);
    RpAllocBuf &lights_buf = builder.GetReadBuffer(lights_buf_);
    RpAllocBuf &decals_buf = builder.GetReadBuffer(decals_buf_);
    RpAllocBuf &oit_depth_buf = builder.GetReadBuffer(oit_depth_buf_);

    RpAllocTex *irradiance_tex = nullptr, *distance_tex = nullptr, *offset_tex = nullptr;
    if (irradiance_tex_) {
        irradiance_tex = &builder.GetReadTexture(irradiance_tex_);
        distance_tex = &builder.GetReadTexture(distance_tex_);
        offset_tex = &builder.GetReadTexture(offset_tex_);
    }

    RpAllocTex *specular_tex = nullptr;
    if (oit_specular_tex_) {
        specular_tex = &builder.GetReadTexture(oit_specular_tex_);
    }

    if ((*p_list_)->alpha_blend_start_index == -1) {
        return;
    }

    { // blit depth layer
        Ren::RastState rast_state;
        rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);
        rast_state.viewport[2] = view_state_->act_res[0];
        rast_state.viewport[3] = view_state_->act_res[1];

        rast_state.depth.test_enabled = true;
        rast_state.depth.compare_op = unsigned(Ren::eCompareOp::Greater);

        const Ren::Binding bindings[] = {
            {Ren::eBindTarget::UTBuf, BlitOITDepth::OIT_DEPTH_BUF_SLOT, *oit_depth_buf.tbos[0]}};

        BlitOITDepth::Params uniform_params = {};
        uniform_params.img_size[0] = view_state_->act_res[0];
        uniform_params.img_size[1] = view_state_->act_res[1];
        uniform_params.layer_index = depth_layer_index_;

        const Ren::RenderTarget depth_target = {depth_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store};

        prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, prog_oit_blit_depth_, {}, depth_target, rast_state,
                            builder.rast_state(), bindings, &uniform_params, sizeof(uniform_params), 0);
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

    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, BIND_NOISE_TEX, noise_tex.ref->id());

    ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, BIND_INST_BUF, GLuint(instances_buf.tbos[0]->id()));
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BIND_INST_NDX_BUF, GLuint(instance_indices_buf.ref->id()));

    ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, OITBlendLayer::CELLS_BUF_SLOT, GLuint(cells_buf.tbos[0]->id()));
    ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, OITBlendLayer::ITEMS_BUF_SLOT, GLuint(items_buf.tbos[0]->id()));
    ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, OITBlendLayer::LIGHT_BUF_SLOT, GLuint(lights_buf.tbos[0]->id()));
    ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, OITBlendLayer::DECAL_BUF_SLOT, GLuint(decals_buf.tbos[0]->id()));

    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, OITBlendLayer::SHADOW_TEX_SLOT, shadow_map_tex.ref->id());
    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, OITBlendLayer::LTC_LUTS_TEX_SLOT, ltc_luts_tex.ref->id());
    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, OITBlendLayer::ENV_TEX_SLOT, env_tex.ref->id());

    if (irradiance_tex) {
        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D_ARRAY, OITBlendLayer::IRRADIANCE_TEX_SLOT, irradiance_tex->arr->id());
        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D_ARRAY, OITBlendLayer::DISTANCE_TEX_SLOT, distance_tex->arr->id());
        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D_ARRAY, OITBlendLayer::OFFSET_TEX_SLOT, offset_tex->arr->id());
    }

    if (specular_tex) {
        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, OITBlendLayer::SPECULAR_TEX_SLOT, specular_tex->ref->id());
    }

    const Ren::Span<const BasicDrawBatch> batches = {(*p_list_)->basic_batches};
    const Ren::Span<const uint32_t> batch_indices = {(*p_list_)->basic_batch_indices};
    const auto &materials = *(*p_list_)->materials;

    const int pi_index = specular_tex ? 1 : 0;

    int draws_count = 0;
    uint32_t i = (*p_list_)->alpha_blend_start_index;
    uint32_t cur_mat_id = 0xffffffff;

    using BDB = BasicDrawBatch;

    { // Simple meshes
        Ren::DebugMarker _m(ctx.api_ctx(), ctx.current_cmd_buf(), "SIMPLE");

        glBindVertexArray(vi_simple_.gl_vao());
        glUseProgram(pi_simple_[pi_index][0].prog()->id());

        { // solid one-sided
            Ren::DebugMarker _mm(ctx.api_ctx(), ctx.current_cmd_buf(), "SOLID-ONE-SIDED");

            Ren::RastState rast_state = pi_simple_[pi_index][0].rast_state();
            rast_state.viewport[2] = view_state_->act_res[0];
            rast_state.viewport[3] = view_state_->act_res[1];
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            i = _draw_range_ext2(builder, materials, *dummy_white.ref, batch_indices, batches, i, BDB::BitAlphaBlend,
                                 cur_mat_id, &draws_count);

            rast_state = pi_simple_[pi_index][1].rast_state();
            rast_state.viewport[2] = view_state_->act_res[0];
            rast_state.viewport[3] = view_state_->act_res[1];
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            i = _draw_range_ext2(builder, materials, *dummy_white.ref, batch_indices, batches, i,
                                 BDB::BitAlphaBlend | BDB::BitBackSided, cur_mat_id, &draws_count);
        }

        { // solid two-sided
            Ren::DebugMarker _mm(ctx.api_ctx(), ctx.current_cmd_buf(), "SOLID-TWO-SIDED");

            Ren::RastState rast_state = pi_simple_[pi_index][2].rast_state();
            rast_state.viewport[2] = view_state_->act_res[0];
            rast_state.viewport[3] = view_state_->act_res[1];
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            i = _draw_range_ext2(builder, materials, *dummy_white.ref, batch_indices, batches, i,
                                 BDB::BitAlphaBlend | BDB::BitTwoSided, cur_mat_id, &draws_count);
        }

        { // moving solid one-sided
            Ren::DebugMarker _mm(ctx.api_ctx(), ctx.current_cmd_buf(), "MOVING-SOLID-ONE-SIDED");

            Ren::RastState rast_state = pi_simple_[pi_index][0].rast_state();
            rast_state.viewport[2] = view_state_->act_res[0];
            rast_state.viewport[3] = view_state_->act_res[1];
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            i = _draw_range_ext2(builder, materials, *dummy_white.ref, batch_indices, batches, i,
                                 BDB::BitAlphaBlend | BDB::BitMoving, cur_mat_id, &draws_count);
        }

        { // moving solid two-sided
            Ren::DebugMarker _mm(ctx.api_ctx(), ctx.current_cmd_buf(), "MOVING-SOLID-TWO-SIDED");

            Ren::RastState rast_state = pi_simple_[pi_index][2].rast_state();
            rast_state.viewport[2] = view_state_->act_res[0];
            rast_state.viewport[3] = view_state_->act_res[1];
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            const uint32_t DrawMask = BDB::BitAlphaBlend | BDB::BitMoving | BDB::BitTwoSided;
            i = _draw_range_ext2(builder, materials, *dummy_white.ref, batch_indices, batches, i, DrawMask, cur_mat_id,
                                 &draws_count);
        }

        { // alpha-tested one-sided
            Ren::DebugMarker _mm(ctx.api_ctx(), ctx.current_cmd_buf(), "ALPHA-ONE-SIDED");

            Ren::RastState rast_state = pi_simple_[pi_index][0].rast_state();
            rast_state.viewport[2] = view_state_->act_res[0];
            rast_state.viewport[3] = view_state_->act_res[1];
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            i = _draw_range_ext2(builder, materials, *dummy_white.ref, batch_indices, batches, i,
                                 BDB::BitAlphaBlend | BDB::BitAlphaTest, cur_mat_id, &draws_count);
        }

        { // alpha-tested two-sided
            Ren::DebugMarker _mm(ctx.api_ctx(), ctx.current_cmd_buf(), "ALPHA-TWO-SIDED");

            Ren::RastState rast_state = pi_simple_[pi_index][2].rast_state();
            rast_state.viewport[2] = view_state_->act_res[0];
            rast_state.viewport[3] = view_state_->act_res[1];
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            const uint32_t DrawMask = BDB::BitAlphaBlend | BDB::BitAlphaTest | BDB::BitTwoSided;
            i = _draw_range_ext2(builder, materials, *dummy_white.ref, batch_indices, batches, i, DrawMask, cur_mat_id,
                                 &draws_count);
        }

        { // moving alpha-tested one-sided
            Ren::DebugMarker _mm(ctx.api_ctx(), ctx.current_cmd_buf(), "MOVING-ALPHA-ONE-SIDED");

            Ren::RastState rast_state = pi_simple_[pi_index][0].rast_state();
            rast_state.viewport[2] = view_state_->act_res[0];
            rast_state.viewport[3] = view_state_->act_res[1];
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            const uint32_t DrawMask = BDB::BitAlphaBlend | BDB::BitMoving | BDB::BitAlphaTest;
            i = _draw_range_ext2(builder, materials, *dummy_white.ref, batch_indices, batches, i, DrawMask, cur_mat_id,
                                 &draws_count);
        }

        { // moving alpha-tested two-sided
            Ren::DebugMarker _mm(ctx.api_ctx(), ctx.current_cmd_buf(), "MOVING-ALPHA-TWO-SIDED");

            Ren::RastState rast_state = pi_simple_[pi_index][2].rast_state();
            rast_state.viewport[2] = view_state_->act_res[0];
            rast_state.viewport[3] = view_state_->act_res[1];
            rast_state.ApplyChanged(builder.rast_state());
            builder.rast_state() = rast_state;

            const uint32_t DrawMask = BDB::BitAlphaBlend | BDB::BitMoving | BDB::BitAlphaTest | BDB::BitTwoSided;
            i = _draw_range_ext2(builder, materials, *dummy_white.ref, batch_indices, batches, i, DrawMask, cur_mat_id,
                                 &draws_count);
        }
    }
}
