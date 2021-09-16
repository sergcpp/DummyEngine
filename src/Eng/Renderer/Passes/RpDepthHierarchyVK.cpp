#include "RpDepthHierarchy.h"

#include <Ren/Context.h>
#include <Ren/Program.h>

#include "../../Utils/ShaderLoader.h"
#include "../Renderer_Structs.h"

#include "../assets/shaders/internal/depth_hierarchy_interface.glsl"

void RpDepthHierarchy::Execute(RpBuilder &builder) {
    using namespace RpDepthHierarchyInternal;

    RpAllocTex &input_tex = builder.GetReadTexture(input_tex_);
    RpAllocTex &output_tex = builder.GetWriteTexture(output_tex_);

    LazyInit(builder.ctx(), builder.sh());

    Ren::Context &ctx = builder.ctx();
    Ren::ApiContext *api_ctx = ctx.api_ctx();

    if (output_tex.ref->handle().views.size() < 6) {
        // Initialize per-mip views
        VkImageViewCreateInfo view_info = {};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = output_tex.ref->handle().img;
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = Ren::VKFormatFromTexFormat(Ren::eTexFormat::RawR32F);
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;
        for (int i = 0; i < 6; ++i) {
            view_info.subresourceRange.baseMipLevel = i;
            VkImageView new_image_view = VK_NULL_HANDLE;
            const VkResult res = vkCreateImageView(api_ctx->device, &view_info, nullptr, &new_image_view);
            if (res != VK_SUCCESS) {
                ctx.log()->Error("Failed to create image view!");
            }
            output_tex.ref->handle().views.push_back(new_image_view);
        }
    }

    VkCommandBuffer cmd_buf = api_ctx->draw_cmd_buf[api_ctx->backend_frame];

    VkDescriptorSetLayout descr_set_layout = pi_depth_hierarchy_.prog()->descr_set_layouts()[0];
    VkDescriptorSet descr_set =
        ctx.default_descr_alloc()->Alloc(1 /* img_sampler_count */, 6 /* store_img_count */, 0 /* ubuf_count */,
                                         0 /* sbuf_count */, 0 /* tbuf_count */, descr_set_layout);

    { // update descriptor set
        const VkDescriptorImageInfo depth_tex_info = input_tex.ref->vk_desc_image_info(1);
        Ren::SmallVector<VkDescriptorImageInfo, 16> depth_img_infos;
        for (int i = 0; i < MipCount; ++i) {
            depth_img_infos.push_back(output_tex.ref->vk_desc_image_info(i + 1, VK_IMAGE_LAYOUT_GENERAL));
        }

        VkWriteDescriptorSet descr_writes[2] = {};
        descr_writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descr_writes[0].dstSet = descr_set;
        descr_writes[0].dstBinding = DepthHierarchy::DEPTH_TEX_SLOT;
        descr_writes[0].dstArrayElement = 0;
        descr_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descr_writes[0].descriptorCount = 1;
        descr_writes[0].pImageInfo = &depth_tex_info;

        descr_writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descr_writes[1].dstSet = descr_set;
        descr_writes[1].dstBinding = DepthHierarchy::DEPTH_IMG_SLOT;
        descr_writes[1].dstArrayElement = 0;
        descr_writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        descr_writes[1].descriptorCount = uint32_t(depth_img_infos.size());
        descr_writes[1].pImageInfo = depth_img_infos.cdata();

        vkUpdateDescriptorSets(api_ctx->device, 2, descr_writes, 0, nullptr);
    }

    vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pi_depth_hierarchy_.handle());
    vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pi_depth_hierarchy_.layout(), 0, 1, &descr_set, 0,
                            nullptr);

    DepthHierarchy::Params uniform_params;
    uniform_params.depth_size = Ren::Vec4i{view_state_->scr_res[0], view_state_->scr_res[1], 0, 0};
    uniform_params.clip_info = view_state_->clip_info;

    vkCmdPushConstants(cmd_buf, pi_depth_hierarchy_.layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uniform_params),
                       &uniform_params);

    vkCmdDispatch(
        cmd_buf,
        (output_tex.ref->params.w + DepthHierarchy::LOCAL_GROUP_SIZE_X - 1) / DepthHierarchy::LOCAL_GROUP_SIZE_X,
        (output_tex.ref->params.h + DepthHierarchy::LOCAL_GROUP_SIZE_Y - 1) / DepthHierarchy::LOCAL_GROUP_SIZE_Y, 1);
}
