#include "SceneManager.h"

#include <Ren/Context.h>
#include <Ren/DescriptorPool.h>
#include <Ren/Utils.h>
#include <Ren/VKCtx.h>
#include <Sys/ScopeExit.h>

#include "../renderer/Renderer_Structs.h"

#include <vtune/ittnotify.h>
extern __itt_domain *__g_itt_domain;

namespace SceneManagerConstants {} // namespace SceneManagerConstants

namespace SceneManagerInternal {
uint32_t next_power_of_two(uint32_t v) {
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}

VkDeviceSize align_up(const VkDeviceSize size, const VkDeviceSize alignment) {
    return (size + alignment - 1) & ~(alignment - 1);
}

void to_khr_xform(const Ren::Mat4f &xform, float matrix[3][4]) {
    // transpose
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 4; ++j) {
            matrix[i][j] = xform[j][i];
        }
    }
}

const VkDeviceSize AccStructAlignment = 256;
} // namespace SceneManagerInternal

bool Eng::SceneManager::UpdateMaterialsBuffer() {
    using namespace SceneManagerInternal;

    Ren::ApiContext *api_ctx = ren_ctx_.api_ctx();
    auto &pers_data = scene_data_.persistent_data;

    const uint32_t max_mat_count = scene_data_.materials.capacity();
    const uint32_t req_mat_buf_size = std::max(1u, max_mat_count) * sizeof(MaterialData);

    if (!pers_data.materials_buf) {
        pers_data.materials_buf = ren_ctx_.LoadBuffer("Materials Buffer", Ren::eBufType::Storage, req_mat_buf_size);
    }

    if (pers_data.materials_buf->size() < req_mat_buf_size) {
        pers_data.materials_buf->Resize(req_mat_buf_size);
    }

    const uint32_t max_tex_count = std::max(1u, MAX_TEX_PER_MATERIAL * max_mat_count);
    // const uint32_t req_tex_buf_size = max_tex_count * sizeof(GLuint64);

    if (!pers_data.textures_descr_pool) {
        pers_data.textures_descr_pool = std::make_unique<Ren::DescrPool>(api_ctx);
    }

    const int materials_per_descriptor = api_ctx->max_combined_image_samplers / MAX_TEX_PER_MATERIAL;

    if (pers_data.textures_descr_pool->descr_count(Ren::eDescrType::CombinedImageSampler) < max_tex_count) {
        assert(materials_per_descriptor > 0);
        const int needed_descriptors_count = (max_mat_count + materials_per_descriptor - 1) / materials_per_descriptor;

        Ren::DescrSizes descr_sizes;
        descr_sizes.img_sampler_count =
            Ren::MaxFramesInFlight * needed_descriptors_count * api_ctx->max_combined_image_samplers;
        pers_data.textures_descr_pool->Init(descr_sizes,
                                            Ren::MaxFramesInFlight * needed_descriptors_count /* sets_count */);

        if (ren_ctx_.capabilities.raytracing) {
            assert(needed_descriptors_count == 1); // we have to be able to bind all textures at once
            if (!pers_data.rt_textures_descr_pool) {
                pers_data.rt_textures_descr_pool = std::make_unique<Ren::DescrPool>(api_ctx);
            }
            pers_data.rt_textures_descr_pool->Init(descr_sizes, Ren::MaxFramesInFlight /* sets_count */);
        }

        if (ren_ctx_.capabilities.ray_query || ren_ctx_.capabilities.swrt) {
            if (!pers_data.rt_inline_textures_descr_pool) {
                pers_data.rt_inline_textures_descr_pool = std::make_unique<Ren::DescrPool>(api_ctx);
            }
            pers_data.rt_inline_textures_descr_pool->Init(descr_sizes, Ren::MaxFramesInFlight /* sets_count */);
        }

        if (!pers_data.textures_descr_layout) {
            VkDescriptorSetLayoutBinding textures_binding = {};
            textures_binding.binding = BIND_BINDLESS_TEX;
            textures_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            textures_binding.descriptorCount = api_ctx->max_combined_image_samplers;
            textures_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

            VkDescriptorSetLayoutCreateInfo layout_info = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
            layout_info.bindingCount = 1;
            layout_info.pBindings = &textures_binding;

            VkDescriptorBindingFlagsEXT bind_flag = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT;

            VkDescriptorSetLayoutBindingFlagsCreateInfoEXT extended_info = {
                VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT};
            extended_info.bindingCount = 1u;
            extended_info.pBindingFlags = &bind_flag;
            layout_info.pNext = &extended_info;

            const VkResult res = api_ctx->vkCreateDescriptorSetLayout(api_ctx->device, &layout_info, nullptr,
                                                                      &pers_data.textures_descr_layout);
            assert(res == VK_SUCCESS);
        }

        if (ren_ctx_.capabilities.raytracing && !pers_data.rt_textures_descr_layout) {
            VkDescriptorSetLayoutBinding textures_binding = {};
            textures_binding.binding = BIND_BINDLESS_TEX;
            textures_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            textures_binding.descriptorCount = api_ctx->max_combined_image_samplers;
            textures_binding.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR;

            VkDescriptorSetLayoutCreateInfo layout_info = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
            layout_info.bindingCount = 1;
            layout_info.pBindings = &textures_binding;

            VkDescriptorBindingFlagsEXT bind_flag = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT;

            VkDescriptorSetLayoutBindingFlagsCreateInfoEXT extended_info = {
                VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT};
            extended_info.bindingCount = 1u;
            extended_info.pBindingFlags = &bind_flag;
            layout_info.pNext = &extended_info;

            const VkResult res = api_ctx->vkCreateDescriptorSetLayout(api_ctx->device, &layout_info, nullptr,
                                                                      &pers_data.rt_textures_descr_layout);
            assert(res == VK_SUCCESS);
        }

        if ((ren_ctx_.capabilities.ray_query || ren_ctx_.capabilities.swrt) &&
            !pers_data.rt_inline_textures_descr_layout) {
            VkDescriptorSetLayoutBinding textures_binding = {};
            textures_binding.binding = BIND_BINDLESS_TEX;
            textures_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            textures_binding.descriptorCount = api_ctx->max_combined_image_samplers;
            textures_binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

            VkDescriptorSetLayoutCreateInfo layout_info = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
            layout_info.bindingCount = 1;
            layout_info.pBindings = &textures_binding;

            VkDescriptorBindingFlagsEXT bind_flag = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT;

            VkDescriptorSetLayoutBindingFlagsCreateInfoEXT extended_info = {
                VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT};
            extended_info.bindingCount = 1u;
            extended_info.pBindingFlags = &bind_flag;
            layout_info.pNext = &extended_info;

            const VkResult res = api_ctx->vkCreateDescriptorSetLayout(api_ctx->device, &layout_info, nullptr,
                                                                      &pers_data.rt_inline_textures_descr_layout);
            assert(res == VK_SUCCESS);
        }

        for (int j = 0; j < Ren::MaxFramesInFlight; ++j) {
            for (int k = 0; k < needed_descriptors_count; ++k) {
                pers_data.textures_descr_sets[j].push_back(
                    pers_data.textures_descr_pool->Alloc(pers_data.textures_descr_layout));
                assert(pers_data.textures_descr_sets[j].back());
            }
            if (ren_ctx_.capabilities.raytracing) {
                pers_data.rt_textures_descr_sets[j] =
                    pers_data.rt_textures_descr_pool->Alloc(pers_data.rt_textures_descr_layout);
            }
            if (ren_ctx_.capabilities.ray_query || ren_ctx_.capabilities.swrt) {
                pers_data.rt_inline_textures_descr_sets[j] =
                    pers_data.rt_inline_textures_descr_pool->Alloc(pers_data.rt_inline_textures_descr_layout);
            }
        }
    }

    for (const uint32_t i : scene_data_.material_changes) {
        for (int j = 0; j < Ren::MaxFramesInFlight; ++j) {
            scene_data_.mat_update_ranges[j].first = std::min(scene_data_.mat_update_ranges[j].first, i);
            scene_data_.mat_update_ranges[j].second = std::max(scene_data_.mat_update_ranges[j].second, i + 1);
        }
    }
    scene_data_.material_changes.clear();

    auto &update_range = scene_data_.mat_update_ranges[ren_ctx_.backend_frame()];
    if (update_range.second <= update_range.first) {
        bool finished = true;
        for (int j = 0; j < Ren::MaxFramesInFlight; ++j) {
            finished &= (scene_data_.mat_update_ranges[j].second <= scene_data_.mat_update_ranges[j].first);
        }
        return finished;
    }

    Ren::Buffer materials_upload_buf("Materials Upload Buffer", ren_ctx_.api_ctx(), Ren::eBufType::Upload,
                                     (update_range.second - update_range.first) * sizeof(MaterialData));
    MaterialData *material_data = reinterpret_cast<MaterialData *>(materials_upload_buf.Map());

    Ren::SmallVector<VkDescriptorImageInfo, 256> img_infos;
    Ren::SmallVector<Ren::TransitionInfo, 256> img_transitions;
    img_infos.reserve((update_range.second - update_range.first) * MAX_TEX_PER_MATERIAL);

    if (white_tex_->resource_state != Ren::eResState::ShaderResource) {
        img_transitions.emplace_back(white_tex_.get(), Ren::eResState::ShaderResource);
    }
    if (error_tex_->resource_state != Ren::eResState::ShaderResource) {
        img_transitions.emplace_back(error_tex_.get(), Ren::eResState::ShaderResource);
    }

    for (uint32_t i = update_range.first; i < update_range.second; ++i) {
        const uint32_t rel_i = i - update_range.first;

        const uint32_t set_index = i / materials_per_descriptor;
        const uint32_t arr_offset = i % materials_per_descriptor;

        const Ren::Material *mat = scene_data_.materials.GetOrNull(i);
        if (mat) {
            int j = 0;
            for (; j < int(mat->textures.size()); ++j) {
                material_data[rel_i].texture_indices[j] = arr_offset * MAX_TEX_PER_MATERIAL + j;

                if (mat->textures[j]->resource_state != Ren::eResState::ShaderResource) {
                    img_transitions.emplace_back(mat->textures[j].get(), Ren::eResState::ShaderResource);
                }

                auto &img_info = img_infos.emplace_back();
                img_info.sampler = mat->samplers[j]->vk_handle();
                img_info.imageView = mat->textures[j]->handle().views[0];
                img_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            }
            for (; j < MAX_TEX_PER_MATERIAL; ++j) {
                material_data[rel_i].texture_indices[j] = i * MAX_TEX_PER_MATERIAL + j;
                img_infos.push_back(white_tex_->vk_desc_image_info());
            }

            int k = 0;
            for (; k < mat->params.size(); ++k) {
                material_data[rel_i].params[k] = mat->params[k];
            }
            for (; k < MAX_MATERIAL_PARAMS; ++k) {
                material_data[rel_i].params[k] = Ren::Vec4f{0.0f};
            }
        } else {
            for (int j = 0; j < MAX_TEX_PER_MATERIAL; ++j) {
                material_data[rel_i].texture_indices[j] = i * MAX_TEX_PER_MATERIAL + j;
                img_infos.push_back(error_tex_->vk_desc_image_info());
            }
        }
    }

    if (!img_transitions.empty()) {
        Ren::TransitionResourceStates(ren_ctx_.api_ctx(), ren_ctx_.current_cmd_buf(), Ren::AllStages, Ren::AllStages,
                                      img_transitions);
    }

    if (!img_infos.empty()) {
        for (uint32_t i = update_range.first; i < update_range.second; ++i) {
            const uint32_t rel_i = i - update_range.first;

            const uint32_t set_index = i / materials_per_descriptor;
            const uint32_t arr_offset = i % materials_per_descriptor;
            const uint32_t arr_count = (materials_per_descriptor - arr_offset);

            VkWriteDescriptorSet descr_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            descr_write.dstSet = scene_data_.persistent_data.textures_descr_sets[ren_ctx_.backend_frame()][set_index];
            descr_write.dstBinding = BIND_BINDLESS_TEX;
            descr_write.dstArrayElement = uint32_t(arr_offset * MAX_TEX_PER_MATERIAL);
            descr_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descr_write.descriptorCount = uint32_t(MAX_TEX_PER_MATERIAL);
            descr_write.pBufferInfo = nullptr;
            descr_write.pImageInfo = img_infos.cdata() + rel_i * MAX_TEX_PER_MATERIAL;
            descr_write.pTexelBufferView = nullptr;
            descr_write.pNext = nullptr;

            // TODO: group this calls!!!
            api_ctx->vkUpdateDescriptorSets(api_ctx->device, 1, &descr_write, 0, nullptr);

            if (ren_ctx_.capabilities.raytracing) {
                descr_write.dstSet = scene_data_.persistent_data.rt_textures_descr_sets[ren_ctx_.backend_frame()];
                api_ctx->vkUpdateDescriptorSets(api_ctx->device, 1, &descr_write, 0, nullptr);
            }
            if (ren_ctx_.capabilities.ray_query || ren_ctx_.capabilities.swrt) {
                descr_write.dstSet =
                    scene_data_.persistent_data.rt_inline_textures_descr_sets[ren_ctx_.backend_frame()];
                api_ctx->vkUpdateDescriptorSets(api_ctx->device, 1, &descr_write, 0, nullptr);
            }
        }
    }

    materials_upload_buf.Unmap();
    scene_data_.persistent_data.materials_buf->UpdateSubRegion(
        update_range.first * sizeof(MaterialData), (update_range.second - update_range.first) * sizeof(MaterialData),
        materials_upload_buf, 0, ren_ctx_.current_cmd_buf());

    update_range = std::make_pair(std::numeric_limits<uint32_t>::max(), 0);

    return false;
}

