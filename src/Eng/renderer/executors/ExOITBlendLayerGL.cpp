#include "ExOITBlendLayer.h"

#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

#include <Ren/Context.h>
#include <Ren/DebugMarker.h>
#include <Ren/Gl/GL.h>
#include <Ren/RastState.h>

#include "../Renderer_DrawList.h"
#include "../framegraph/FgBuilder.h"
#include "../shaders/blit_oit_depth_interface.h"
#include "../shaders/oit_blend_layer_interface.h"

namespace ExSharedInternal {
void _bind_textures_and_samplers(const Ren::StoragesRef &storages, const Ren::MaterialMain &mat,
                                 Ren::SmallVectorImpl<Ren::SamplerHandle> &temp_samplers);
uint32_t _draw_range_ext2(const Eng::FgContext &fg, const Ren::ImageMain &white_tex,
                          Ren::Span<const uint32_t> batch_indices, Ren::Span<const Eng::basic_draw_batch_t> batches,
                          uint32_t i, uint64_t mask, uint32_t &cur_mat_id, int *draws_count);
} // namespace ExSharedInternal

void Eng::ExOITBlendLayer::DrawTransparent(const FgContext &fg, const Ren::ImageRWHandle depth_tex,
                                           const Ren::ImageRWHandle color_tex) {
    using namespace ExSharedInternal;

    const Ren::BufferROHandle attrib_bufs[] = {fg.AccessROBuffer(vtx_buf1_), fg.AccessROBuffer(vtx_buf2_)};
    const Ren::BufferROHandle ndx_buf = fg.AccessROBuffer(ndx_buf_);

    const Ren::ImageROHandle noise = fg.AccessROImage(noise_);
    const Ren::ImageROHandle dummy_white = fg.AccessROImage(dummy_white_);
    const Ren::ImageROHandle shadow_depth = fg.AccessROImage(shadow_depth_);
    const Ren::ImageROHandle ltc_luts_tex = fg.AccessROImage(ltc_luts_);
    const Ren::ImageROHandle env = fg.AccessROImage(env_);
    const Ren::BufferROHandle instances = fg.AccessROBuffer(instances_);
    const Ren::BufferROHandle instance_indices = fg.AccessROBuffer(instance_indices_);
    const Ren::BufferROHandle unif_shared_data = fg.AccessROBuffer(shared_data_);
    const Ren::BufferROHandle materials = fg.AccessROBuffer(materials_);
    const Ren::BufferROHandle cells = fg.AccessROBuffer(cells_);
    const Ren::BufferROHandle items = fg.AccessROBuffer(items_);
    const Ren::BufferROHandle lights = fg.AccessROBuffer(lights_);
    const Ren::BufferROHandle decals = fg.AccessROBuffer(decals_);
    const Ren::BufferROHandle oit_depth = fg.AccessROBuffer(oit_depth_);
    const Ren::ImageROHandle back_color = fg.AccessROImage(back_color_);
    const Ren::ImageROHandle back_depth = fg.AccessROImage(back_depth_);

    Ren::ImageROHandle irr, dist, off;
    if (irradiance_) {
        irr = fg.AccessROImage(irradiance_);
        dist = fg.AccessROImage(distance_);
        off = fg.AccessROImage(offset_);
    }

    Ren::ImageROHandle specular = {};
    if (oit_specular_) {
        specular = fg.AccessROImage(oit_specular_);
    }

    if ((*p_list_)->alpha_blend_start_index == -1) {
        return;
    }

    { // blit depth layer
        Ren::RastState rast_state;
        rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);
        rast_state.viewport[2] = view_state_->ren_res[0];
        rast_state.viewport[3] = view_state_->ren_res[1];

        rast_state.depth.test_enabled = true;
        rast_state.depth.compare_op = unsigned(Ren::eCompareOp::Greater);

        rast_state.stencil.enabled = true;
        rast_state.stencil.pass = uint8_t(Ren::eStencilOp::Replace);
        rast_state.stencil.compare_mask = rast_state.stencil.write_mask = STENCIL_TRANSPARENT_BIT;
        rast_state.stencil.reference = STENCIL_TRANSPARENT_BIT;

        const Ren::Binding bindings[] = {{Ren::eBindTarget::UTBuf, BlitOITDepth::OIT_DEPTH_BUF_SLOT, oit_depth}};

        BlitOITDepth::Params uniform_params = {};
        uniform_params.img_size = view_state_->ren_res;
        uniform_params.layer_index = depth_layer_index_;

        const Ren::RenderTarget depth_target = {depth_tex, Ren::eLoadOp::Load, Ren::eStoreOp::Store, Ren::eLoadOp::Load,
                                                Ren::eStoreOp::Store};

        prim_draw_.DrawPrim(fg.cmd_buf(), PrimDraw::ePrim::Quad, prog_oit_blit_depth_, depth_target, {}, rast_state,
                            fg.rast_state(), bindings, &uniform_params, sizeof(uniform_params), 0, fg.framebuffers());
    }

    Ren::RastState _rast_state;
    _rast_state.poly.cull = uint8_t(Ren::eCullFace::Front);

    if ((*p_list_)->render_settings.debug_wireframe) {
        _rast_state.poly.mode = uint8_t(Ren::ePolygonMode::Line);
    } else {
        _rast_state.poly.mode = uint8_t(Ren::ePolygonMode::Fill);
    }

    _rast_state.depth.test_enabled = true;
    _rast_state.depth.write_enabled = false;
    _rast_state.depth.compare_op = unsigned(Ren::eCompareOp::Greater);

    const Ren::StoragesRef &storages = fg.storages();

    const Ren::ImageRWHandle color_targets[] = {color_tex};
    const Ren::FramebufferHandle fb = fg.FindOrCreateFramebuffer({}, depth_tex, depth_tex, color_targets);

    // Bind main buffer for drawing
    glBindFramebuffer(GL_FRAMEBUFFER, storages.framebuffers[fb].first.id);

    const Ren::BufferMain &materials_main = storages.buffers[materials].first;
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BIND_MATERIALS_BUF, GLuint(materials_main.buf));
    if (fg.ren_ctx().capabilities.bindless_texture) {
        const Ren::BufferMain &buf_main = storages.buffers[bindless_tex_->rt_inline_textures.buf].first;
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BIND_BINDLESS_TEX, GLuint(buf_main.buf));
    }

    const Ren::BufferMain &unif_shared_data_main = storages.buffers[unif_shared_data].first;
    glBindBufferBase(GL_UNIFORM_BUFFER, BIND_UB_SHARED_DATA_BUF, unif_shared_data_main.buf);

    if ((*p_list_)->decals_atlas) {
        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, BIND_DECAL_TEX, (*p_list_)->decals_atlas->tex_id(0));
    }

    const Ren::ImageMain &noise_main = storages.images[noise].first;
    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, BIND_NOISE_TEX, noise_main.img);

    const Ren::BufferMain &instances_buf_main = storages.buffers[instances].first;
    ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, BIND_INST_BUF, GLuint(instances_buf_main.views[0].second));
    const Ren::BufferMain &instance_indices_buf_main = storages.buffers[instance_indices].first;
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BIND_INST_NDX_BUF, GLuint(instance_indices_buf_main.buf));

    ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, OITBlendLayer::CELLS_BUF_SLOT,
                               storages.buffers[cells].first.views[0].second);
    ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, OITBlendLayer::ITEMS_BUF_SLOT,
                               storages.buffers[items].first.views[0].second);
    ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, OITBlendLayer::LIGHT_BUF_SLOT,
                               storages.buffers[lights].first.views[0].second);
    ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, OITBlendLayer::DECAL_BUF_SLOT,
                               storages.buffers[decals].first.views[0].second);

    const Ren::ImageMain &shadow_depth_main = storages.images[shadow_depth].first;
    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, OITBlendLayer::SHADOW_TEX_SLOT, shadow_depth_main.img);
    const Ren::ImageMain &ltc_luts_main = storages.images[ltc_luts_tex].first;
    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, OITBlendLayer::LTC_LUTS_TEX_SLOT, ltc_luts_main.img);
    const Ren::ImageMain &env_main = storages.images[env].first;
    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, OITBlendLayer::ENV_TEX_SLOT, env_main.img);

    const Ren::ImageMain &back_color_main = storages.images[back_color].first;
    const Ren::ImageMain &back_depth_main = storages.images[back_depth].first;
    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, OITBlendLayer::BACK_COLOR_TEX_SLOT, back_color_main.img);
    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, OITBlendLayer::BACK_DEPTH_TEX_SLOT, back_depth_main.img);

    if (irr) {
        const Ren::ImageMain &irr_main = storages.images[irr].first, &dist_main = storages.images[dist].first,
                             &off_main = storages.images[off].first;
        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D_ARRAY, OITBlendLayer::IRRADIANCE_TEX_SLOT, irr_main.img);
        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D_ARRAY, OITBlendLayer::DISTANCE_TEX_SLOT, dist_main.img);
        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D_ARRAY, OITBlendLayer::OFFSET_TEX_SLOT, off_main.img);
    }

    if (specular) {
        const Ren::ImageMain &specular_main = storages.images[specular].first;
        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, OITBlendLayer::SPEC_TEX_SLOT, specular_main.img);
    }

    const Ren::ImageMain &dummy_white_main = storages.images[dummy_white].first;

    const Ren::Span<const basic_draw_batch_t> batches = {(*p_list_)->basic_batches};
    const Ren::Span<const uint32_t> batch_indices = {(*p_list_)->basic_batch_indices};

    int draws_count = 0;
    uint32_t i = (*p_list_)->alpha_blend_start_index;
    uint32_t cur_mat_id = 0xffffffff;

    using BDB = basic_draw_batch_t;

    const Ren::ApiContext &api = fg.ren_ctx().api();

    { // Simple meshes
        Ren::DebugMarker _m(api, fg.cmd_buf(), "SIMPLE");

        const Ren::PipelineMain &pi_simple0_main = storages.pipelines[pi_simple_[0]].first;
        const Ren::PipelineMain &pi_simple1_main = storages.pipelines[pi_simple_[1]].first;
        const Ren::PipelineMain &pi_simple2_main = storages.pipelines[pi_simple_[2]].first;

        const Ren::VertexInput &vi = storages.vtx_inputs[pi_simple0_main.vtx_input];
        VertexInput_BindBuffers(api, vi, storages.buffers, attrib_bufs, ndx_buf);
        glUseProgram(storages.programs[pi_simple0_main.prog].first.id);

        { // solid one-sided
            Ren::DebugMarker _mm(api, fg.cmd_buf(), "SOLID-ONE-SIDED");

            Ren::RastState rast_state = pi_simple0_main.rast_state;
            rast_state.viewport[2] = view_state_->ren_res[0];
            rast_state.viewport[3] = view_state_->ren_res[1];
            rast_state.ApplyChanged(fg.rast_state());
            fg.rast_state() = rast_state;

            i = _draw_range_ext2(fg, dummy_white_main, batch_indices, batches, i, BDB::BitAlphaBlend, cur_mat_id,
                                 &draws_count);

            rast_state = pi_simple1_main.rast_state;
            rast_state.viewport[2] = view_state_->ren_res[0];
            rast_state.viewport[3] = view_state_->ren_res[1];
            rast_state.ApplyChanged(fg.rast_state());
            fg.rast_state() = rast_state;

            i = _draw_range_ext2(fg, dummy_white_main, batch_indices, batches, i,
                                 BDB::BitAlphaBlend | BDB::BitBackSided, cur_mat_id, &draws_count);
        }
        { // solid two-sided
            Ren::DebugMarker _mm(api, fg.cmd_buf(), "SOLID-TWO-SIDED");

            Ren::RastState rast_state = pi_simple2_main.rast_state;
            rast_state.viewport[2] = view_state_->ren_res[0];
            rast_state.viewport[3] = view_state_->ren_res[1];
            rast_state.ApplyChanged(fg.rast_state());
            fg.rast_state() = rast_state;

            i = _draw_range_ext2(fg, dummy_white_main, batch_indices, batches, i, BDB::BitAlphaBlend | BDB::BitTwoSided,
                                 cur_mat_id, &draws_count);
        }
        { // moving solid one-sided
            Ren::DebugMarker _mm(api, fg.cmd_buf(), "MOVING-SOLID-ONE-SIDED");

            Ren::RastState rast_state = pi_simple0_main.rast_state;
            rast_state.viewport[2] = view_state_->ren_res[0];
            rast_state.viewport[3] = view_state_->ren_res[1];
            rast_state.ApplyChanged(fg.rast_state());
            fg.rast_state() = rast_state;

            i = _draw_range_ext2(fg, dummy_white_main, batch_indices, batches, i, BDB::BitAlphaBlend | BDB::BitMoving,
                                 cur_mat_id, &draws_count);
        }
        { // moving solid two-sided
            Ren::DebugMarker _mm(api, fg.cmd_buf(), "MOVING-SOLID-TWO-SIDED");

            Ren::RastState rast_state = pi_simple2_main.rast_state;
            rast_state.viewport[2] = view_state_->ren_res[0];
            rast_state.viewport[3] = view_state_->ren_res[1];
            rast_state.ApplyChanged(fg.rast_state());
            fg.rast_state() = rast_state;

            const uint64_t DrawMask = BDB::BitAlphaBlend | BDB::BitMoving | BDB::BitTwoSided;
            i = _draw_range_ext2(fg, dummy_white_main, batch_indices, batches, i, DrawMask, cur_mat_id, &draws_count);
        }
        { // alpha-tested one-sided
            Ren::DebugMarker _mm(api, fg.cmd_buf(), "ALPHA-ONE-SIDED");

            Ren::RastState rast_state = pi_simple0_main.rast_state;
            rast_state.viewport[2] = view_state_->ren_res[0];
            rast_state.viewport[3] = view_state_->ren_res[1];
            rast_state.ApplyChanged(fg.rast_state());
            fg.rast_state() = rast_state;

            i = _draw_range_ext2(fg, dummy_white_main, batch_indices, batches, i,
                                 BDB::BitAlphaBlend | BDB::BitAlphaTest, cur_mat_id, &draws_count);
        }
        { // alpha-tested two-sided
            Ren::DebugMarker _mm(api, fg.cmd_buf(), "ALPHA-TWO-SIDED");

            Ren::RastState rast_state = pi_simple2_main.rast_state;
            rast_state.viewport[2] = view_state_->ren_res[0];
            rast_state.viewport[3] = view_state_->ren_res[1];
            rast_state.ApplyChanged(fg.rast_state());
            fg.rast_state() = rast_state;

            const uint64_t DrawMask = BDB::BitAlphaBlend | BDB::BitAlphaTest | BDB::BitTwoSided;
            i = _draw_range_ext2(fg, dummy_white_main, batch_indices, batches, i, DrawMask, cur_mat_id, &draws_count);
        }
        { // moving alpha-tested one-sided
            Ren::DebugMarker _mm(api, fg.cmd_buf(), "MOVING-ALPHA-ONE-SIDED");

            Ren::RastState rast_state = pi_simple0_main.rast_state;
            rast_state.viewport[2] = view_state_->ren_res[0];
            rast_state.viewport[3] = view_state_->ren_res[1];
            rast_state.ApplyChanged(fg.rast_state());
            fg.rast_state() = rast_state;

            const uint64_t DrawMask = BDB::BitAlphaBlend | BDB::BitMoving | BDB::BitAlphaTest;
            i = _draw_range_ext2(fg, dummy_white_main, batch_indices, batches, i, DrawMask, cur_mat_id, &draws_count);
        }
        { // moving alpha-tested two-sided
            Ren::DebugMarker _mm(api, fg.cmd_buf(), "MOVING-ALPHA-TWO-SIDED");

            Ren::RastState rast_state = pi_simple2_main.rast_state;
            rast_state.viewport[2] = view_state_->ren_res[0];
            rast_state.viewport[3] = view_state_->ren_res[1];
            rast_state.ApplyChanged(fg.rast_state());
            fg.rast_state() = rast_state;

            const uint64_t DrawMask = BDB::BitAlphaBlend | BDB::BitMoving | BDB::BitAlphaTest | BDB::BitTwoSided;
            i = _draw_range_ext2(fg, dummy_white_main, batch_indices, batches, i, DrawMask, cur_mat_id, &draws_count);
        }
    }
}
