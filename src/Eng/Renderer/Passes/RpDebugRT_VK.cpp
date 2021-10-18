#include "RpDebugRT.h"

#include <Ren/Context.h>
#include <Ren/RastState.h>
#include <Ren/Texture.h>

#include "../../Utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

#include "../assets/shaders/internal/rt_debug_interface.glsl"

void RpDebugRT::Execute(RpBuilder &builder) {
    LazyInit(builder.ctx(), builder.sh());

    RpAllocBuf &geo_data_buf = builder.GetReadBuffer(geo_data_buf_);
    RpAllocBuf &materials_buf = builder.GetReadBuffer(materials_buf_);
    RpAllocBuf &vtx_buf1 = builder.GetReadBuffer(vtx_buf1_);
    RpAllocBuf &vtx_buf2 = builder.GetReadBuffer(vtx_buf2_);
    RpAllocBuf &ndx_buf = builder.GetReadBuffer(ndx_buf_);
    RpAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(shared_data_buf_);
    RpAllocTex &env_tex = builder.GetReadTexture(env_tex_);
    RpAllocTex &dummy_black = builder.GetReadTexture(dummy_black_);
    RpAllocTex *lm_tex[5];
    for (int i = 0; i < 5; ++i) {
        if (lm_tex_[i]) {
            lm_tex[i] = &builder.GetReadTexture(lm_tex_[i]);
        } else {
            lm_tex[i] = &dummy_black;
        }
    }

    RpAllocTex *output_tex = nullptr;
    if (output_tex_) {
        output_tex = &builder.GetWriteTexture(output_tex_);
    }

    Ren::Context &ctx = builder.ctx();
    Ren::ApiContext *api_ctx = ctx.api_ctx();

    auto *acc_struct = static_cast<Ren::AccStructureVK *>(acc_struct_data_->rt_tlas);

    VkCommandBuffer cmd_buf = api_ctx->draw_cmd_buf[api_ctx->backend_frame];

    VkDescriptorSetLayout descr_set_layout = pi_debug_rt_.prog()->descr_set_layouts()[0];
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

    vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pi_debug_rt_.handle());
    vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pi_debug_rt_.layout(), 0, 2, descr_sets, 0,
                            nullptr);

    RTDebug::Params uniform_params;
    uniform_params.pixel_spread_angle = std::atan(
        2.0f * std::tan(0.5f * view_state_->vertical_fov * Ren::Pi<float>() / 180.0f) / float(view_state_->scr_res[1]));

    vkCmdPushConstants(cmd_buf, pi_debug_rt_.layout(), VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, 0, sizeof(uniform_params),
                       &uniform_params);

    vkCmdTraceRaysKHR(cmd_buf, pi_debug_rt_.rgen_table(), pi_debug_rt_.miss_table(), pi_debug_rt_.hit_table(),
                      pi_debug_rt_.call_table(), uint32_t(view_state_->scr_res[0]), uint32_t(view_state_->scr_res[1]),
                      1);
}

void RpDebugRT::LazyInit(Ren::Context &ctx, ShaderLoader &sh) {
    if (!initialized) {
        Ren::ProgramRef debug_rt_prog =
            sh.LoadProgram(ctx, "rt_debug", "internal/rt_debug.rgen.glsl", "internal/rt_debug.rchit.glsl",
                           "internal/rt_debug.rahit.glsl", "internal/rt_debug.rmiss.glsl", nullptr);
        assert(debug_rt_prog->ready());

        if (!pi_debug_rt_.Init(ctx.api_ctx(), debug_rt_prog, ctx.log())) {
            ctx.log()->Error("RpDebugRT: Failed to initialize pipeline!");
        }

        initialized = true;
    }
}