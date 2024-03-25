#include "RpGBufferFill.h"

#include <Ren/Context.h>
#include <Ren/DebugMarker.h>
#include <Ren/DescriptorPool.h>
#include <Ren/RastState.h>
#include <Ren/Span.h>
#include <Ren/VKCtx.h>

#include "../Renderer_Structs.h"

namespace RpSharedInternal {
uint32_t _draw_range_ext(Ren::ApiContext *api_ctx, VkCommandBuffer cmd_buf, const Ren::Pipeline &pipeline,
                         Ren::Span<const uint32_t> batch_indices, Ren::Span<const Eng::BasicDrawBatch> batches,
                         uint32_t i, uint32_t mask, const uint32_t materials_per_descriptor,
                         Ren::Span<const VkDescriptorSet> descr_sets, int *draws_count);
uint32_t _skip_range(Ren::Span<const uint32_t> batch_indices, Ren::Span<const Eng::BasicDrawBatch> batches, uint32_t i,
                     uint32_t mask);
} // namespace RpSharedInternal

void Eng::RpGBufferFill::DrawOpaque(RpBuilder &builder) {
    using namespace RpSharedInternal;

    auto &ctx = builder.ctx();
    auto *api_ctx = ctx.api_ctx();

    //
    // Prepare descriptor sets
    //
    RpAllocBuf &instances_buf = builder.GetReadBuffer(instances_buf_);
    RpAllocBuf &instance_indices_buf = builder.GetReadBuffer(instance_indices_buf_);
    RpAllocBuf &unif_shared_data_buf = builder.GetReadBuffer(shared_data_buf_);
    RpAllocBuf &materials_buf = builder.GetReadBuffer(materials_buf_);
    RpAllocBuf &cells_buf = builder.GetReadBuffer(cells_buf_);
    RpAllocBuf &items_buf = builder.GetReadBuffer(items_buf_);
    RpAllocBuf &decals_buf = builder.GetReadBuffer(decals_buf_);

    RpAllocTex &noise_tex = builder.GetReadTexture(noise_tex_);
    RpAllocTex &dummy_black = builder.GetReadTexture(dummy_black_);

    VkDescriptorSet descr_sets[2];

    { // allocate descriptors
        Ren::DescrSizes descr_sizes;
        descr_sizes.img_sampler_count = 2;
        descr_sizes.ubuf_count = 1;
        descr_sizes.sbuf_count = 2;
        descr_sizes.tbuf_count = 4;

        descr_sets[0] = ctx.default_descr_alloc()->Alloc(descr_sizes, descr_set_layout_);
        descr_sets[1] = bindless_tex_->textures_descr_sets[0];
    }

    { // update descriptor set
        const VkDescriptorImageInfo decal_info =
            decals_atlas_ ? decals_atlas_->vk_desc_image_info() : dummy_black.ref->vk_desc_image_info();
        const VkDescriptorImageInfo noise_info = noise_tex.ref->vk_desc_image_info();

        const VkBufferView decals_buf_view = decals_buf.tbos[0]->view();
        const VkBufferView cells_buf_view = cells_buf.tbos[0]->view();
        const VkBufferView items_buf_view = items_buf.tbos[0]->view();

        const VkDescriptorBufferInfo ubuf_info = {unif_shared_data_buf.ref->vk_handle(), 0, VK_WHOLE_SIZE};

        const VkBufferView instances_buf_view = instances_buf.tbos[0]->view();
        const VkDescriptorBufferInfo instance_indices_buf_info = {instance_indices_buf.ref->vk_handle(), 0,
                                                                  VK_WHOLE_SIZE};
        const VkDescriptorBufferInfo mat_buf_info = {materials_buf.ref->vk_handle(), 0, VK_WHOLE_SIZE};

        Ren::SmallVector<VkWriteDescriptorSet, 16> descr_writes;

        { // decals tex
            auto &descr_write = descr_writes.emplace_back();
            descr_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            descr_write.dstSet = descr_sets[0];
            descr_write.dstBinding = BIND_DECAL_TEX;
            descr_write.dstArrayElement = 0;
            descr_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descr_write.descriptorCount = 1;
            descr_write.pImageInfo = &decal_info;
        }
        { // noise tex
            auto &descr_write = descr_writes.emplace_back();
            descr_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            descr_write.dstSet = descr_sets[0];
            descr_write.dstBinding = BIND_NOISE_TEX;
            descr_write.dstArrayElement = 0;
            descr_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descr_write.descriptorCount = 1;
            descr_write.pImageInfo = &noise_info;
        }
        { // decals tbuf
            auto &descr_write = descr_writes.emplace_back();
            descr_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            descr_write.dstSet = descr_sets[0];
            descr_write.dstBinding = BIND_DECAL_BUF;
            descr_write.dstArrayElement = 0;
            descr_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
            descr_write.descriptorCount = 1;
            descr_write.pTexelBufferView = &decals_buf_view;
        }
        { // cells tbuf
            auto &descr_write = descr_writes.emplace_back();
            descr_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            descr_write.dstSet = descr_sets[0];
            descr_write.dstBinding = BIND_CELLS_BUF;
            descr_write.dstArrayElement = 0;
            descr_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
            descr_write.descriptorCount = 1;
            descr_write.pTexelBufferView = &cells_buf_view;
        }
        { // items tbuf
            auto &descr_write = descr_writes.emplace_back();
            descr_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            descr_write.dstSet = descr_sets[0];
            descr_write.dstBinding = BIND_ITEMS_BUF;
            descr_write.dstArrayElement = 0;
            descr_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
            descr_write.descriptorCount = 1;
            descr_write.pTexelBufferView = &items_buf_view;
        }
        { // instances tbuf
            auto &descr_write = descr_writes.emplace_back();
            descr_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            descr_write.dstSet = descr_sets[0];
            descr_write.dstBinding = BIND_INST_BUF;
            descr_write.dstArrayElement = 0;
            descr_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
            descr_write.descriptorCount = 1;
            descr_write.pTexelBufferView = &instances_buf_view;
        }
        { // instance indices sbuf
            auto &descr_write = descr_writes.emplace_back();
            descr_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            descr_write.dstSet = descr_sets[0];
            descr_write.dstBinding = BIND_INST_NDX_BUF;
            descr_write.dstArrayElement = 0;
            descr_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descr_write.descriptorCount = 1;
            descr_write.pBufferInfo = &instance_indices_buf_info;
        }
        { // shared data ubuf
            auto &descr_write = descr_writes.emplace_back();
            descr_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            descr_write.dstSet = descr_sets[0];
            descr_write.dstBinding = BIND_UB_SHARED_DATA_BUF;
            descr_write.dstArrayElement = 0;
            descr_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descr_write.descriptorCount = 1;
            descr_write.pBufferInfo = &ubuf_info;
        }
        { // materials sbuf
            auto &descr_write = descr_writes.emplace_back();
            descr_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            descr_write.dstSet = descr_sets[0];
            descr_write.dstBinding = BIND_MATERIALS_BUF;
            descr_write.dstArrayElement = 0;
            descr_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descr_write.descriptorCount = 1;
            descr_write.pBufferInfo = &mat_buf_info;
        }

        api_ctx->vkUpdateDescriptorSets(api_ctx->device, uint32_t(descr_writes.size()), descr_writes.cdata(), 0,
                                        nullptr);
    }

    using BDB = BasicDrawBatch;

    const uint32_t materials_per_descriptor = api_ctx->max_combined_image_samplers / MAX_TEX_PER_MATERIAL;

    VkCommandBuffer cmd_buf = api_ctx->draw_cmd_buf[api_ctx->backend_frame];

    const Ren::Span<const BasicDrawBatch> batches = {(*p_list_)->basic_batches};
    const Ren::Span<const uint32_t> batch_indices = {(*p_list_)->basic_batch_indices};

    int draws_count = 0;
    uint32_t i = 0;

    { // solid meshes
        VkRenderPassBeginInfo rp_begin_info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rp_begin_info.renderPass = rp_main_draw_.handle();
        rp_begin_info.framebuffer = main_draw_fb_[api_ctx->backend_frame][fb_to_use_].handle();
        rp_begin_info.renderArea = {0, 0, uint32_t(view_state_->act_res[0]), uint32_t(view_state_->act_res[1])};
        api_ctx->vkCmdBeginRenderPass(cmd_buf, &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);

        // TODO: optimize this (we do not care if it is moving/alpha tested/skinned etc.)!

        { // Simple meshes
            Ren::DebugMarker _m(api_ctx, cmd_buf, "SIMPLE");

            vi_simple_.BindBuffers(api_ctx, cmd_buf, 0, VK_INDEX_TYPE_UINT32);

            { // solid one-sided
                Ren::DebugMarker _mm(api_ctx, cmd_buf, "SOLID-ONE-SIDED");
                api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple_[0].handle());
                api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple_[0].layout(), 0, 2,
                                                 descr_sets, 0, nullptr);
                i = _draw_range_ext(api_ctx, cmd_buf, pi_simple_[0], batch_indices, batches, i, 0,
                                    materials_per_descriptor, bindless_tex_->textures_descr_sets, &draws_count);
                i = _skip_range(batch_indices, batches, i, BDB::BitCustomShaded);
            }

            { // solid two-sided
                Ren::DebugMarker _mm(api_ctx, cmd_buf, "SOLID-TWO-SIDED");
                api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple_[1].handle());
                api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple_[1].layout(), 0, 2,
                                                 descr_sets, 0, nullptr);
                i = _draw_range_ext(api_ctx, cmd_buf, pi_simple_[1], batch_indices, batches, i, BDB::BitTwoSided,
                                    materials_per_descriptor, bindless_tex_->textures_descr_sets, &draws_count);
                i = _skip_range(batch_indices, batches, i, BDB::BitTwoSided | BDB::BitCustomShaded);
            }

            { // moving solid one-sided
                Ren::DebugMarker _mm(api_ctx, cmd_buf, "MOVING-SOLID-ONE-SIDED");
                api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple_[0].handle());
                api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple_[0].layout(), 0, 2,
                                                 descr_sets, 0, nullptr);
                i = _draw_range_ext(api_ctx, cmd_buf, pi_simple_[0], batch_indices, batches, i, BDB::BitMoving,
                                    materials_per_descriptor, bindless_tex_->textures_descr_sets, &draws_count);
                i = _skip_range(batch_indices, batches, i, BDB::BitMoving | BDB::BitCustomShaded);
            }

            { // moving solid two-sided
                Ren::DebugMarker _mm(api_ctx, cmd_buf, "MOVING-SOLID-TWO-SIDED");
                api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple_[1].handle());
                api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple_[1].layout(), 0, 2,
                                                 descr_sets, 0, nullptr);
                i = _draw_range_ext(api_ctx, cmd_buf, pi_simple_[1], batch_indices, batches, i,
                                    BDB::BitMoving | BDB::BitTwoSided, materials_per_descriptor,
                                    bindless_tex_->textures_descr_sets, &draws_count);
                i = _skip_range(batch_indices, batches, i, BDB::BitMoving | BDB::BitTwoSided | BDB::BitCustomShaded);
            }

            { // alpha-tested one-sided
                Ren::DebugMarker _mm(api_ctx, cmd_buf, "ALPHA-ONE-SIDED");
                api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple_[0].handle());
                api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple_[0].layout(), 0, 2,
                                                 descr_sets, 0, nullptr);
                i = _draw_range_ext(api_ctx, cmd_buf, pi_simple_[0], batch_indices, batches, i, BDB::BitAlphaTest,
                                    materials_per_descriptor, bindless_tex_->textures_descr_sets, &draws_count);
                i = _skip_range(batch_indices, batches, i, BDB::BitAlphaTest | BDB::BitCustomShaded);
            }

            { // alpha-tested two-sided
                Ren::DebugMarker _mm(api_ctx, cmd_buf, "ALPHA-TWO-SIDED");
                api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple_[1].handle());
                api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple_[1].layout(), 0, 2,
                                                 descr_sets, 0, nullptr);
                i = _draw_range_ext(api_ctx, cmd_buf, pi_simple_[1], batch_indices, batches, i,
                                    BDB::BitAlphaTest | BDB::BitTwoSided, materials_per_descriptor,
                                    bindless_tex_->textures_descr_sets, &draws_count);
                i = _skip_range(batch_indices, batches, i, BDB::BitAlphaTest | BDB::BitTwoSided | BDB::BitCustomShaded);
            }

            { // moving alpha-tested one-sided
                Ren::DebugMarker _mm(api_ctx, cmd_buf, "MOVING-ALPHA-ONE-SIDED");
                api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple_[0].handle());
                api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple_[0].layout(), 0, 2,
                                                 descr_sets, 0, nullptr);
                i = _draw_range_ext(api_ctx, cmd_buf, pi_simple_[0], batch_indices, batches, i,
                                    BDB::BitMoving | BDB::BitAlphaTest, materials_per_descriptor,
                                    bindless_tex_->textures_descr_sets, &draws_count);
                i = _skip_range(batch_indices, batches, i, BDB::BitMoving | BDB::BitAlphaTest | BDB::BitCustomShaded);
            }

            { // moving alpha-tested two-sided
                Ren::DebugMarker _mm(api_ctx, cmd_buf, "MOVING-ALPHA-TWO-SIDED");
                api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple_[1].handle());
                api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple_[1].layout(), 0, 2,
                                                 descr_sets, 0, nullptr);
                i = _draw_range_ext(api_ctx, cmd_buf, pi_simple_[1], batch_indices, batches, i,
                                    BDB::BitMoving | BDB::BitAlphaTest | BDB::BitTwoSided, materials_per_descriptor,
                                    bindless_tex_->textures_descr_sets, &draws_count);
                i = _skip_range(batch_indices, batches, i,
                                BDB::BitMoving | BDB::BitAlphaTest | BDB::BitTwoSided | BDB::BitCustomShaded);
            }
        }

        { // Vegetation meshes
            Ren::DebugMarker _m(api_ctx, cmd_buf, "VEGETATION");

            vi_vegetation_.BindBuffers(api_ctx, cmd_buf, 0, VK_INDEX_TYPE_UINT32);

            { // vegetation solid one-sided
                Ren::DebugMarker _mm(api_ctx, cmd_buf, "VEGE-SOLID-ONE-SIDED");
                api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_vegetation_[0].handle());
                api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_vegetation_[0].layout(),
                                                 0, 2, descr_sets, 0, nullptr);
                i = _draw_range_ext(api_ctx, cmd_buf, pi_vegetation_[0], batch_indices, batches, i, BDB::BitsVege,
                                    materials_per_descriptor, bindless_tex_->textures_descr_sets, &draws_count);
                i = _skip_range(batch_indices, batches, i, BDB::BitsVege | BDB::BitCustomShaded);
            }

            { // vegetation solid two-sided
                Ren::DebugMarker _mm(api_ctx, cmd_buf, "SOLID-TWO-SIDED");
                api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_vegetation_[1].handle());
                api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_vegetation_[1].layout(),
                                                 0, 2, descr_sets, 0, nullptr);
                const uint32_t DrawMask = BDB::BitsVege | BDB::BitTwoSided;
                i = _draw_range_ext(api_ctx, cmd_buf, pi_vegetation_[1], batch_indices, batches, i, DrawMask,
                                    materials_per_descriptor, bindless_tex_->textures_descr_sets, &draws_count);
                i = _skip_range(batch_indices, batches, i, DrawMask | BDB::BitCustomShaded);
            }

            { // vegetation moving solid one-sided
                Ren::DebugMarker _mm(api_ctx, cmd_buf, "VEGE-MOVING-SOLID-ONE-SIDED");
                api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_vegetation_[0].handle());
                api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_vegetation_[0].layout(),
                                                 0, 2, descr_sets, 0, nullptr);
                const uint32_t DrawMask = BDB::BitsVege | BDB::BitMoving;
                i = _draw_range_ext(api_ctx, cmd_buf, pi_vegetation_[0], batch_indices, batches, i, DrawMask,
                                    materials_per_descriptor, bindless_tex_->textures_descr_sets, &draws_count);
                i = _skip_range(batch_indices, batches, i, DrawMask | BDB::BitCustomShaded);
            }

            { // vegetation moving solid two-sided
                Ren::DebugMarker _mm(api_ctx, cmd_buf, "VEGE-MOVING-SOLID-TWO-SIDED");
                api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_vegetation_[1].handle());
                api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_vegetation_[1].layout(),
                                                 0, 2, descr_sets, 0, nullptr);
                const uint32_t DrawMask = BDB::BitsVege | BDB::BitMoving | BDB::BitTwoSided;
                i = _draw_range_ext(api_ctx, cmd_buf, pi_vegetation_[1], batch_indices, batches, i, DrawMask,
                                    materials_per_descriptor, bindless_tex_->textures_descr_sets, &draws_count);
                i = _skip_range(batch_indices, batches, i, DrawMask | BDB::BitCustomShaded);
            }

            { // vegetation alpha-tested one-sided
                Ren::DebugMarker _mm(api_ctx, cmd_buf, "VEGE-ALPHA-ONE-SIDED");
                api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_vegetation_[0].handle());
                api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_vegetation_[0].layout(),
                                                 0, 2, descr_sets, 0, nullptr);
                const uint32_t DrawMask = BDB::BitsVege | BDB::BitAlphaTest;
                i = _draw_range_ext(api_ctx, cmd_buf, pi_vegetation_[0], batch_indices, batches, i, DrawMask,
                                    materials_per_descriptor, bindless_tex_->textures_descr_sets, &draws_count);
                i = _skip_range(batch_indices, batches, i, DrawMask | BDB::BitCustomShaded);
            }

            { // vegetation alpha-tested two-sided
                Ren::DebugMarker _mm(api_ctx, cmd_buf, "VEGE-ALPHA-TWO-SIDED");
                api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_vegetation_[1].handle());
                api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_vegetation_[1].layout(),
                                                 0, 2, descr_sets, 0, nullptr);
                const uint32_t DrawMask = BDB::BitsVege | BDB::BitAlphaTest | BDB::BitTwoSided;
                i = _draw_range_ext(api_ctx, cmd_buf, pi_vegetation_[1], batch_indices, batches, i, DrawMask,
                                    materials_per_descriptor, bindless_tex_->textures_descr_sets, &draws_count);
                i = _skip_range(batch_indices, batches, i, DrawMask | BDB::BitCustomShaded);
            }

            { // vegetation moving alpha-tested one-sided
                Ren::DebugMarker _mm(api_ctx, cmd_buf, "VEGE-MOVING-ALPHA-ONE-SIDED");
                api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_vegetation_[0].handle());
                api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_vegetation_[0].layout(),
                                                 0, 2, descr_sets, 0, nullptr);
                const uint32_t DrawMask = BDB::BitsVege | BDB::BitMoving | BDB::BitAlphaTest;
                i = _draw_range_ext(api_ctx, cmd_buf, pi_vegetation_[0], batch_indices, batches, i, DrawMask,
                                    materials_per_descriptor, bindless_tex_->textures_descr_sets, &draws_count);
                i = _skip_range(batch_indices, batches, i, DrawMask | BDB::BitCustomShaded);
            }

            { // vegetation moving alpha-tested two-sided
                Ren::DebugMarker _mm(api_ctx, cmd_buf, "VEGE-MOVING-ALPHA-TWO-SIDED");
                api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_vegetation_[1].handle());
                api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_vegetation_[1].layout(),
                                                 0, 2, descr_sets, 0, nullptr);
                const uint32_t DrawMask = BDB::BitsVege | BDB::BitMoving | BDB::BitAlphaTest | BDB::BitTwoSided;
                i = _draw_range_ext(api_ctx, cmd_buf, pi_vegetation_[1], batch_indices, batches, i, DrawMask,
                                    materials_per_descriptor, bindless_tex_->textures_descr_sets, &draws_count);
                i = _skip_range(batch_indices, batches, i, DrawMask | BDB::BitCustomShaded);
            }
        }

        { // Skinned meshes
            Ren::DebugMarker _m(api_ctx, cmd_buf, "SKINNED");

            vi_simple_.BindBuffers(api_ctx, cmd_buf, 0, VK_INDEX_TYPE_UINT32);

            { // skinned solid one-sided
                Ren::DebugMarker _mm(api_ctx, cmd_buf, "SKIN-SOLID-ONE-SIDED");
                api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple_[0].handle());
                api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple_[0].layout(), 0, 2,
                                                 descr_sets, 0, nullptr);
                i = _draw_range_ext(api_ctx, cmd_buf, pi_simple_[0], batch_indices, batches, i, BDB::BitsSkinned,
                                    materials_per_descriptor, bindless_tex_->textures_descr_sets, &draws_count);
                i = _skip_range(batch_indices, batches, i, BDB::BitsSkinned | BDB::BitCustomShaded);
            }

            { // skinned solid two-sided
                Ren::DebugMarker _mm(api_ctx, cmd_buf, "SKIN-SOLID-TWO-SIDED");
                api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple_[1].handle());
                api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple_[1].layout(), 0, 2,
                                                 descr_sets, 0, nullptr);
                const uint32_t DrawMask = BDB::BitsSkinned | BDB::BitTwoSided;
                i = _draw_range_ext(api_ctx, cmd_buf, pi_simple_[1], batch_indices, batches, i, DrawMask,
                                    materials_per_descriptor, bindless_tex_->textures_descr_sets, &draws_count);
                i = _skip_range(batch_indices, batches, i, DrawMask | BDB::BitCustomShaded);
            }

            { // skinned moving solid one-sided
                Ren::DebugMarker _mm(api_ctx, cmd_buf, "SKIN-MOVING-SOLID-ONE-SIDED");
                api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple_[0].handle());
                api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple_[0].layout(), 0, 2,
                                                 descr_sets, 0, nullptr);
                const uint32_t DrawMask = BDB::BitsSkinned | BDB::BitMoving;
                i = _draw_range_ext(api_ctx, cmd_buf, pi_simple_[0], batch_indices, batches, i, DrawMask,
                                    materials_per_descriptor, bindless_tex_->textures_descr_sets, &draws_count);
                i = _skip_range(batch_indices, batches, i, DrawMask | BDB::BitCustomShaded);
            }

            { // skinned moving solid two-sided
                Ren::DebugMarker _mm(api_ctx, cmd_buf, "SKIN-MOVING-SOLID-TWO-SIDED");
                api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple_[1].handle());
                api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple_[1].layout(), 0, 2,
                                                 descr_sets, 0, nullptr);
                const uint32_t DrawMask = BDB::BitsSkinned | BDB::BitMoving | BDB::BitTwoSided;
                i = _draw_range_ext(api_ctx, cmd_buf, pi_simple_[1], batch_indices, batches, i, DrawMask,
                                    materials_per_descriptor, bindless_tex_->textures_descr_sets, &draws_count);
                i = _skip_range(batch_indices, batches, i, DrawMask | BDB::BitCustomShaded);
            }

            { // skinned alpha-tested one-sided
                Ren::DebugMarker _mm(api_ctx, cmd_buf, "SKIN-ALPHA-ONE-SIDED");
                api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple_[0].handle());
                api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple_[0].layout(), 0, 2,
                                                 descr_sets, 0, nullptr);
                const uint32_t DrawMask = BDB::BitsSkinned | BDB::BitAlphaTest;
                i = _draw_range_ext(api_ctx, cmd_buf, pi_simple_[0], batch_indices, batches, i, DrawMask,
                                    materials_per_descriptor, bindless_tex_->textures_descr_sets, &draws_count);
                i = _skip_range(batch_indices, batches, i, DrawMask | BDB::BitCustomShaded);
            }

            { // skinned alpha-tested two-sided
                Ren::DebugMarker _mm(api_ctx, cmd_buf, "SKIN-ALPHA-TWO-SIDED");
                api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple_[1].handle());
                api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple_[1].layout(), 0, 2,
                                                 descr_sets, 0, nullptr);
                const uint32_t DrawMask = BDB::BitsSkinned | BDB::BitAlphaTest | BDB::BitTwoSided;
                i = _draw_range_ext(api_ctx, cmd_buf, pi_simple_[1], batch_indices, batches, i, DrawMask,
                                    materials_per_descriptor, bindless_tex_->textures_descr_sets, &draws_count);
                i = _skip_range(batch_indices, batches, i, DrawMask | BDB::BitCustomShaded);
            }

            { // skinned moving alpha-tested one-sided
                Ren::DebugMarker _mm(api_ctx, cmd_buf, "SKIN-MOVING-ALPHA-ONE-SIDED");
                api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple_[0].handle());
                api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple_[0].layout(), 0, 2,
                                                 descr_sets, 0, nullptr);
                const uint32_t DrawMask = BDB::BitsSkinned | BDB::BitMoving | BDB::BitAlphaTest;
                i = _draw_range_ext(api_ctx, cmd_buf, pi_simple_[0], batch_indices, batches, i, DrawMask,
                                    materials_per_descriptor, bindless_tex_->textures_descr_sets, &draws_count);
                i = _skip_range(batch_indices, batches, i, DrawMask | BDB::BitCustomShaded);
            }

            { // skinned moving alpha-tested two-sided
                Ren::DebugMarker _mm(api_ctx, cmd_buf, "SKIN-MOVING-ALPHA-TWO-SIDED");
                api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple_[1].handle());
                api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple_[1].layout(), 0, 2,
                                                 descr_sets, 0, nullptr);
                const uint32_t DrawMask = BDB::BitsSkinned | BDB::BitMoving | BDB::BitAlphaTest | BDB::BitTwoSided;
                i = _draw_range_ext(api_ctx, cmd_buf, pi_simple_[1], batch_indices, batches, i, DrawMask,
                                    materials_per_descriptor, bindless_tex_->textures_descr_sets, &draws_count);
                i = _skip_range(batch_indices, batches, i, DrawMask | BDB::BitCustomShaded);
            }
        }

        api_ctx->vkCmdEndRenderPass(cmd_buf);
    }
}

