#include "ExGBufferFill.h"

#include <Ren/Context.h>
#include <Ren/DebugMarker.h>
#include <Ren/DrawCall.h>
#include <Ren/Vk/VKCtx.h>
#include <Ren/utils/Span.h>

#include "../Renderer_DrawList.h"
#include "../framegraph/FgBuilder.h"

namespace ExSharedInternal {
uint32_t _draw_range_ext(const Ren::ApiContext &api, VkCommandBuffer cmd_buf, const Ren::PipelineMain &pipeline,
                         Ren::Span<const uint32_t> batch_indices, Ren::Span<const Eng::basic_draw_batch_t> batches,
                         uint32_t i, uint64_t mask, uint32_t materials_per_descriptor,
                         Ren::Span<const VkDescriptorSet> descr_sets, int *draws_count);
uint32_t _skip_range(Ren::Span<const uint32_t> batch_indices, Ren::Span<const Eng::basic_draw_batch_t> batches,
                     uint32_t i, uint64_t mask);
} // namespace ExSharedInternal

void Eng::ExGBufferFill::DrawOpaque(const FgContext &fg, const Ren::ImageRWHandle albedo,
                                    const Ren::ImageRWHandle normal, const Ren::ImageRWHandle spec,
                                    const Ren::ImageRWHandle depth) {
    using namespace ExSharedInternal;

    const Ren::ApiContext &api = fg.ren_ctx().api();
    const Ren::StoragesRef &storages = fg.storages();

    //
    // Prepare descriptor sets
    //
    const Ren::BufferROHandle attrib_bufs[] = {fg.AccessROBuffer(args_->vtx_buf1), fg.AccessROBuffer(args_->vtx_buf2)};
    const Ren::BufferROHandle ndx_buf = fg.AccessROBuffer(args_->ndx_buf);

    const Ren::BufferROHandle instances = fg.AccessROBuffer(args_->instances);
    const Ren::BufferROHandle instance_indices = fg.AccessROBuffer(args_->instance_indices);
    const Ren::BufferROHandle unif_shared_data = fg.AccessROBuffer(args_->shared_data);
    const Ren::BufferROHandle materials = fg.AccessROBuffer(args_->materials);
    const Ren::BufferROHandle cells = fg.AccessROBuffer(args_->cells);
    const Ren::BufferROHandle items = fg.AccessROBuffer(args_->items);
    const Ren::BufferROHandle decals = fg.AccessROBuffer(args_->decals);

    const Ren::ImageROHandle noise = fg.AccessROImage(args_->noise);
    const Ren::ImageROHandle dummy_black = fg.AccessROImage(args_->dummy_black);

    VkDescriptorSet descr_sets[2];
    { // allocate descriptors
        const Ren::PipelineMain &pi_vegetation_main = storages.pipelines[pi_vegetation_[0]].first;
        const Ren::ProgramMain &pr_main = storages.programs[pi_vegetation_main.prog].first;

        const Ren::Binding bindings[] = {{Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, unif_shared_data},
                                         {Ren::eBindTarget::UTBuf, BIND_INST_BUF, instances},
                                         {Ren::eBindTarget::UTBuf, BIND_DECAL_BUF, decals},
                                         {Ren::eBindTarget::UTBuf, BIND_CELLS_BUF, cells},
                                         {Ren::eBindTarget::UTBuf, BIND_ITEMS_BUF, items},
                                         {Ren::eBindTarget::SBufRO, BIND_INST_NDX_BUF, instance_indices},
                                         {Ren::eBindTarget::SBufRO, BIND_MATERIALS_BUF, materials},
                                         {Ren::eBindTarget::TexSampled, BIND_NOISE_TEX, noise},
                                         {Ren::eBindTarget::TexSampled, BIND_DECAL_TEX, dummy_black}};
        descr_sets[0] =
            PrepareDescriptorSet(api, storages, pr_main.descr_set_layouts[0], bindings, fg.descr_alloc(), fg.log());
        descr_sets[1] = bindless_tex_->textures_descr_sets[0];
    }

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
    uint32_t i = 0;

    { // solid meshes
        const Ren::PipelineMain &pi_simple0_main = storages.pipelines[pi_simple_[0]].first;
        const Ren::PipelineMain &pi_simple1_main = storages.pipelines[pi_simple_[1]].first;
        const Ren::PipelineMain &pi_simple2_main = storages.pipelines[pi_simple_[2]].first;

        const Ren::RenderPass &rp = storages.render_passes[pi_simple0_main.render_pass];

        const Ren::ImageRWHandle color_targets[] = {albedo, normal, spec};
        const Ren::FramebufferHandle fb =
            fg.FindOrCreateFramebuffer(pi_simple0_main.render_pass, depth, depth, color_targets);

        VkRenderPassBeginInfo rp_begin_info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rp_begin_info.renderPass = rp.handle;
        rp_begin_info.framebuffer = storages.framebuffers[fb].first.handle;
        rp_begin_info.renderArea = {{0, 0}, {uint32_t(view_state_->ren_res[0]), uint32_t(view_state_->ren_res[1])}};
        const VkClearValue clear_values[4] = {{}, {}, {}, {}};
        rp_begin_info.pClearValues = clear_values;
        rp_begin_info.clearValueCount = 4;
        api.vkCmdBeginRenderPass(cmd_buf, &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);

        { // Simple meshes
            Ren::DebugMarker _m(api, cmd_buf, "SIMPLE");

            const Ren::VertexInput &vtx_input = storages.vtx_inputs[pi_simple0_main.vtx_input];
            VertexInput_BindBuffers(api, vtx_input, storages.buffers, attrib_bufs, ndx_buf, cmd_buf, 0,
                                    VK_INDEX_TYPE_UINT32);

            { // solid one-sided
                Ren::DebugMarker _mm(api, cmd_buf, "SOLID-ONE-SIDED");
                api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple0_main.pipeline);
                api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple0_main.layout, 0, 2,
                                            descr_sets, 0, nullptr);
                i = _draw_range_ext(api, cmd_buf, pi_simple0_main, batch_indices, batches, i, 0,
                                    materials_per_descriptor, bindless_tex_->textures_descr_sets, &draws_count);
                api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple1_main.pipeline);
                i = _draw_range_ext(api, cmd_buf, pi_simple1_main, batch_indices, batches, i, BDB::BitBackSided,
                                    materials_per_descriptor, bindless_tex_->textures_descr_sets, &draws_count);
            }

            { // solid two-sided
                Ren::DebugMarker _mm(api, cmd_buf, "SOLID-TWO-SIDED");
                api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple2_main.pipeline);
                api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple2_main.layout, 0, 2,
                                            descr_sets, 0, nullptr);
                i = _draw_range_ext(api, cmd_buf, pi_simple2_main, batch_indices, batches, i, BDB::BitTwoSided,
                                    materials_per_descriptor, bindless_tex_->textures_descr_sets, &draws_count);
            }

            { // moving solid one-sided
                Ren::DebugMarker _mm(api, cmd_buf, "MOVING-SOLID-ONE-SIDED");
                api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple0_main.pipeline);
                api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple0_main.layout, 0, 2,
                                            descr_sets, 0, nullptr);
                i = _draw_range_ext(api, cmd_buf, pi_simple0_main, batch_indices, batches, i, BDB::BitMoving,
                                    materials_per_descriptor, bindless_tex_->textures_descr_sets, &draws_count);
            }

            { // moving solid two-sided
                Ren::DebugMarker _mm(api, cmd_buf, "MOVING-SOLID-TWO-SIDED");
                api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple2_main.pipeline);
                api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple2_main.layout, 0, 2,
                                            descr_sets, 0, nullptr);
                i = _draw_range_ext(api, cmd_buf, pi_simple2_main, batch_indices, batches, i,
                                    BDB::BitMoving | BDB::BitTwoSided, materials_per_descriptor,
                                    bindless_tex_->textures_descr_sets, &draws_count);
            }

            { // alpha-tested one-sided
                Ren::DebugMarker _mm(api, cmd_buf, "ALPHA-ONE-SIDED");
                api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple0_main.pipeline);
                api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple0_main.layout, 0, 2,
                                            descr_sets, 0, nullptr);
                i = _draw_range_ext(api, cmd_buf, pi_simple0_main, batch_indices, batches, i, BDB::BitAlphaTest,
                                    materials_per_descriptor, bindless_tex_->textures_descr_sets, &draws_count);
                api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple1_main.pipeline);
                i = _draw_range_ext(api, cmd_buf, pi_simple1_main, batch_indices, batches, i,
                                    BDB::BitAlphaTest | BDB::BitBackSided, materials_per_descriptor,
                                    bindless_tex_->textures_descr_sets, &draws_count);
            }

            { // alpha-tested two-sided
                Ren::DebugMarker _mm(api, cmd_buf, "ALPHA-TWO-SIDED");
                api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple2_main.pipeline);
                api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple2_main.layout, 0, 2,
                                            descr_sets, 0, nullptr);
                i = _draw_range_ext(api, cmd_buf, pi_simple2_main, batch_indices, batches, i,
                                    BDB::BitAlphaTest | BDB::BitTwoSided, materials_per_descriptor,
                                    bindless_tex_->textures_descr_sets, &draws_count);
            }

            { // moving alpha-tested one-sided
                Ren::DebugMarker _mm(api, cmd_buf, "MOVING-ALPHA-ONE-SIDED");
                api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple0_main.pipeline);
                api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple0_main.layout, 0, 2,
                                            descr_sets, 0, nullptr);
                i = _draw_range_ext(api, cmd_buf, pi_simple0_main, batch_indices, batches, i,
                                    BDB::BitMoving | BDB::BitAlphaTest, materials_per_descriptor,
                                    bindless_tex_->textures_descr_sets, &draws_count);
                api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple1_main.pipeline);
                i = _draw_range_ext(api, cmd_buf, pi_simple1_main, batch_indices, batches, i,
                                    BDB::BitMoving | BDB::BitAlphaTest | BDB::BitBackSided, materials_per_descriptor,
                                    bindless_tex_->textures_descr_sets, &draws_count);
            }

            { // moving alpha-tested two-sided
                Ren::DebugMarker _mm(api, cmd_buf, "MOVING-ALPHA-TWO-SIDED");
                api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple2_main.pipeline);
                api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple2_main.layout, 0, 2,
                                            descr_sets, 0, nullptr);
                i = _draw_range_ext(api, cmd_buf, pi_simple2_main, batch_indices, batches, i,
                                    BDB::BitMoving | BDB::BitAlphaTest | BDB::BitTwoSided, materials_per_descriptor,
                                    bindless_tex_->textures_descr_sets, &draws_count);
            }
        }

        { // Vegetation meshes
            Ren::DebugMarker _m(api, cmd_buf, "VEGETATION");

            const Ren::PipelineMain &pi_vegetation0_main = storages.pipelines[pi_vegetation_[0]].first;
            const Ren::PipelineMain &pi_vegetation1_main = storages.pipelines[pi_vegetation_[1]].first;

            const Ren::VertexInput &vtx_input = storages.vtx_inputs[pi_vegetation0_main.vtx_input];
            VertexInput_BindBuffers(api, vtx_input, storages.buffers, attrib_bufs, ndx_buf, cmd_buf, 0,
                                    VK_INDEX_TYPE_UINT32);

            { // vegetation solid one-sided
                Ren::DebugMarker _mm(api, cmd_buf, "VEGE-SOLID-ONE-SIDED");
                api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_vegetation0_main.pipeline);
                api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_vegetation0_main.layout, 0, 2,
                                            descr_sets, 0, nullptr);
                i = _draw_range_ext(api, cmd_buf, pi_vegetation0_main, batch_indices, batches, i, BDB::BitsVege,
                                    materials_per_descriptor, bindless_tex_->textures_descr_sets, &draws_count);
            }

            { // vegetation solid two-sided
                Ren::DebugMarker _mm(api, cmd_buf, "SOLID-TWO-SIDED");
                api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_vegetation1_main.pipeline);
                api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_vegetation1_main.layout, 0, 2,
                                            descr_sets, 0, nullptr);
                const uint64_t DrawMask = BDB::BitsVege | BDB::BitTwoSided;
                i = _draw_range_ext(api, cmd_buf, pi_vegetation1_main, batch_indices, batches, i, DrawMask,
                                    materials_per_descriptor, bindless_tex_->textures_descr_sets, &draws_count);
            }

            { // vegetation moving solid one-sided
                Ren::DebugMarker _mm(api, cmd_buf, "VEGE-MOVING-SOLID-ONE-SIDED");
                api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_vegetation0_main.pipeline);
                api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_vegetation0_main.layout, 0, 2,
                                            descr_sets, 0, nullptr);
                const uint64_t DrawMask = BDB::BitsVege | BDB::BitMoving;
                i = _draw_range_ext(api, cmd_buf, pi_vegetation0_main, batch_indices, batches, i, DrawMask,
                                    materials_per_descriptor, bindless_tex_->textures_descr_sets, &draws_count);
            }

            { // vegetation moving solid two-sided
                Ren::DebugMarker _mm(api, cmd_buf, "VEGE-MOVING-SOLID-TWO-SIDED");
                api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_vegetation1_main.pipeline);
                api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_vegetation1_main.layout, 0, 2,
                                            descr_sets, 0, nullptr);
                const uint64_t DrawMask = BDB::BitsVege | BDB::BitMoving | BDB::BitTwoSided;
                i = _draw_range_ext(api, cmd_buf, pi_vegetation1_main, batch_indices, batches, i, DrawMask,
                                    materials_per_descriptor, bindless_tex_->textures_descr_sets, &draws_count);
            }

            { // vegetation alpha-tested one-sided
                Ren::DebugMarker _mm(api, cmd_buf, "VEGE-ALPHA-ONE-SIDED");
                api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_vegetation0_main.pipeline);
                api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_vegetation0_main.layout, 0, 2,
                                            descr_sets, 0, nullptr);
                const uint64_t DrawMask = BDB::BitsVege | BDB::BitAlphaTest;
                i = _draw_range_ext(api, cmd_buf, pi_vegetation0_main, batch_indices, batches, i, DrawMask,
                                    materials_per_descriptor, bindless_tex_->textures_descr_sets, &draws_count);
            }

            { // vegetation alpha-tested two-sided
                Ren::DebugMarker _mm(api, cmd_buf, "VEGE-ALPHA-TWO-SIDED");
                api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_vegetation1_main.pipeline);
                api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_vegetation1_main.layout, 0, 2,
                                            descr_sets, 0, nullptr);
                const uint64_t DrawMask = BDB::BitsVege | BDB::BitAlphaTest | BDB::BitTwoSided;
                i = _draw_range_ext(api, cmd_buf, pi_vegetation1_main, batch_indices, batches, i, DrawMask,
                                    materials_per_descriptor, bindless_tex_->textures_descr_sets, &draws_count);
            }

            { // vegetation moving alpha-tested one-sided
                Ren::DebugMarker _mm(api, cmd_buf, "VEGE-MOVING-ALPHA-ONE-SIDED");
                api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_vegetation0_main.pipeline);
                api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_vegetation0_main.layout, 0, 2,
                                            descr_sets, 0, nullptr);
                const uint64_t DrawMask = BDB::BitsVege | BDB::BitMoving | BDB::BitAlphaTest;
                i = _draw_range_ext(api, cmd_buf, pi_vegetation0_main, batch_indices, batches, i, DrawMask,
                                    materials_per_descriptor, bindless_tex_->textures_descr_sets, &draws_count);
            }

            { // vegetation moving alpha-tested two-sided
                Ren::DebugMarker _mm(api, cmd_buf, "VEGE-MOVING-ALPHA-TWO-SIDED");
                api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_vegetation1_main.pipeline);
                api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_vegetation1_main.layout, 0, 2,
                                            descr_sets, 0, nullptr);
                const uint64_t DrawMask = BDB::BitsVege | BDB::BitMoving | BDB::BitAlphaTest | BDB::BitTwoSided;
                i = _draw_range_ext(api, cmd_buf, pi_vegetation1_main, batch_indices, batches, i, DrawMask,
                                    materials_per_descriptor, bindless_tex_->textures_descr_sets, &draws_count);
            }
        }

        { // Skinned meshes
            Ren::DebugMarker _m(api, cmd_buf, "SKINNED");

            const Ren::VertexInput &vtx_input = storages.vtx_inputs[pi_simple0_main.vtx_input];
            VertexInput_BindBuffers(api, vtx_input, storages.buffers, attrib_bufs, ndx_buf, cmd_buf, 0,
                                    VK_INDEX_TYPE_UINT32);

            { // skinned solid one-sided
                Ren::DebugMarker _mm(api, cmd_buf, "SKIN-SOLID-ONE-SIDED");
                api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple0_main.pipeline);
                api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple0_main.layout, 0, 2,
                                            descr_sets, 0, nullptr);
                i = _draw_range_ext(api, cmd_buf, pi_simple0_main, batch_indices, batches, i, BDB::BitsSkinned,
                                    materials_per_descriptor, bindless_tex_->textures_descr_sets, &draws_count);
            }

            { // skinned solid two-sided
                Ren::DebugMarker _mm(api, cmd_buf, "SKIN-SOLID-TWO-SIDED");
                api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple2_main.pipeline);
                api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple2_main.layout, 0, 2,
                                            descr_sets, 0, nullptr);
                const uint64_t DrawMask = BDB::BitsSkinned | BDB::BitTwoSided;
                i = _draw_range_ext(api, cmd_buf, pi_simple2_main, batch_indices, batches, i, DrawMask,
                                    materials_per_descriptor, bindless_tex_->textures_descr_sets, &draws_count);
            }

            { // skinned moving solid one-sided
                Ren::DebugMarker _mm(api, cmd_buf, "SKIN-MOVING-SOLID-ONE-SIDED");
                api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple0_main.pipeline);
                api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple0_main.layout, 0, 2,
                                            descr_sets, 0, nullptr);
                const uint64_t DrawMask = BDB::BitsSkinned | BDB::BitMoving;
                i = _draw_range_ext(api, cmd_buf, pi_simple0_main, batch_indices, batches, i, DrawMask,
                                    materials_per_descriptor, bindless_tex_->textures_descr_sets, &draws_count);
            }

            { // skinned moving solid two-sided
                Ren::DebugMarker _mm(api, cmd_buf, "SKIN-MOVING-SOLID-TWO-SIDED");
                api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple2_main.pipeline);
                api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple2_main.layout, 0, 2,
                                            descr_sets, 0, nullptr);
                const uint64_t DrawMask = BDB::BitsSkinned | BDB::BitMoving | BDB::BitTwoSided;
                i = _draw_range_ext(api, cmd_buf, pi_simple2_main, batch_indices, batches, i, DrawMask,
                                    materials_per_descriptor, bindless_tex_->textures_descr_sets, &draws_count);
            }

            { // skinned alpha-tested one-sided
                Ren::DebugMarker _mm(api, cmd_buf, "SKIN-ALPHA-ONE-SIDED");
                api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple0_main.pipeline);
                api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple0_main.layout, 0, 2,
                                            descr_sets, 0, nullptr);
                const uint64_t DrawMask = BDB::BitsSkinned | BDB::BitAlphaTest;
                i = _draw_range_ext(api, cmd_buf, pi_simple0_main, batch_indices, batches, i, DrawMask,
                                    materials_per_descriptor, bindless_tex_->textures_descr_sets, &draws_count);
            }

            { // skinned alpha-tested two-sided
                Ren::DebugMarker _mm(api, cmd_buf, "SKIN-ALPHA-TWO-SIDED");
                api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple2_main.pipeline);
                api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple2_main.layout, 0, 2,
                                            descr_sets, 0, nullptr);
                const uint64_t DrawMask = BDB::BitsSkinned | BDB::BitAlphaTest | BDB::BitTwoSided;
                i = _draw_range_ext(api, cmd_buf, pi_simple2_main, batch_indices, batches, i, DrawMask,
                                    materials_per_descriptor, bindless_tex_->textures_descr_sets, &draws_count);
            }

            { // skinned moving alpha-tested one-sided
                Ren::DebugMarker _mm(api, cmd_buf, "SKIN-MOVING-ALPHA-ONE-SIDED");
                api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple0_main.pipeline);
                api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple0_main.layout, 0, 2,
                                            descr_sets, 0, nullptr);
                const uint64_t DrawMask = BDB::BitsSkinned | BDB::BitMoving | BDB::BitAlphaTest;
                i = _draw_range_ext(api, cmd_buf, pi_simple0_main, batch_indices, batches, i, DrawMask,
                                    materials_per_descriptor, bindless_tex_->textures_descr_sets, &draws_count);
            }

            { // skinned moving alpha-tested two-sided
                Ren::DebugMarker _mm(api, cmd_buf, "SKIN-MOVING-ALPHA-TWO-SIDED");
                api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple2_main.pipeline);
                api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple2_main.layout, 0, 2,
                                            descr_sets, 0, nullptr);
                const uint64_t DrawMask = BDB::BitsSkinned | BDB::BitMoving | BDB::BitAlphaTest | BDB::BitTwoSided;
                i = _draw_range_ext(api, cmd_buf, pi_simple2_main, batch_indices, batches, i, DrawMask,
                                    materials_per_descriptor, bindless_tex_->textures_descr_sets, &draws_count);
            }
        }

        api.vkCmdEndRenderPass(cmd_buf);
    }
}
