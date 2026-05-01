#include "ExOITBlendLayer.h"

#include <Ren/Context.h>
#include <Ren/DebugMarker.h>
#include <Ren/RastState.h>
#include <Ren/Vk/VKCtx.h>

#include "../PrimDraw.h"
#include "../Renderer_DrawList.h"
#include "../framegraph/FgBuilder.h"
#include "../shaders/blit_oit_depth_interface.h"
#include "../shaders/oit_blend_layer_interface.h"

namespace ExSharedInternal {
uint32_t _draw_range_ext(const Ren::ApiContext &api, VkCommandBuffer cmd_buf, const Ren::PipelineMain &pipeline,
                         Ren::Span<const uint32_t> batch_indices, Ren::Span<const Eng::basic_draw_batch_t> batches,
                         uint32_t i, uint64_t mask, uint32_t materials_per_descriptor,
                         Ren::Span<const VkDescriptorSet> descr_sets, int *draws_count);
}

void Eng::ExOITBlendLayer::DrawTransparent(const FgContext &fg, const Ren::ImageRWHandle depth,
                                           const Ren::ImageRWHandle color) {
    using namespace ExSharedInternal;

    const Ren::ApiContext &api = fg.ren_ctx().api();
    const Ren::StoragesRef &storages = fg.storages();

    const Ren::BufferROHandle attrib_bufs[] = {fg.AccessROBuffer(args_->vtx_buf1), fg.AccessROBuffer(args_->vtx_buf2)};
    const Ren::BufferROHandle ndx_buf = fg.AccessROBuffer(args_->ndx_buf);

    const Ren::ImageROHandle noise = fg.AccessROImage(args_->noise);
    const Ren::ImageROHandle shadow_depth = fg.AccessROImage(args_->shadow_depth);
    const Ren::ImageROHandle ltc_luts = fg.AccessROImage(args_->ltc_luts);
    const Ren::ImageROHandle env = fg.AccessROImage(args_->env);
    const Ren::BufferROHandle instances = fg.AccessROBuffer(args_->instances);
    const Ren::BufferROHandle instance_indices = fg.AccessROBuffer(args_->instance_indices);
    const Ren::BufferROHandle unif_shared_data = fg.AccessROBuffer(args_->shared_data);
    const Ren::BufferROHandle materials = fg.AccessROBuffer(args_->materials);
    const Ren::BufferROHandle cells = fg.AccessROBuffer(args_->cells);
    const Ren::BufferROHandle items = fg.AccessROBuffer(args_->items);
    const Ren::BufferROHandle lights = fg.AccessROBuffer(args_->lights);
    const Ren::BufferROHandle decals = fg.AccessROBuffer(args_->decals);
    const Ren::BufferROHandle oit_depth = fg.AccessROBuffer(args_->oit_depth);

    const Ren::ImageROHandle back_color = fg.AccessROImage(args_->back_color);
    const Ren::ImageROHandle back_depth = fg.AccessROImage(args_->back_depth);

    Ren::ImageROHandle irr, dist, off;
    if (args_->irradiance) {
        irr = fg.AccessROImage(args_->irradiance);
        dist = fg.AccessROImage(args_->distance);
        off = fg.AccessROImage(args_->offset);
    }

    Ren::ImageROHandle specular = {};
    if (args_->oit_specular) {
        specular = fg.AccessROImage(args_->oit_specular);
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
        uniform_params.img_size[0] = view_state_->ren_res[0];
        uniform_params.img_size[1] = view_state_->ren_res[1];
        uniform_params.layer_index = args_->depth_layer_index;

        const Ren::RenderTarget depth_target = {depth, Ren::eLoadOp::Load, Ren::eStoreOp::Store, Ren::eLoadOp::Load,
                                                Ren::eStoreOp::Store};

        prim_draw_.DrawPrim(fg.cmd_buf(), PrimDraw::ePrim::Quad, prog_oit_blit_depth_, depth_target, {}, rast_state,
                            fg.rast_state(), bindings, &uniform_params, sizeof(uniform_params), 0, fg.framebuffers());
    }

    Ren::SmallVector<Ren::Binding, 16> bindings = {
        {Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, unif_shared_data},
        {Ren::eBindTarget::UTBuf, OITBlendLayer::CELLS_BUF_SLOT, cells},
        {Ren::eBindTarget::UTBuf, OITBlendLayer::ITEMS_BUF_SLOT, items},
        {Ren::eBindTarget::UTBuf, OITBlendLayer::LIGHT_BUF_SLOT, lights},
        {Ren::eBindTarget::UTBuf, OITBlendLayer::DECAL_BUF_SLOT, decals},
        {Ren::eBindTarget::UTBuf, BIND_INST_BUF, instances},
        {Ren::eBindTarget::SBufRO, BIND_INST_NDX_BUF, instance_indices},
        {Ren::eBindTarget::SBufRO, BIND_MATERIALS_BUF, materials},
        {Ren::eBindTarget::TexSampled, BIND_NOISE_TEX, noise},
        {Ren::eBindTarget::TexSampled, OITBlendLayer::SHADOW_TEX_SLOT, shadow_depth},
        {Ren::eBindTarget::TexSampled, OITBlendLayer::LTC_LUTS_TEX_SLOT, ltc_luts},
        {Ren::eBindTarget::TexSampled, OITBlendLayer::ENV_TEX_SLOT, env},
        {Ren::eBindTarget::TexSampled, OITBlendLayer::BACK_COLOR_TEX_SLOT, back_color},
        {Ren::eBindTarget::TexSampled, OITBlendLayer::BACK_DEPTH_TEX_SLOT, {back_depth, 1}}};
    if (irr) {
        bindings.emplace_back(Ren::eBindTarget::TexSampled, OITBlendLayer::IRRADIANCE_TEX_SLOT, irr);
        bindings.emplace_back(Ren::eBindTarget::TexSampled, OITBlendLayer::DISTANCE_TEX_SLOT, dist);
        bindings.emplace_back(Ren::eBindTarget::TexSampled, OITBlendLayer::OFFSET_TEX_SLOT, off);
    }
    if (specular) {
        bindings.emplace_back(Ren::eBindTarget::TexSampled, OITBlendLayer::SPEC_TEX_SLOT, specular);
    }

    const Ren::PipelineMain &pi_vegetation0_main = storages.pipelines[pi_vegetation_[0]].first;
    const Ren::ProgramMain &pr_vegetation0_main = storages.programs[pi_vegetation0_main.prog].first;

    VkDescriptorSet descr_sets[2];
    descr_sets[0] = PrepareDescriptorSet(api, storages, pr_vegetation0_main.descr_set_layouts[0], bindings,
                                         fg.descr_alloc(), fg.log());
    descr_sets[1] = bindless_tex_->textures_descr_sets[0];

    using BDB = basic_draw_batch_t;

    const uint32_t materials_per_descriptor = api.max_combined_image_samplers / MAX_TEX_PER_MATERIAL;

    VkCommandBuffer cmd_buf = fg.cmd_buf();

    const VkViewport viewport = {0.0f, 0.0f, float(view_state_->ren_res[0]), float(view_state_->ren_res[1]),
                                 0.0f, 1.0f};
    api.vkCmdSetViewport(cmd_buf, 0, 1, &viewport);
    const VkRect2D scissor = {{0, 0}, {uint32_t(view_state_->ren_res[0]), uint32_t(view_state_->ren_res[1])}};
    api.vkCmdSetScissor(cmd_buf, 0, 1, &scissor);

    const Ren::Span<const basic_draw_batch_t> batches = {(*p_list_)->basic_batches};
    const Ren::Span<const uint32_t> batch_indices = {(*p_list_)->basic_batch_indices};

    int draws_count = 0;
    uint32_t i = (*p_list_)->alpha_blend_start_index;

    { // solid meshes
        const Ren::PipelineMain &pi_simple0_main = storages.pipelines[pi_simple_[0]].first;
        const Ren::PipelineMain &pi_simple1_main = storages.pipelines[pi_simple_[1]].first;
        const Ren::PipelineMain &pi_simple2_main = storages.pipelines[pi_simple_[2]].first;

        const Ren::RenderPass &rp_simple0 = storages.render_passes[pi_simple0_main.render_pass];

        const Ren::ImageRWHandle color_targets[] = {color};
        const Ren::FramebufferHandle fb =
            fg.FindOrCreateFramebuffer(pi_simple0_main.render_pass, depth, depth, color_targets);

        VkRenderPassBeginInfo rp_begin_info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rp_begin_info.renderPass = rp_simple0.handle;
        rp_begin_info.framebuffer = storages.framebuffers[fb].first.handle;
        rp_begin_info.renderArea = {{0, 0}, {uint32_t(view_state_->ren_res[0]), uint32_t(view_state_->ren_res[1])}};
        const VkClearValue clear_values[4] = {{}, {}, {}, {}};
        rp_begin_info.pClearValues = clear_values;
        rp_begin_info.clearValueCount = 4;
        api.vkCmdBeginRenderPass(cmd_buf, &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);

        { // Simple meshes
            Ren::DebugMarker _m(api, cmd_buf, "SIMPLE");

            const Ren::VertexInput &vtx_input_main = storages.vtx_inputs[pi_simple0_main.vtx_input];
            VertexInput_BindBuffers(api, vtx_input_main, storages.buffers, attrib_bufs, ndx_buf, cmd_buf, 0,
                                    VK_INDEX_TYPE_UINT32);

            { // solid one-sided
                Ren::DebugMarker _mm(api, cmd_buf, "SOLID-ONE-SIDED");
                api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple0_main.pipeline);
                api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple0_main.layout, 0, 2,
                                            descr_sets, 0, nullptr);
                i = _draw_range_ext(api, cmd_buf, pi_simple0_main, batch_indices, batches, i, BDB::BitAlphaBlend,
                                    materials_per_descriptor, bindless_tex_->textures_descr_sets, &draws_count);
                api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple1_main.pipeline);
                i = _draw_range_ext(api, cmd_buf, pi_simple1_main, batch_indices, batches, i,
                                    BDB::BitAlphaBlend | BDB::BitBackSided, materials_per_descriptor,
                                    bindless_tex_->textures_descr_sets, &draws_count);
            }
            { // solid two-sided
                Ren::DebugMarker _mm(api, cmd_buf, "SOLID-TWO-SIDED");
                api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple2_main.pipeline);
                api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple2_main.layout, 0, 2,
                                            descr_sets, 0, nullptr);
                i = _draw_range_ext(api, cmd_buf, pi_simple2_main, batch_indices, batches, i,
                                    BDB::BitAlphaBlend | BDB::BitTwoSided, materials_per_descriptor,
                                    bindless_tex_->textures_descr_sets, &draws_count);
            }
            { // moving solid one-sided
                Ren::DebugMarker _mm(api, cmd_buf, "MOVING-SOLID-ONE-SIDED");
                api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple0_main.pipeline);
                api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple0_main.layout, 0, 2,
                                            descr_sets, 0, nullptr);
                i = _draw_range_ext(api, cmd_buf, pi_simple0_main, batch_indices, batches, i,
                                    BDB::BitAlphaBlend | BDB::BitMoving, materials_per_descriptor,
                                    bindless_tex_->textures_descr_sets, &draws_count);
            }
            { // moving solid two-sided
                Ren::DebugMarker _mm(api, cmd_buf, "MOVING-SOLID-TWO-SIDED");
                api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple2_main.pipeline);
                api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple2_main.layout, 0, 2,
                                            descr_sets, 0, nullptr);
                i = _draw_range_ext(api, cmd_buf, pi_simple2_main, batch_indices, batches, i,
                                    BDB::BitAlphaBlend | BDB::BitMoving | BDB::BitTwoSided, materials_per_descriptor,
                                    bindless_tex_->textures_descr_sets, &draws_count);
            }
            { // alpha-tested one-sided
                Ren::DebugMarker _mm(api, cmd_buf, "ALPHA-ONE-SIDED");
                api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple0_main.pipeline);
                api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple0_main.layout, 0, 2,
                                            descr_sets, 0, nullptr);
                i = _draw_range_ext(api, cmd_buf, pi_simple0_main, batch_indices, batches, i,
                                    BDB::BitAlphaBlend | BDB::BitAlphaTest, materials_per_descriptor,
                                    bindless_tex_->textures_descr_sets, &draws_count);
                api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple1_main.pipeline);
                i = _draw_range_ext(api, cmd_buf, pi_simple1_main, batch_indices, batches, i,
                                    BDB::BitAlphaBlend | BDB::BitAlphaTest | BDB::BitBackSided,
                                    materials_per_descriptor, bindless_tex_->textures_descr_sets, &draws_count);
            }
            { // alpha-tested two-sided
                Ren::DebugMarker _mm(api, cmd_buf, "ALPHA-TWO-SIDED");
                api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple2_main.pipeline);
                api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple2_main.layout, 0, 2,
                                            descr_sets, 0, nullptr);
                i = _draw_range_ext(api, cmd_buf, pi_simple2_main, batch_indices, batches, i,
                                    BDB::BitAlphaBlend | BDB::BitAlphaTest | BDB::BitTwoSided, materials_per_descriptor,
                                    bindless_tex_->textures_descr_sets, &draws_count);
            }
            { // moving alpha-tested one-sided
                Ren::DebugMarker _mm(api, cmd_buf, "MOVING-ALPHA-ONE-SIDED");
                api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple0_main.pipeline);
                api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple0_main.layout, 0, 2,
                                            descr_sets, 0, nullptr);
                i = _draw_range_ext(api, cmd_buf, pi_simple0_main, batch_indices, batches, i,
                                    BDB::BitAlphaBlend | BDB::BitMoving | BDB::BitAlphaTest, materials_per_descriptor,
                                    bindless_tex_->textures_descr_sets, &draws_count);
                api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple1_main.pipeline);
                i = _draw_range_ext(api, cmd_buf, pi_simple1_main, batch_indices, batches, i,
                                    BDB::BitAlphaBlend | BDB::BitMoving | BDB::BitAlphaTest | BDB::BitBackSided,
                                    materials_per_descriptor, bindless_tex_->textures_descr_sets, &draws_count);
            }
            { // moving alpha-tested two-sided
                Ren::DebugMarker _mm(api, cmd_buf, "MOVING-ALPHA-TWO-SIDED");
                api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple2_main.pipeline);
                api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple2_main.layout, 0, 2,
                                            descr_sets, 0, nullptr);
                i = _draw_range_ext(api, cmd_buf, pi_simple2_main, batch_indices, batches, i,
                                    BDB::BitAlphaBlend | BDB::BitMoving | BDB::BitAlphaTest | BDB::BitTwoSided,
                                    materials_per_descriptor, bindless_tex_->textures_descr_sets, &draws_count);
            }
        }

        api.vkCmdEndRenderPass(cmd_buf);
    }
}