void Eng::RpGBufferFill::InitDescrSetLayout() {
    VkDescriptorSetLayoutBinding bindings[] = {
        // textures (2)
        {BIND_DECAL_TEX, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
        {BIND_NOISE_TEX, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT},
        // texel buffers (4)
        {BIND_DECAL_BUF, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
        {BIND_CELLS_BUF, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
        {BIND_ITEMS_BUF, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
        {BIND_INST_BUF, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT},
        // uniform buffers (1)
        {BIND_UB_SHARED_DATA_BUF, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
         VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT},
        // storage buffers (2)
        {BIND_MATERIALS_BUF, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT},
        {BIND_INST_NDX_BUF, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT}};

    VkDescriptorSetLayoutCreateInfo layout_info = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layout_info.bindingCount = uint32_t(std::size(bindings));
    layout_info.pBindings = bindings;

    const VkResult res =
        api_ctx_->vkCreateDescriptorSetLayout(api_ctx_->device, &layout_info, nullptr, &descr_set_layout_);
    assert(res == VK_SUCCESS);
}

Eng::RpGBufferFill::~RpGBufferFill() {
    if (descr_set_layout_ != VK_NULL_HANDLE) {
        api_ctx_->vkDestroyDescriptorSetLayout(api_ctx_->device, descr_set_layout_, nullptr);
    }
}
