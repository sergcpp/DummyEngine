#include "RpDebugRT.h"

#include <Ren/Context.h>
#include <Ren/Texture.h>
#include <Ren/VKCtx.h>

#include "../../utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"
#include "../shaders/rt_debug_interface.h"

void Eng::RpDebugRT::Execute_HWRT(RpBuilder &builder) {
    RpAllocBuf &geo_data_buf = builder.GetReadBuffer(pass_data_->geo_data_buf);
    RpAllocBuf &materials_buf = builder.GetReadBuffer(pass_data_->materials_buf);
    RpAllocBuf &vtx_buf1 = builder.GetReadBuffer(pass_data_->vtx_buf1);
    RpAllocBuf &vtx_buf2 = builder.GetReadBuffer(pass_data_->vtx_buf2);
    RpAllocBuf &ndx_buf = builder.GetReadBuffer(pass_data_->ndx_buf);
    RpAllocBuf &lights_buf = builder.GetReadBuffer(pass_data_->lights_buf);
    RpAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(pass_data_->shared_data);
    RpAllocTex &env_tex = builder.GetReadTexture(pass_data_->env_tex);
    RpAllocTex &dummy_black = builder.GetReadTexture(pass_data_->dummy_black);
    RpAllocTex &shadowmap_tex = builder.GetReadTexture(pass_data_->shadowmap_tex);
    RpAllocTex &ltc_luts_tex = builder.GetReadTexture(pass_data_->ltc_luts_tex);
    RpAllocTex *lm_tex[5];
    for (int i = 0; i < 5; ++i) {
        if (pass_data_->lm_tex[i]) {
            lm_tex[i] = &builder.GetReadTexture(pass_data_->lm_tex[i]);
        } else {
            lm_tex[i] = &dummy_black;
        }
    }
    RpAllocBuf &cells_buf = builder.GetReadBuffer(pass_data_->cells_buf);
    RpAllocBuf &items_buf = builder.GetReadBuffer(pass_data_->items_buf);
    RpAllocTex *output_tex = &builder.GetWriteTexture(pass_data_->output_tex);

    Ren::Context &ctx = builder.ctx();
    Ren::ApiContext *api_ctx = ctx.api_ctx();

    const auto *acc_struct = static_cast<const Ren::AccStructureVK *>(tlas_to_debug_);

    VkCommandBuffer cmd_buf = api_ctx->draw_cmd_buf[api_ctx->backend_frame];

    VkDescriptorSetLayout descr_set_layout = pi_debug_hwrt_.prog()->descr_set_layouts()[0];
    Ren::DescrSizes descr_sizes;
    descr_sizes.img_sampler_count = 14;
    descr_sizes.store_img_count = 1;
    descr_sizes.ubuf_count = 1;
    descr_sizes.acc_count = 1;
    descr_sizes.sbuf_count = 6;
    VkDescriptorSet descr_sets[2];
    descr_sets[0] = ctx.default_descr_alloc()->Alloc(descr_sizes, descr_set_layout);
    descr_sets[1] = bindless_tex_->rt_textures_descr_set;

    { // update descriptor set
        const VkDescriptorBufferInfo ubuf_info = {unif_sh_data_buf.ref->vk_handle(), 0, VK_WHOLE_SIZE};
        const VkDescriptorImageInfo env_info = {env_tex.ref->handle().sampler, env_tex.ref->handle().views[0],
                                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}; // environment texture
        const VkDescriptorImageInfo shadowmap_tex_info = shadowmap_tex.ref->vk_desc_image_info();
        const VkDescriptorImageInfo ltc_luts_tex_info = ltc_luts_tex.ref->vk_desc_image_info();
        const VkDescriptorBufferInfo geo_data_info = {geo_data_buf.ref->vk_handle(), 0, VK_WHOLE_SIZE};
        const VkDescriptorBufferInfo mat_data_info = {materials_buf.ref->vk_handle(), 0, VK_WHOLE_SIZE};
        const VkDescriptorBufferInfo vtx_buf1_info = {vtx_buf1.ref->vk_handle(), 0, VK_WHOLE_SIZE};
        const VkDescriptorBufferInfo vtx_buf2_info = {vtx_buf2.ref->vk_handle(), 0, VK_WHOLE_SIZE};
        const VkDescriptorBufferInfo ndx_buf_info = {ndx_buf.ref->vk_handle(), 0, VK_WHOLE_SIZE};
        const VkDescriptorBufferInfo lights_buf_info = {lights_buf.ref->vk_handle(), 0, VK_WHOLE_SIZE};
        const VkBufferView cells_buf_view = cells_buf.tbos[0]->view();
        const VkBufferView items_buf_view = items_buf.tbos[0]->view();
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

        Ren::SmallVector<VkWriteDescriptorSet, 32> descr_writes;
        { // shared buf
            auto &descr_write = descr_writes.emplace_back();
            descr_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            descr_write.dstSet = descr_sets[0];
            descr_write.dstBinding = BIND_UB_SHARED_DATA_BUF;
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
        { // lights_buf
            auto &descr_write = descr_writes.emplace_back();
            descr_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            descr_write.dstSet = descr_sets[0];
            descr_write.dstBinding = RTDebug::LIGHTS_BUF_SLOT;
            descr_write.dstArrayElement = 0;
            descr_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descr_write.descriptorCount = 1;
            descr_write.pBufferInfo = &lights_buf_info;
        }
        { // cells_buf
            auto &descr_write = descr_writes.emplace_back();
            descr_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            descr_write.dstSet = descr_sets[0];
            descr_write.dstBinding = RTDebug::CELLS_BUF_SLOT;
            descr_write.dstArrayElement = 0;
            descr_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
            descr_write.descriptorCount = 1;
            descr_write.pTexelBufferView = &cells_buf_view;
        }
        { // items_buf
            auto &descr_write = descr_writes.emplace_back();
            descr_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            descr_write.dstSet = descr_sets[0];
            descr_write.dstBinding = RTDebug::ITEMS_BUF_SLOT;
            descr_write.dstArrayElement = 0;
            descr_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
            descr_write.descriptorCount = 1;
            descr_write.pTexelBufferView = &items_buf_view;
        }
        { // shadowmap_tex
            auto &descr_write = descr_writes.emplace_back();
            descr_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            descr_write.dstSet = descr_sets[0];
            descr_write.dstBinding = RTDebug::SHADOW_TEX_SLOT;
            descr_write.dstArrayElement = 0;
            descr_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descr_write.descriptorCount = 1;
            descr_write.pImageInfo = &shadowmap_tex_info;
        }
        { // LTC LUTs
            auto &descr_write = descr_writes.emplace_back();
            descr_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            descr_write.dstSet = descr_sets[0];
            descr_write.dstBinding = RTDebug::LTC_LUTS_TEX_SLOT;
            descr_write.dstArrayElement = 0;
            descr_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descr_write.descriptorCount = 1;
            descr_write.pImageInfo = &ltc_luts_tex_info;
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

        api_ctx->vkUpdateDescriptorSets(api_ctx->device, uint32_t(descr_writes.size()), descr_writes.cdata(), 0,
                                        nullptr);
    }

    api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pi_debug_hwrt_.handle());
    api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pi_debug_hwrt_.layout(), 0, 2,
                                     descr_sets, 0, nullptr);

    RTDebug::Params uniform_params;
    uniform_params.img_size[0] = view_state_->scr_res[0];
    uniform_params.img_size[1] = view_state_->scr_res[1];
    uniform_params.pixel_spread_angle = view_state_->pixel_spread_angle;

    api_ctx->vkCmdPushConstants(cmd_buf, pi_debug_hwrt_.layout(), VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, 0,
                                sizeof(uniform_params), &uniform_params);

    api_ctx->vkCmdTraceRaysKHR(cmd_buf, pi_debug_hwrt_.rgen_table(), pi_debug_hwrt_.miss_table(),
                               pi_debug_hwrt_.hit_table(), pi_debug_hwrt_.call_table(),
                               uint32_t(view_state_->scr_res[0]), uint32_t(view_state_->scr_res[1]), 1);
}

