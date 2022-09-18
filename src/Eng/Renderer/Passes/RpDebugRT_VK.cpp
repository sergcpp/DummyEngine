#include "RpDebugRT.h"

#include <Ren/Context.h>
#include <Ren/RastState.h>
#include <Ren/Texture.h>

#include "../../Utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

#include "../assets/shaders/internal/rt_debug_interface.glsl"

void RpDebugRT::Execute_HWRT(RpBuilder &builder) {
    RpAllocBuf &geo_data_buf = builder.GetReadBuffer(pass_data_->geo_data_buf);
    RpAllocBuf &materials_buf = builder.GetReadBuffer(pass_data_->materials_buf);
    RpAllocBuf &vtx_buf1 = builder.GetReadBuffer(pass_data_->vtx_buf1);
    RpAllocBuf &vtx_buf2 = builder.GetReadBuffer(pass_data_->vtx_buf2);
    RpAllocBuf &ndx_buf = builder.GetReadBuffer(pass_data_->ndx_buf);
    RpAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(pass_data_->shared_data);
    RpAllocTex &env_tex = builder.GetReadTexture(pass_data_->env_tex);
    RpAllocTex &dummy_black = builder.GetReadTexture(pass_data_->dummy_black);
    RpAllocTex *lm_tex[5];
    for (int i = 0; i < 5; ++i) {
        if (pass_data_->lm_tex[i]) {
            lm_tex[i] = &builder.GetReadTexture(pass_data_->lm_tex[i]);
        } else {
            lm_tex[i] = &dummy_black;
        }
    }
    RpAllocTex *output_tex = &builder.GetWriteTexture(pass_data_->output_tex);

    Ren::Context &ctx = builder.ctx();
    Ren::ApiContext *api_ctx = ctx.api_ctx();

    const auto *acc_struct = static_cast<const Ren::AccStructureVK *>(tlas_to_debug_);

    VkCommandBuffer cmd_buf = api_ctx->draw_cmd_buf[api_ctx->backend_frame];

    VkDescriptorSetLayout descr_set_layout = pi_debug_hwrt_.prog()->descr_set_layouts()[0];
    Ren::DescrSizes descr_sizes;
    descr_sizes.img_sampler_count = 5;
    descr_sizes.store_img_count = 1;
    descr_sizes.ubuf_count = 1;
    descr_sizes.acc_count = 1;
    descr_sizes.sbuf_count = 5;
    VkDescriptorSet descr_sets[2];
    descr_sets[0] = ctx.default_descr_alloc()->Alloc(descr_sizes, descr_set_layout);
    descr_sets[1] = bindless_tex_->rt_textures_descr_set;

    { // update descriptor set
        const VkDescriptorBufferInfo ubuf_info = {unif_sh_data_buf.ref->vk_handle(), 0, VK_WHOLE_SIZE};
        const VkDescriptorImageInfo env_info = {env_tex.ref->handle().sampler, env_tex.ref->handle().views[0],
                                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}; // environment texture
        const VkDescriptorBufferInfo geo_data_info = {geo_data_buf.ref->vk_handle(), 0, VK_WHOLE_SIZE};
        const VkDescriptorBufferInfo mat_data_info = {materials_buf.ref->vk_handle(), 0, VK_WHOLE_SIZE};
        const VkDescriptorBufferInfo vtx_buf1_info = {vtx_buf1.ref->vk_handle(), 0, VK_WHOLE_SIZE};
        const VkDescriptorBufferInfo vtx_buf2_info = {vtx_buf2.ref->vk_handle(), 0, VK_WHOLE_SIZE};
        const VkDescriptorBufferInfo ndx_buf_info = {ndx_buf.ref->vk_handle(), 0, VK_WHOLE_SIZE};
        const VkDescriptorImageInfo lm_infos[] = {
            lm_tex[0]->ref->vk_desc_image_info(), lm_tex[1]->ref->vk_desc_image_info(),
            lm_tex[2]->ref->vk_desc_image_info(), lm_tex[3]->ref->vk_desc_image_info(),
            lm_tex[4]->ref->vk_desc_image_info()};
        const VkAccelerationStructureKHR tlas = acc_struct->vk_handle();

        VkDescriptorImageInfo output_img_info;
        if (output_tex) {
            output_img_info = output_tex->ref->vk_desc_image_info(0, VK_IMAGE_LAYOUT_GENERAL);
        } else {
            output_img_info = ctx.backbuffer_ref()->vk_desc_image_info(0, VK_IMAGE_LAYOUT_GENERAL);
        }

        Ren::SmallVector<VkWriteDescriptorSet, 16> descr_writes;
        { // shared buf
            auto &descr_write = descr_writes.emplace_back();
            descr_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            descr_write.dstSet = descr_sets[0];
            descr_write.dstBinding = REN_UB_SHARED_DATA_LOC;
            descr_write.dstArrayElement = 0;
            descr_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descr_write.descriptorCount = 1;
            descr_write.pBufferInfo = &ubuf_info;
        }
        VkWriteDescriptorSetAccelerationStructureKHR desc_tlas_info = {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR};
        desc_tlas_info.pAccelerationStructures = &tlas;
        desc_tlas_info.accelerationStructureCount = 1;
        { // acceleration structure
            auto &descr_write = descr_writes.emplace_back();
            descr_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            descr_write.dstSet = descr_sets[0];
            descr_write.dstBinding = RTDebug::TLAS_SLOT;
            descr_write.dstArrayElement = 0;
            descr_write.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
            descr_write.descriptorCount = 1;
            descr_write.pNext = &desc_tlas_info;
        }
        { // env texture
            auto &descr_write = descr_writes.emplace_back();
            descr_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            descr_write.dstSet = descr_sets[0];
            descr_write.dstBinding = RTDebug::ENV_TEX_SLOT;
            descr_write.dstArrayElement = 0;
            descr_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descr_write.descriptorCount = 1;
            descr_write.pImageInfo = &env_info;
        }
        { // geometry data
            auto &descr_write = descr_writes.emplace_back();
            descr_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            descr_write.dstSet = descr_sets[0];
            descr_write.dstBinding = RTDebug::GEO_DATA_BUF_SLOT;
            descr_write.dstArrayElement = 0;
            descr_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descr_write.descriptorCount = 1;
            descr_write.pBufferInfo = &geo_data_info;
        }
        { // materials
            auto &descr_write = descr_writes.emplace_back();
            descr_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            descr_write.dstSet = descr_sets[0];
            descr_write.dstBinding = RTDebug::MATERIAL_BUF_SLOT;
            descr_write.dstArrayElement = 0;
            descr_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descr_write.descriptorCount = 1;
            descr_write.pBufferInfo = &mat_data_info;
        }
        { // vtx_buf1
            auto &descr_write = descr_writes.emplace_back();
            descr_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            descr_write.dstSet = descr_sets[0];
            descr_write.dstBinding = RTDebug::VTX_BUF1_SLOT;
            descr_write.dstArrayElement = 0;
            descr_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descr_write.descriptorCount = 1;
            descr_write.pBufferInfo = &vtx_buf1_info;
        }
        { // vtx_buf1
            auto &descr_write = descr_writes.emplace_back();
            descr_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            descr_write.dstSet = descr_sets[0];
            descr_write.dstBinding = RTDebug::VTX_BUF2_SLOT;
            descr_write.dstArrayElement = 0;
            descr_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descr_write.descriptorCount = 1;
            descr_write.pBufferInfo = &vtx_buf2_info;
        }
        { // ndx_buf
            auto &descr_write = descr_writes.emplace_back();
            descr_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            descr_write.dstSet = descr_sets[0];
            descr_write.dstBinding = RTDebug::NDX_BUF_SLOT;
            descr_write.dstArrayElement = 0;
            descr_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descr_write.descriptorCount = 1;
            descr_write.pBufferInfo = &ndx_buf_info;
        }
        { // lightmap
            auto &descr_write = descr_writes.emplace_back();
            descr_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            descr_write.dstSet = descr_sets[0];
            descr_write.dstBinding = RTDebug::LMAP_TEX_SLOTS;
            descr_write.dstArrayElement = 0;
            descr_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descr_write.descriptorCount = 5;
            descr_write.pImageInfo = lm_infos;
        }
        { // output image
            auto &descr_write = descr_writes.emplace_back();
            descr_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            descr_write.dstSet = descr_sets[0];
            descr_write.dstBinding = RTDebug::OUT_IMG_SLOT;
            descr_write.dstArrayElement = 0;
            descr_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            descr_write.descriptorCount = 1;
            descr_write.pImageInfo = &output_img_info;
        }

        vkUpdateDescriptorSets(api_ctx->device, uint32_t(descr_writes.size()), descr_writes.cdata(), 0, nullptr);
    }

    vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pi_debug_hwrt_.handle());
    vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pi_debug_hwrt_.layout(), 0, 2, descr_sets,
                            0, nullptr);

    RTDebug::Params uniform_params;
    uniform_params.img_size[0] = view_state_->scr_res[0];
    uniform_params.img_size[1] = view_state_->scr_res[1];
    uniform_params.pixel_spread_angle = std::atan(
        2.0f * std::tan(0.5f * view_state_->vertical_fov * Ren::Pi<float>() / 180.0f) / float(view_state_->scr_res[1]));

    vkCmdPushConstants(cmd_buf, pi_debug_hwrt_.layout(), VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, 0, sizeof(uniform_params),
                       &uniform_params);

    vkCmdTraceRaysKHR(cmd_buf, pi_debug_hwrt_.rgen_table(), pi_debug_hwrt_.miss_table(), pi_debug_hwrt_.hit_table(),
                      pi_debug_hwrt_.call_table(), uint32_t(view_state_->scr_res[0]), uint32_t(view_state_->scr_res[1]),
                      1);
}

