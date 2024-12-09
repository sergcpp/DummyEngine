#include "ExSkinning.h"

#include <Ren/Buffer.h>
#include <Ren/Context.h>
#include <Ren/DescriptorPool.h>
#include <Ren/VKCtx.h>

#include "../Renderer_Structs.h"
#include "../shaders/skinning_interface.h"

void Eng::ExSkinning::Execute(FgBuilder &builder) {
    LazyInit(builder.ctx(), builder.sh());

    FgAllocBuf &skin_vtx_buf = builder.GetReadBuffer(skin_vtx_buf_);
    FgAllocBuf &skin_transforms_buf = builder.GetReadBuffer(skin_transforms_buf_);
    FgAllocBuf &shape_keys_buf = builder.GetReadBuffer(shape_keys_buf_);
    FgAllocBuf &delta_buf = builder.GetReadBuffer(delta_buf_);

    FgAllocBuf &vtx_buf1 = builder.GetWriteBuffer(vtx_buf1_);
    FgAllocBuf &vtx_buf2 = builder.GetWriteBuffer(vtx_buf2_);

    if (!p_list_->skin_regions.empty()) {
        Ren::Context &ctx = builder.ctx();
        Ren::ApiContext *api_ctx = ctx.api_ctx();
        VkCommandBuffer cmd_buf = api_ctx->draw_cmd_buf[api_ctx->backend_frame];

        VkDescriptorSetLayout descr_set_layout = pi_skinning_->prog()->descr_set_layouts()[0];
        Ren::DescrSizes descr_sizes;
        descr_sizes.sbuf_count = 6;
        VkDescriptorSet descr_set = ctx.default_descr_alloc()->Alloc(descr_sizes, descr_set_layout);

        { // update descriptor set
            const VkDescriptorBufferInfo buf_infos[6] = {
                {skin_vtx_buf.ref->vk_handle(), 0, VK_WHOLE_SIZE},        // input vertices binding
                {skin_transforms_buf.ref->vk_handle(), 0, VK_WHOLE_SIZE}, // input matrices binding
                {shape_keys_buf.ref->vk_handle(), 0, VK_WHOLE_SIZE},      // input shape keys binding
                {delta_buf.ref->vk_handle(), 0, VK_WHOLE_SIZE},           // input vertex deltas binding
                {vtx_buf1.ref->vk_handle(), 0, VK_WHOLE_SIZE},            // output vertices0 binding
                {vtx_buf2.ref->vk_handle(), 0, VK_WHOLE_SIZE}             // output vertices1 binding
            };

            VkWriteDescriptorSet descr_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            descr_write.dstSet = descr_set;
            descr_write.dstBinding = 0;
            descr_write.dstArrayElement = 0;
            descr_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descr_write.descriptorCount = 6;
            descr_write.pBufferInfo = buf_infos;

            api_ctx->vkUpdateDescriptorSets(api_ctx->device, 1, &descr_write, 0, nullptr);
        }

        api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pi_skinning_->handle());
        api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pi_skinning_->layout(), 0, 1,
                                         &descr_set, 0, nullptr);

        for (uint32_t i = 0; i < uint32_t(p_list_->skin_regions.size()); i++) {
            const SkinRegion &sr = p_list_->skin_regions[i];

            const uint32_t non_shapekeyed_vertex_count = sr.vertex_count - sr.shape_keyed_vertex_count;

            if (non_shapekeyed_vertex_count) {
                Skinning::Params uniform_params;
                uniform_params.uSkinParams =
                    Ren::Vec4u{sr.in_vtx_offset, non_shapekeyed_vertex_count, sr.xform_offset, sr.out_vtx_offset};
                uniform_params.uShapeParamsCurr = Ren::Vec4u{0, 0, 0, 0};
                uniform_params.uShapeParamsPrev = Ren::Vec4u{0, 0, 0, 0};

                api_ctx->vkCmdPushConstants(cmd_buf, pi_skinning_->layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0,
                                            sizeof(Skinning::Params), &uniform_params);

                api_ctx->vkCmdDispatch(
                    cmd_buf, (sr.vertex_count + Skinning::LOCAL_GROUP_SIZE - 1) / Skinning::LOCAL_GROUP_SIZE, 1, 1);
            }

            if (sr.shape_keyed_vertex_count) {
                Skinning::Params uniform_params;
                uniform_params.uSkinParams =
                    Ren::Vec4u{sr.in_vtx_offset + non_shapekeyed_vertex_count, sr.shape_keyed_vertex_count,
                               sr.xform_offset, sr.out_vtx_offset + non_shapekeyed_vertex_count};
                uniform_params.uShapeParamsCurr =
                    Ren::Vec4u{sr.shape_key_offset_curr, sr.shape_key_count_curr, sr.delta_offset, 0};
                uniform_params.uShapeParamsPrev =
                    Ren::Vec4u{sr.shape_key_offset_prev, sr.shape_key_count_prev, sr.delta_offset, 0};

                api_ctx->vkCmdPushConstants(cmd_buf, pi_skinning_->layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0,
                                            sizeof(Skinning::Params), &uniform_params);

                api_ctx->vkCmdDispatch(
                    cmd_buf,
                    (sr.shape_keyed_vertex_count + Skinning::LOCAL_GROUP_SIZE - 1) / Skinning::LOCAL_GROUP_SIZE, 1, 1);
            }
        }
    }
}