std::unique_ptr<Ren::IAccStructure> Eng::SceneManager::Build_HWRT_BLAS(const AccStructure &acc) {
    using namespace SceneManagerInternal;

    Ren::ApiContext *api_ctx = ren_ctx_.api_ctx();

    const Ren::BufferRange &attribs = acc.mesh->attribs_buf1(), &indices = acc.mesh->indices_buf();

    VkAccelerationStructureGeometryTrianglesDataKHR tri_data = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR};
    tri_data.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    tri_data.vertexData.deviceAddress = attribs.buf->vk_device_address();
    tri_data.vertexStride = 16;
    tri_data.indexType = VK_INDEX_TYPE_UINT32;
    tri_data.indexData.deviceAddress = indices.buf->vk_device_address();
    tri_data.maxVertex = attribs.size / 16;

    //
    // Gather geometries
    //
    Ren::SmallVector<VkAccelerationStructureGeometryKHR, 16> geometries;
    Ren::SmallVector<VkAccelerationStructureBuildRangeInfoKHR, 16> build_ranges;
    Ren::SmallVector<uint32_t, 16> prim_counts;

    const uint32_t indices_start = indices.sub.offset;
    const Ren::Span<const Ren::TriGroup> groups = acc.mesh->groups();
    for (int j = 0; j < int(groups.size()); ++j) {
        const Ren::TriGroup &grp = groups[j];
        const Ren::Material *front_mat =
            (j >= acc.material_override.size()) ? grp.front_mat.get() : acc.material_override[j].first.get();
        const Ren::Material *back_mat =
            (j >= acc.material_override.size()) ? grp.back_mat.get() : acc.material_override[j].second.get();
        const Ren::Bitmask<Ren::eMatFlags> front_mat_flags = front_mat->flags();

        if (front_mat_flags & Ren::eMatFlags::AlphaBlend) {
            // Include only opaque surfaces
            continue;
        }

        auto &new_geo = geometries.emplace_back();
        new_geo = {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
        new_geo.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
        new_geo.flags = 0;
        if (!(front_mat_flags & Ren::eMatFlags::AlphaTest) && !(back_mat->flags() & Ren::eMatFlags::AlphaTest)) {
            new_geo.flags |= VK_GEOMETRY_OPAQUE_BIT_KHR;
        }
        new_geo.geometry.triangles = tri_data;

        auto &new_range = build_ranges.emplace_back();
        new_range.firstVertex = attribs.sub.offset / 16;
        new_range.primitiveCount = grp.num_indices / 3;
        new_range.primitiveOffset = indices_start + grp.offset;
        new_range.transformOffset = 0;

        prim_counts.push_back(grp.num_indices / 3);
    }

    //
    // Query needed memory
    //
    VkAccelerationStructureBuildGeometryInfoKHR build_info = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
    build_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    build_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    build_info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
                       VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR;
    build_info.geometryCount = uint32_t(geometries.size());
    build_info.pGeometries = geometries.cdata();

    VkAccelerationStructureBuildSizesInfoKHR size_info = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
    api_ctx->vkGetAccelerationStructureBuildSizesKHR(api_ctx->device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                                     &build_info, prim_counts.cdata(), &size_info);

    // make sure we will not use this potentially stale pointer
    build_info.pGeometries = nullptr;

    const uint32_t needed_build_scratch_size = uint32_t(size_info.buildScratchSize);
    const uint32_t needed_total_acc_struct_size =
        uint32_t(align_up(size_info.accelerationStructureSize, AccStructAlignment));

    Ren::Buffer scratch_buf =
        Ren::Buffer("BLAS Scratch Buf", api_ctx, Ren::eBufType::Storage, needed_build_scratch_size);
    VkDeviceAddress scratch_addr = scratch_buf.vk_device_address();

    Ren::Buffer acc_structs_buf("BLAS Before-Compaction Buf", api_ctx, Ren::eBufType::AccStructure,
                                needed_total_acc_struct_size);

    VkQueryPoolCreateInfo query_pool_create_info = {VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
    query_pool_create_info.queryCount = 1;
    query_pool_create_info.queryType = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR;

    VkQueryPool query_pool = {};
    VkResult res = api_ctx->vkCreateQueryPool(api_ctx->device, &query_pool_create_info, nullptr, &query_pool);
    if (res != VK_SUCCESS) {
        ren_ctx_.log()->Error("Failed to create query pool!");
        return nullptr;
    }
    SCOPE_EXIT({ api_ctx->vkDestroyQueryPool(api_ctx->device, query_pool, nullptr); })

    VkAccelerationStructureKHR blas_before_compaction = {};

    { // Submit build commands
        VkAccelerationStructureCreateInfoKHR acc_create_info = {
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
        acc_create_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        acc_create_info.buffer = acc_structs_buf.vk_handle();
        acc_create_info.offset = 0;
        acc_create_info.size = size_info.accelerationStructureSize;

        VkResult res = api_ctx->vkCreateAccelerationStructureKHR(api_ctx->device, &acc_create_info, nullptr,
                                                                 &blas_before_compaction);
        if (res != VK_SUCCESS) {
            ren_ctx_.log()->Error("Failed to create acceleration structure!");
            return nullptr;
        }

        build_info.pGeometries = geometries.cdata();

        build_info.dstAccelerationStructure = blas_before_compaction;
        build_info.scratchData.deviceAddress = scratch_addr;

        VkCommandBuffer cmd_buf = api_ctx->BegSingleTimeCommands();

        api_ctx->vkCmdResetQueryPool(cmd_buf, query_pool, 0, 1);

        const VkAccelerationStructureBuildRangeInfoKHR *_build_ranges = build_ranges.cdata();
        api_ctx->vkCmdBuildAccelerationStructuresKHR(cmd_buf, 1, &build_info, &_build_ranges);

        { // Place barrier
            VkMemoryBarrier scr_buf_barrier = {VK_STRUCTURE_TYPE_MEMORY_BARRIER};
            scr_buf_barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
            scr_buf_barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;

            api_ctx->vkCmdPipelineBarrier(cmd_buf, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                                          VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1,
                                          &scr_buf_barrier, 0, nullptr, 0, nullptr);
        }

        api_ctx->vkCmdWriteAccelerationStructuresPropertiesKHR(cmd_buf, 1, &build_info.dstAccelerationStructure,
                                                               VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR,
                                                               query_pool, 0);

        api_ctx->EndSingleTimeCommands(cmd_buf);
    }

    VkDeviceSize compact_size = {};
    res = api_ctx->vkGetQueryPoolResults(api_ctx->device, query_pool, 0, 1, sizeof(VkDeviceSize), &compact_size,
                                         sizeof(VkDeviceSize), VK_QUERY_RESULT_WAIT_BIT);
    if (res != VK_SUCCESS) {
        ren_ctx_.log()->Error("Failed to query compacted structure size!");
        return nullptr;
    }

    Ren::FreelistAlloc::Allocation mem_alloc =
        scene_data_.persistent_data.hwrt.rt_blas_mem_alloc.Alloc(AccStructAlignment, uint32_t(compact_size));
    if (mem_alloc.offset == 0xffffffff) {
        // allocate one more buffer
        const uint32_t buf_size =
            std::max(next_power_of_two(uint32_t(compact_size)), Eng::RtBLASChunkSize);
        std::string buf_name =
            "RT BLAS Buffer #" + std::to_string(scene_data_.persistent_data.hwrt.rt_blas_buffers.size());
        scene_data_.persistent_data.hwrt.rt_blas_buffers.emplace_back(
            ren_ctx_.LoadBuffer(buf_name, Ren::eBufType::AccStructure, buf_size));
        const uint16_t pool_index = scene_data_.persistent_data.hwrt.rt_blas_mem_alloc.AddPool(buf_size);
        if (pool_index != scene_data_.persistent_data.hwrt.rt_blas_buffers.size() - 1) {
            ren_ctx_.log()->Error("Invalid pool index!");
            return nullptr;
        }
        // try to allocate again
        mem_alloc =
            scene_data_.persistent_data.hwrt.rt_blas_mem_alloc.Alloc(AccStructAlignment, uint32_t(compact_size));
        assert(mem_alloc.offset != 0xffffffff);
    }

    std::unique_ptr<Ren::AccStructureVK> compacted_blas = std::make_unique<Ren::AccStructureVK>();

    { // Submit compaction commands
        VkCommandBuffer cmd_buf = api_ctx->BegSingleTimeCommands();

        VkAccelerationStructureCreateInfoKHR acc_create_info = {
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
        acc_create_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        acc_create_info.buffer = scene_data_.persistent_data.hwrt.rt_blas_buffers[mem_alloc.pool]->vk_handle();
        acc_create_info.offset = mem_alloc.offset;
        acc_create_info.size = compact_size;

        VkAccelerationStructureKHR compact_acc_struct;
        const VkResult res =
            api_ctx->vkCreateAccelerationStructureKHR(api_ctx->device, &acc_create_info, nullptr, &compact_acc_struct);
        if (res != VK_SUCCESS) {
            ren_ctx_.log()->Error("Failed to create acceleration structure!");
        }

        VkCopyAccelerationStructureInfoKHR copy_info = {VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR};
        copy_info.mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR;
        copy_info.src = blas_before_compaction;
        copy_info.dst = compact_acc_struct;

        api_ctx->vkCmdCopyAccelerationStructureKHR(cmd_buf, &copy_info);

        compacted_blas->mem_alloc = mem_alloc;
        if (!compacted_blas->Init(api_ctx, compact_acc_struct)) {
            ren_ctx_.log()->Error("Blas compaction failed!");
            return nullptr;
        }

        api_ctx->EndSingleTimeCommands(cmd_buf);

        api_ctx->vkDestroyAccelerationStructureKHR(api_ctx->device, blas_before_compaction, nullptr);
        acc_structs_buf.FreeImmediate();
        scratch_buf.FreeImmediate();
    }

    return compacted_blas;
}

void Eng::SceneManager::Alloc_HWRT_TLAS() {
    using namespace SceneManagerInternal;

    Ren::ApiContext *api_ctx = ren_ctx_.api_ctx();

    const uint32_t max_instance_count = MAX_RT_OBJ_INSTANCES; // allocate for worst case

    VkAccelerationStructureGeometryInstancesDataKHR instances_data = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR};
    instances_data.data.deviceAddress = {};

    VkAccelerationStructureGeometryKHR tlas_geo = {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
    tlas_geo.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    tlas_geo.geometry.instances = instances_data;

    VkAccelerationStructureBuildGeometryInfoKHR tlas_build_info = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
    tlas_build_info.flags =
        VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_NV;
    tlas_build_info.geometryCount = 1;
    tlas_build_info.pGeometries = &tlas_geo;
    tlas_build_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    tlas_build_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    tlas_build_info.srcAccelerationStructure = VK_NULL_HANDLE;

    VkAccelerationStructureBuildSizesInfoKHR size_info = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
    api_ctx->vkGetAccelerationStructureBuildSizesKHR(api_ctx->device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                                     &tlas_build_info, &max_instance_count, &size_info);

    scene_data_.persistent_data.rt_tlas_buf =
        ren_ctx_.LoadBuffer("TLAS Buf", Ren::eBufType::AccStructure, uint32_t(size_info.accelerationStructureSize));
    scene_data_.persistent_data.rt_sh_tlas_buf = ren_ctx_.LoadBuffer("TLAS Shadow Buf", Ren::eBufType::AccStructure,
                                                                     uint32_t(size_info.accelerationStructureSize));

    Ren::BufferRef tlas_scratch_buf =
        ren_ctx_.LoadBuffer("TLAS Scratch Buf", Ren::eBufType::Storage, uint32_t(size_info.buildScratchSize));

    { // Main TLAS
        VkAccelerationStructureCreateInfoKHR create_info = {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
        create_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        create_info.buffer = scene_data_.persistent_data.rt_tlas_buf->vk_handle();
        create_info.offset = 0;
        create_info.size = size_info.accelerationStructureSize;

        VkAccelerationStructureKHR tlas_handle;
        VkResult res = api_ctx->vkCreateAccelerationStructureKHR(api_ctx->device, &create_info, nullptr, &tlas_handle);
        if (res != VK_SUCCESS) {
            ren_ctx_.log()->Error("Failed to create acceleration structure!");
        }

        std::unique_ptr<Ren::AccStructureVK> vk_tlas = std::make_unique<Ren::AccStructureVK>();
        if (!vk_tlas->Init(api_ctx, tlas_handle)) {
            ren_ctx_.log()->Error("Failed to init TLAS!");
        }
        scene_data_.persistent_data.rt_tlas = std::move(vk_tlas);
    }

    { // Shadow TLAS
        VkAccelerationStructureCreateInfoKHR create_info = {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
        create_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        create_info.buffer = scene_data_.persistent_data.rt_sh_tlas_buf->vk_handle();
        create_info.offset = 0;
        create_info.size = size_info.accelerationStructureSize;

        VkAccelerationStructureKHR tlas_handle;
        VkResult res = api_ctx->vkCreateAccelerationStructureKHR(api_ctx->device, &create_info, nullptr, &tlas_handle);
        if (res != VK_SUCCESS) {
            ren_ctx_.log()->Error("Failed to create acceleration structure!");
        }

        std::unique_ptr<Ren::AccStructureVK> vk_tlas = std::make_unique<Ren::AccStructureVK>();
        if (!vk_tlas->Init(api_ctx, tlas_handle)) {
            ren_ctx_.log()->Error("Failed to init TLAS!");
        }
        scene_data_.persistent_data.rt_sh_tlas = std::move(vk_tlas);
    }

    scene_data_.persistent_data.rt_tlas_build_scratch_size = uint32_t(size_info.buildScratchSize);
}