void RpDebugRT::Execute_SWRT(RpBuilder &builder) {
    RpAllocBuf &geo_data_buf = builder.GetReadBuffer(pass_data_->geo_data_buf);
    RpAllocBuf &materials_buf = builder.GetReadBuffer(pass_data_->materials_buf);
    RpAllocBuf &vtx_buf1 = builder.GetReadBuffer(pass_data_->vtx_buf1);
    RpAllocBuf &vtx_buf2 = builder.GetReadBuffer(pass_data_->vtx_buf2);
    RpAllocBuf &ndx_buf = builder.GetReadBuffer(pass_data_->ndx_buf);
    RpAllocBuf &rt_blas_buf = builder.GetReadBuffer(pass_data_->swrt.rt_blas_buf);
    RpAllocBuf &rt_tlas_buf = builder.GetReadBuffer(pass_data_->swrt.rt_tlas_buf);
    RpAllocBuf &prim_ndx_buf = builder.GetReadBuffer(pass_data_->swrt.prim_ndx_buf);
    RpAllocBuf &meshes_buf = builder.GetReadBuffer(pass_data_->swrt.meshes_buf);
    RpAllocBuf &mesh_instances_buf = builder.GetReadBuffer(pass_data_->swrt.mesh_instances_buf);
    RpAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(pass_data_->shared_data);
    RpAllocTex &env_tex = builder.GetReadTexture(pass_data_->env_tex);
    RpAllocTex &dummy_black = builder.GetReadTexture(pass_data_->dummy_black);
    RpAllocTex *lm_tex[5];
    for (int i = 0; i < 5; ++i) {
        if (pass_data_->lm_tex[i]) {
            lm_tex[i] = &builder.GetReadTexture(pass_data_->lm_tex[i]);
        } else {
            lm_tex[i] = &dummy_black;
        }
    }
    RpAllocTex *output_tex = &builder.GetWriteTexture(pass_data_->output_tex);

    Ren::Context &ctx = builder.ctx();
    Ren::ApiContext *api_ctx = ctx.api_ctx();

    const Ren::Binding bindings[] = {
        {Ren::eBindTarget::UBuf, REN_UB_SHARED_DATA_LOC, *unif_sh_data_buf.ref},
        {Ren::eBindTarget::SBuf, RTDebug::GEO_DATA_BUF_SLOT, *geo_data_buf.ref},
        {Ren::eBindTarget::SBuf, RTDebug::MATERIAL_BUF_SLOT, *materials_buf.ref},
        {Ren::eBindTarget::SBuf, RTDebug::VTX_BUF1_SLOT, *vtx_buf1.ref},
        {Ren::eBindTarget::SBuf, RTDebug::VTX_BUF2_SLOT, *vtx_buf2.ref},
        {Ren::eBindTarget::SBuf, RTDebug::NDX_BUF_SLOT, *ndx_buf.ref},
        {Ren::eBindTarget::SBuf, RTDebug::BLAS_BUF_SLOT, *rt_blas_buf.ref},
        {Ren::eBindTarget::SBuf, RTDebug::TLAS_BUF_SLOT, *rt_tlas_buf.ref},
        {Ren::eBindTarget::SBuf, RTDebug::PRIM_NDX_BUF_SLOT, *prim_ndx_buf.ref},
        {Ren::eBindTarget::SBuf, RTDebug::MESHES_BUF_SLOT, *meshes_buf.ref},
        {Ren::eBindTarget::SBuf, RTDebug::MESH_INSTANCES_BUF_SLOT, *mesh_instances_buf.ref},
        {Ren::eBindTarget::Tex2D, RTDebug::LMAP_TEX_SLOTS, 0, *lm_tex[0]->ref},
        {Ren::eBindTarget::Tex2D, RTDebug::LMAP_TEX_SLOTS, 1, *lm_tex[1]->ref},
        {Ren::eBindTarget::Tex2D, RTDebug::LMAP_TEX_SLOTS, 2, *lm_tex[2]->ref},
        {Ren::eBindTarget::Tex2D, RTDebug::LMAP_TEX_SLOTS, 3, *lm_tex[3]->ref},
        {Ren::eBindTarget::Tex2D, RTDebug::LMAP_TEX_SLOTS, 4, *lm_tex[4]->ref},
        {Ren::eBindTarget::Tex2D, RTDebug::ENV_TEX_SLOT, *env_tex.ref},
        {Ren::eBindTarget::Image, RTDebug::OUT_IMG_SLOT, *output_tex->ref}};

    const auto grp_count =
        Ren::Vec3u{(view_state_->act_res[0] + RTDebug::LOCAL_GROUP_SIZE_X - 1u) / RTDebug::LOCAL_GROUP_SIZE_X,
                   (view_state_->act_res[1] + RTDebug::LOCAL_GROUP_SIZE_Y - 1u) / RTDebug::LOCAL_GROUP_SIZE_Y, 1u};

    RTDebug::Params uniform_params;
    uniform_params.img_size[0] = view_state_->act_res[0];
    uniform_params.img_size[1] = view_state_->act_res[1];
    uniform_params.pixel_spread_angle = std::atan(
        2.0f * std::tan(0.5f * view_state_->vertical_fov * Ren::Pi<float>() / 180.0f) / float(view_state_->act_res[1]));
    uniform_params.root_node = pass_data_->swrt.root_node;

    VkCommandBuffer cmd_buf = api_ctx->draw_cmd_buf[api_ctx->backend_frame];

    VkDescriptorSet descr_sets[2];
    descr_sets[0] = Ren::PrepareDescriptorSet(api_ctx, pi_debug_swrt_.prog()->descr_set_layouts()[0], bindings,
                                              ctx.default_descr_alloc(), ctx.log());
    descr_sets[1] = bindless_tex_->rt_inline_textures_descr_set;

    vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pi_debug_swrt_.handle());
    vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pi_debug_swrt_.layout(), 0, 2, descr_sets, 0,
                            nullptr);

    vkCmdPushConstants(cmd_buf, pi_debug_swrt_.layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uniform_params),
                       &uniform_params);

    vkCmdDispatch(cmd_buf, grp_count[0], grp_count[1], grp_count[2]);
}