void Eng::RpDebugRT::Execute_SWRT(RpBuilder &builder) {
    RpAllocBuf &geo_data_buf = builder.GetReadBuffer(pass_data_->geo_data_buf);
    RpAllocBuf &materials_buf = builder.GetReadBuffer(pass_data_->materials_buf);
    RpAllocBuf &vtx_buf1 = builder.GetReadBuffer(pass_data_->vtx_buf1);
    RpAllocBuf &vtx_buf2 = builder.GetReadBuffer(pass_data_->vtx_buf2);
    RpAllocBuf &ndx_buf = builder.GetReadBuffer(pass_data_->ndx_buf);
    RpAllocBuf &lights_buf = builder.GetReadBuffer(pass_data_->lights_buf);
    RpAllocBuf &rt_blas_buf = builder.GetReadBuffer(pass_data_->swrt.rt_blas_buf);
    RpAllocBuf &rt_tlas_buf = builder.GetReadBuffer(pass_data_->swrt.rt_tlas_buf);
    RpAllocBuf &prim_ndx_buf = builder.GetReadBuffer(pass_data_->swrt.prim_ndx_buf);
    RpAllocBuf &meshes_buf = builder.GetReadBuffer(pass_data_->swrt.meshes_buf);
    RpAllocBuf &mesh_instances_buf = builder.GetReadBuffer(pass_data_->swrt.mesh_instances_buf);
    RpAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(pass_data_->shared_data);
    RpAllocTex &env_tex = builder.GetReadTexture(pass_data_->env_tex);
    RpAllocTex &dummy_black = builder.GetReadTexture(pass_data_->dummy_black);
    RpAllocTex &shadowmap_tex = builder.GetReadTexture(pass_data_->shadowmap_tex);
    RpAllocTex &ltc_luts_tex = builder.GetReadTexture(pass_data_->ltc_luts_tex);
    RpAllocTex *lm_tex[5];
    for (int i = 0; i < 5; ++i) {
        if (pass_data_->lm_tex[i]) {
            lm_tex[i] = &builder.GetReadTexture(pass_data_->lm_tex[i]);
        } else {
            lm_tex[i] = &dummy_black;
        }
    }
    RpAllocBuf &cells_buf = builder.GetReadBuffer(pass_data_->cells_buf);
    RpAllocBuf &items_buf = builder.GetReadBuffer(pass_data_->items_buf);
    RpAllocTex *output_tex = &builder.GetWriteTexture(pass_data_->output_tex);

    Ren::Context &ctx = builder.ctx();
    Ren::ApiContext *api_ctx = ctx.api_ctx();

    if (!vtx_buf1.tbos[0] || vtx_buf1.tbos[0]->params().size != vtx_buf1.ref->size()) {
        vtx_buf1.tbos[0] =
            ctx.CreateTexture1D("Vertex Buf 1 TBO", vtx_buf1.ref, Ren::eTexFormat::RawRGBA32F, 0, vtx_buf1.ref->size());
    }

    if (!vtx_buf2.tbos[0] || vtx_buf2.tbos[0]->params().size != vtx_buf2.ref->size()) {
        vtx_buf2.tbos[0] = ctx.CreateTexture1D("Vertex Buf 2 TBO", vtx_buf2.ref, Ren::eTexFormat::RawRGBA32UI, 0,
                                               vtx_buf2.ref->size());
    }

    if (!ndx_buf.tbos[0] || ndx_buf.tbos[0]->params().size != ndx_buf.ref->size()) {
        ndx_buf.tbos[0] =
            ctx.CreateTexture1D("Index Buf TBO", ndx_buf.ref, Ren::eTexFormat::RawR32UI, 0, ndx_buf.ref->size());
    }

    if (!prim_ndx_buf.tbos[0] || prim_ndx_buf.tbos[0]->params().size != prim_ndx_buf.ref->size()) {
        prim_ndx_buf.tbos[0] = ctx.CreateTexture1D("Prim Ndx TBO", prim_ndx_buf.ref, Ren::eTexFormat::RawR32UI, 0,
                                                   prim_ndx_buf.ref->size());
    }

    if (!rt_blas_buf.tbos[0] || rt_blas_buf.tbos[0]->params().size != rt_blas_buf.ref->size()) {
        rt_blas_buf.tbos[0] = ctx.CreateTexture1D("RT BLAS TBO", rt_blas_buf.ref, Ren::eTexFormat::RawRGBA32F, 0,
                                                  rt_blas_buf.ref->size());
    }

    if (!rt_tlas_buf.tbos[0] || rt_tlas_buf.tbos[0]->params().size != rt_tlas_buf.ref->size()) {
        rt_tlas_buf.tbos[0] = ctx.CreateTexture1D("RT TLAS TBO", rt_tlas_buf.ref, Ren::eTexFormat::RawRGBA32F, 0,
                                                  rt_tlas_buf.ref->size());
    }

    if (!mesh_instances_buf.tbos[0] || mesh_instances_buf.tbos[0]->params().size != mesh_instances_buf.ref->size()) {
        mesh_instances_buf.tbos[0] =
            ctx.CreateTexture1D("Mesh Instances TBO", mesh_instances_buf.ref, Ren::eTexFormat::RawRGBA32F, 0,
                                mesh_instances_buf.ref->size());
    }

    if (!meshes_buf.tbos[0] || meshes_buf.tbos[0]->params().size != meshes_buf.ref->size()) {
        meshes_buf.tbos[0] =
            ctx.CreateTexture1D("Meshes TBO", meshes_buf.ref, Ren::eTexFormat::RawRG32UI, 0, meshes_buf.ref->size());
    }

    const Ren::Binding bindings[] = {
        {Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, *unif_sh_data_buf.ref},
        {Ren::eBindTarget::SBuf, RTDebug::GEO_DATA_BUF_SLOT, *geo_data_buf.ref},
        {Ren::eBindTarget::SBuf, RTDebug::MATERIAL_BUF_SLOT, *materials_buf.ref},
        {Ren::eBindTarget::TBuf, RTDebug::VTX_BUF1_SLOT, *vtx_buf1.tbos[0]},
        {Ren::eBindTarget::TBuf, RTDebug::VTX_BUF2_SLOT, *vtx_buf2.tbos[0]},
        {Ren::eBindTarget::TBuf, RTDebug::NDX_BUF_SLOT, *ndx_buf.tbos[0]},
        {Ren::eBindTarget::TBuf, RTDebug::BLAS_BUF_SLOT, *rt_blas_buf.tbos[0]},
        {Ren::eBindTarget::TBuf, RTDebug::TLAS_BUF_SLOT, *rt_tlas_buf.tbos[0]},
        {Ren::eBindTarget::TBuf, RTDebug::PRIM_NDX_BUF_SLOT, *prim_ndx_buf.tbos[0]},
        {Ren::eBindTarget::TBuf, RTDebug::MESHES_BUF_SLOT, *meshes_buf.tbos[0]},
        {Ren::eBindTarget::TBuf, RTDebug::MESH_INSTANCES_BUF_SLOT, *mesh_instances_buf.tbos[0]},
        {Ren::eBindTarget::SBuf, RTDebug::LIGHTS_BUF_SLOT, *lights_buf.ref},
        {Ren::eBindTarget::Tex2D, RTDebug::LMAP_TEX_SLOTS, 0, *lm_tex[0]->ref},
        {Ren::eBindTarget::Tex2D, RTDebug::LMAP_TEX_SLOTS, 1, *lm_tex[1]->ref},
        {Ren::eBindTarget::Tex2D, RTDebug::LMAP_TEX_SLOTS, 2, *lm_tex[2]->ref},
        {Ren::eBindTarget::Tex2D, RTDebug::LMAP_TEX_SLOTS, 3, *lm_tex[3]->ref},
        {Ren::eBindTarget::Tex2D, RTDebug::LMAP_TEX_SLOTS, 4, *lm_tex[4]->ref},
        {Ren::eBindTarget::Tex2D, RTDebug::ENV_TEX_SLOT, *env_tex.ref},
        {Ren::eBindTarget::Tex2D, RTDebug::SHADOW_TEX_SLOT, *shadowmap_tex.ref},
        {Ren::eBindTarget::Tex2D, RTDebug::LTC_LUTS_TEX_SLOT, *ltc_luts_tex.ref},
        {Ren::eBindTarget::TBuf, RTDebug::CELLS_BUF_SLOT, *cells_buf.tbos[0]},
        {Ren::eBindTarget::TBuf, RTDebug::ITEMS_BUF_SLOT, *items_buf.tbos[0]},
        {Ren::eBindTarget::Image2D, RTDebug::OUT_IMG_SLOT, *output_tex->ref}};

    const auto grp_count =
        Ren::Vec3u{(view_state_->act_res[0] + RTDebug::LOCAL_GROUP_SIZE_X - 1u) / RTDebug::LOCAL_GROUP_SIZE_X,
                   (view_state_->act_res[1] + RTDebug::LOCAL_GROUP_SIZE_Y - 1u) / RTDebug::LOCAL_GROUP_SIZE_Y, 1u};

    RTDebug::Params uniform_params;
    uniform_params.img_size[0] = view_state_->act_res[0];
    uniform_params.img_size[1] = view_state_->act_res[1];
    uniform_params.pixel_spread_angle = view_state_->pixel_spread_angle;
    uniform_params.root_node = pass_data_->swrt.root_node;

    VkCommandBuffer cmd_buf = api_ctx->draw_cmd_buf[api_ctx->backend_frame];

    VkDescriptorSet descr_sets[2];
    descr_sets[0] = Ren::PrepareDescriptorSet(api_ctx, pi_debug_swrt_.prog()->descr_set_layouts()[0], bindings,
                                              ctx.default_descr_alloc(), ctx.log());
    descr_sets[1] = bindless_tex_->rt_inline_textures_descr_set;

    api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pi_debug_swrt_.handle());
    api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pi_debug_swrt_.layout(), 0, 2, descr_sets,
                                     0, nullptr);

    api_ctx->vkCmdPushConstants(cmd_buf, pi_debug_swrt_.layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0,
                                sizeof(uniform_params), &uniform_params);

    api_ctx->vkCmdDispatch(cmd_buf, grp_count[0], grp_count[1], grp_count[2]);
}