#include "SceneManager.h"

#include <Ren/Context.h>
#include <Ren/DescriptorPool.h>
#include <Ren/Utils.h>
#include <Ren/VKCtx.h>

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

    Ren::Buffer materials_stage_buf("Materials Stage Buffer", ren_ctx_.api_ctx(), Ren::eBufType::Stage,
                                    (update_range.second - update_range.first) * sizeof(MaterialData));
    MaterialData *material_data = reinterpret_cast<MaterialData *>(materials_stage_buf.Map(Ren::eBufMap::Write));

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

    materials_stage_buf.FlushMappedRange(
        0, materials_stage_buf.AlignMapOffset((update_range.second - update_range.first) * sizeof(MaterialData)));
    materials_stage_buf.Unmap();
    scene_data_.persistent_data.materials_buf->UpdateSubRegion(
        update_range.first * sizeof(MaterialData), (update_range.second - update_range.first) * sizeof(MaterialData),
        materials_stage_buf, 0, ren_ctx_.current_cmd_buf());

    update_range = std::make_pair(std::numeric_limits<uint32_t>::max(), 0);

    return false;
}

void Eng::SceneManager::InitHWRTAccStructures() {
    using namespace SceneManagerInternal;

    const VkDeviceSize AccStructAlignment = 256;

    Ren::ApiContext *api_ctx = ren_ctx_.api_ctx();

    struct Blas {
        Ren::SmallVector<VkAccelerationStructureGeometryKHR, 16> geometries;
        Ren::SmallVector<VkAccelerationStructureBuildRangeInfoKHR, 16> build_ranges;
        Ren::SmallVector<uint32_t, 16> prim_counts;
        VkAccelerationStructureBuildSizesInfoKHR size_info;
        VkAccelerationStructureBuildGeometryInfoKHR build_info;
        AccStructure *acc;
    };
    std::vector<Blas> all_blases;

    uint32_t needed_build_scratch_size = 0;
    uint32_t needed_total_acc_struct_size = 0;

    uint32_t acc_index = scene_data_.comp_store[CompAccStructure]->First();
    while (acc_index != 0xffffffff) {
        auto *acc = (AccStructure *)scene_data_.comp_store[CompAccStructure]->Get(acc_index);
        if (acc->mesh->blas) {
            // already processed
            acc_index = scene_data_.comp_store[CompAccStructure]->Next(acc_index);
            continue;
        }

        const Ren::BufferRange &attribs = acc->mesh->attribs_buf1();
        const Ren::BufferRange &indices = acc->mesh->indices_buf();

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
        all_blases.emplace_back();
        Blas &new_blas = all_blases.back();

        const uint32_t indices_start = indices.sub.offset;
        for (const Ren::TriGroup &grp : acc->mesh->groups()) {
            const Ren::Material *mat = grp.mat.get();
            const uint32_t mat_flags = mat->flags();

            if ((mat_flags & uint32_t(Ren::eMatFlags::AlphaBlend)) != 0) {
                // Include only opaque surfaces
                continue;
            }

            auto &new_geo = new_blas.geometries.emplace_back();
            new_geo = {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
            new_geo.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
            new_geo.flags = 0;
            if ((mat_flags & uint32_t(Ren::eMatFlags::AlphaTest)) == 0) {
                new_geo.flags |= VK_GEOMETRY_OPAQUE_BIT_KHR;
            }
            new_geo.geometry.triangles = tri_data;

            auto &new_range = new_blas.build_ranges.emplace_back();
            new_range.firstVertex = attribs.sub.offset / 16;
            new_range.primitiveCount = grp.num_indices / 3;
            new_range.primitiveOffset = indices_start + grp.offset;
            new_range.transformOffset = 0;

            new_blas.prim_counts.push_back(grp.num_indices / 3);
        }

        //
        // Query needed memory
        //
        new_blas.build_info = {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
        new_blas.build_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        new_blas.build_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        new_blas.build_info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
                                    VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR;
        new_blas.build_info.geometryCount = uint32_t(new_blas.geometries.size());
        new_blas.build_info.pGeometries = new_blas.geometries.cdata();

        new_blas.size_info = {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
        api_ctx->vkGetAccelerationStructureBuildSizesKHR(
            api_ctx->device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &new_blas.build_info,
            new_blas.prim_counts.cdata(), &new_blas.size_info);

        // make sure we will not use this potentially stale pointer
        new_blas.build_info.pGeometries = nullptr;

        needed_build_scratch_size = std::max(needed_build_scratch_size, uint32_t(new_blas.size_info.buildScratchSize));
        needed_total_acc_struct_size +=
            uint32_t(align_up(new_blas.size_info.accelerationStructureSize, AccStructAlignment));

        new_blas.acc = acc;
        acc->mesh->blas = std::make_unique<Ren::AccStructureVK>();

        acc_index = scene_data_.comp_store[CompAccStructure]->Next(acc_index);
    }

    if (!all_blases.empty()) {
        //
        // Allocate memory
        //
        Ren::Buffer scratch_buf("BLAS Scratch Buf", api_ctx, Ren::eBufType::Storage,
                                next_power_of_two(needed_build_scratch_size));
        VkDeviceAddress scratch_addr = scratch_buf.vk_device_address();

        Ren::Buffer acc_structs_buf("BLAS Before-Compaction Buf", api_ctx, Ren::eBufType::AccStructure,
                                    needed_total_acc_struct_size);

        //

        VkQueryPoolCreateInfo query_pool_create_info = {VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
        query_pool_create_info.queryCount = uint32_t(all_blases.size());
        query_pool_create_info.queryType = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR;

        VkQueryPool query_pool;
        VkResult res = api_ctx->vkCreateQueryPool(api_ctx->device, &query_pool_create_info, nullptr, &query_pool);
        assert(res == VK_SUCCESS);

        { // Submit build commands
            VkDeviceSize acc_buf_offset = 0;
            VkCommandBuffer cmd_buf = api_ctx->BegSingleTimeCommands();

            api_ctx->vkCmdResetQueryPool(cmd_buf, query_pool, 0, uint32_t(all_blases.size()));

            for (int i = 0; i < int(all_blases.size()); ++i) {
                VkAccelerationStructureCreateInfoKHR acc_create_info = {
                    VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
                acc_create_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
                acc_create_info.buffer = acc_structs_buf.vk_handle();
                acc_create_info.offset = acc_buf_offset;
                acc_create_info.size = all_blases[i].size_info.accelerationStructureSize;
                acc_buf_offset += align_up(acc_create_info.size, AccStructAlignment);

                VkAccelerationStructureKHR acc_struct;
                VkResult res =
                    api_ctx->vkCreateAccelerationStructureKHR(api_ctx->device, &acc_create_info, nullptr, &acc_struct);
                if (res != VK_SUCCESS) {
                    ren_ctx_.log()->Error(
                        "[SceneManager::InitHWAccStructures]: Failed to create acceleration structure!");
                }

                auto &vk_blas = static_cast<Ren::AccStructureVK &>(*all_blases[i].acc->mesh->blas);
                if (!vk_blas.Init(api_ctx, acc_struct)) {
                    ren_ctx_.log()->Error("[SceneManager::InitHWAccStructures]: Failed to init BLAS!");
                }

                all_blases[i].build_info.pGeometries = all_blases[i].geometries.cdata();

                all_blases[i].build_info.dstAccelerationStructure = acc_struct;
                all_blases[i].build_info.scratchData.deviceAddress = scratch_addr;

                const VkAccelerationStructureBuildRangeInfoKHR *build_ranges = all_blases[i].build_ranges.cdata();
                api_ctx->vkCmdBuildAccelerationStructuresKHR(cmd_buf, 1, &all_blases[i].build_info, &build_ranges);

                { // Place barrier
                    VkMemoryBarrier scr_buf_barrier = {VK_STRUCTURE_TYPE_MEMORY_BARRIER};
                    scr_buf_barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
                    scr_buf_barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;

                    api_ctx->vkCmdPipelineBarrier(cmd_buf, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                                                  VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1,
                                                  &scr_buf_barrier, 0, nullptr, 0, nullptr);
                }

                api_ctx->vkCmdWriteAccelerationStructuresPropertiesKHR(
                    cmd_buf, 1, &all_blases[i].build_info.dstAccelerationStructure,
                    VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR, query_pool, i);
            }

            api_ctx->EndSingleTimeCommands(cmd_buf);
        }

        std::vector<VkDeviceSize> compact_sizes(all_blases.size());
        res = api_ctx->vkGetQueryPoolResults(api_ctx->device, query_pool, 0, uint32_t(all_blases.size()),
                                             all_blases.size() * sizeof(VkDeviceSize), compact_sizes.data(),
                                             sizeof(VkDeviceSize), VK_QUERY_RESULT_WAIT_BIT);
        assert(res == VK_SUCCESS);

        api_ctx->vkDestroyQueryPool(api_ctx->device, query_pool, nullptr);

        VkDeviceSize total_compacted_size = 0;
        for (int i = 0; i < int(compact_sizes.size()); ++i) {
            total_compacted_size += align_up(compact_sizes[i], AccStructAlignment);
        }

        scene_data_.persistent_data.rt_blas_buf = ren_ctx_.LoadBuffer(
            "BLAS After-Compaction Buf", Ren::eBufType::AccStructure, uint32_t(total_compacted_size));

        { // Submit compaction commands
            VkDeviceSize compact_acc_buf_offset = 0;
            VkCommandBuffer cmd_buf = api_ctx->BegSingleTimeCommands();

            for (int i = 0; i < int(all_blases.size()); ++i) {
                VkAccelerationStructureCreateInfoKHR acc_create_info = {
                    VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
                acc_create_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
                acc_create_info.buffer = scene_data_.persistent_data.rt_blas_buf->vk_handle();
                acc_create_info.offset = compact_acc_buf_offset;
                acc_create_info.size = compact_sizes[i];
                assert(compact_acc_buf_offset + compact_sizes[i] <= total_compacted_size);
                compact_acc_buf_offset += align_up(acc_create_info.size, AccStructAlignment);

                VkAccelerationStructureKHR compact_acc_struct;
                const VkResult res = api_ctx->vkCreateAccelerationStructureKHR(api_ctx->device, &acc_create_info,
                                                                               nullptr, &compact_acc_struct);
                if (res != VK_SUCCESS) {
                    ren_ctx_.log()->Error(
                        "[SceneManager::InitHWAccStructures]: Failed to create acceleration structure!");
                }

                auto &vk_blas = static_cast<Ren::AccStructureVK &>(*all_blases[i].acc->mesh->blas);

                VkCopyAccelerationStructureInfoKHR copy_info = {VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR};
                copy_info.mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR;
                copy_info.src = vk_blas.vk_handle();
                copy_info.dst = compact_acc_struct;

                api_ctx->vkCmdCopyAccelerationStructureKHR(cmd_buf, &copy_info);

                if (!vk_blas.Init(api_ctx, compact_acc_struct)) {
                    ren_ctx_.log()->Error("[SceneManager::InitHWAccStructures]: Blas compaction failed!");
                }
            }

            api_ctx->EndSingleTimeCommands(cmd_buf);
        }
    }

    //
    // Build TLAS
    //

    // retrieve pointers to components for fast access
    const auto *transforms = (Transform *)scene_data_.comp_store[CompTransform]->SequentialData();
    const auto *acc_structs = (AccStructure *)scene_data_.comp_store[CompAccStructure]->SequentialData();
    const auto *lightmaps = (Lightmap *)scene_data_.comp_store[CompLightmap]->SequentialData();
    const auto *probes = (LightProbe *)scene_data_.comp_store[CompProbe]->SequentialData();
    const CompStorage *probe_store = scene_data_.comp_store[CompProbe];

    std::vector<RTGeoInstance> geo_instances;
    std::vector<VkAccelerationStructureInstanceKHR> tlas_instances;

    for (const auto &obj : scene_data_.objects) {
        if ((obj.comp_mask & (CompTransformBit | CompAccStructureBit)) != (CompTransformBit | CompAccStructureBit)) {
            continue;
        }

        const Transform &tr = transforms[obj.components[CompTransform]];
        const AccStructure &acc = acc_structs[obj.components[CompAccStructure]];
        const Lightmap *lm = nullptr;
        if (obj.comp_mask & CompLightmapBit) {
            lm = &lightmaps[obj.components[CompLightmap]];
        }
        uint32_t closest_probe = 0xffffffff;
        if (obj.comp_mask & CompProbeBit) {
            closest_probe = probes[obj.components[CompProbe]].layer_index;
        }

        auto &vk_blas = static_cast<Ren::AccStructureVK &>(*acc.mesh->blas);
        vk_blas.geo_index = uint32_t(geo_instances.size());
        vk_blas.geo_count = 0;

        tlas_instances.emplace_back();
        auto &new_instance = tlas_instances.back();
        to_khr_xform(tr.world_from_object, new_instance.transform.matrix);
        new_instance.instanceCustomIndex = vk_blas.geo_index;
        new_instance.mask = 0xff;
        new_instance.instanceShaderBindingTableRecordOffset = 0;
        new_instance.flags = 0;
        // VK_GEOMETRY_INSTANCE_TRIANGLE_FRONT_COUNTERCLOCKWISE_BIT_KHR; //
        // VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        new_instance.accelerationStructureReference = static_cast<uint64_t>(vk_blas.vk_device_address());

        const uint32_t indices_start = acc.mesh->indices_buf().sub.offset;
        for (const Ren::TriGroup &grp : acc.mesh->groups()) {
            const Ren::Material *mat = grp.mat.get();
            const uint32_t mat_flags = mat->flags();
            if ((mat_flags & uint32_t(Ren::eMatFlags::AlphaBlend)) != 0) {
                // Include only opaque surfaces
                continue;
            }

            ++vk_blas.geo_count;

            geo_instances.emplace_back();
            auto &geo = geo_instances.back();
            geo.indices_start = (indices_start + grp.offset) / sizeof(uint32_t);
            geo.vertices_start = acc.mesh->attribs_buf1().sub.offset / 16;
            geo.material_index = grp.mat.index();
            geo.flags = 0;
            if (lm) {
                geo.flags |= RTGeoLightmappedBit;
                memcpy(&geo.lmap_transform[0], ValuePtr(lm->xform), 4 * sizeof(float));
            } else {
                if (closest_probe == 0xffffffff) {
                    // find closest probe
                    float min_dist2 = std::numeric_limits<float>::max();
                    for (const auto &probe : scene_data_.objects) {
                        if ((probe.comp_mask & (CompTransformBit | CompProbeBit)) !=
                            (CompTransformBit | CompProbeBit)) {
                            continue;
                        }

                        const Transform &probe_tr = transforms[probe.components[CompTransform]];
                        const LightProbe &probe_pr = probes[probe.components[CompProbe]];

                        const float dist2 =
                            Distance2(0.5f * (tr.bbox_min_ws + tr.bbox_max_ws),
                                      0.5f * (probe_tr.bbox_min_ws + probe_tr.bbox_max_ws) + probe_pr.offset);
                        if (dist2 < min_dist2) {
                            closest_probe = probe_pr.layer_index;
                            min_dist2 = dist2;
                        }
                    }
                }
                geo.flags |= (closest_probe & 0xff);
            }
        }
    }

    if (geo_instances.empty()) {
        geo_instances.emplace_back();
        auto &dummy_geo = geo_instances.back();
        dummy_geo = {};

        tlas_instances.emplace_back();
        auto &dummy_instance = tlas_instances.back();
        dummy_instance = {};
    }

    scene_data_.persistent_data.rt_geo_data_buf = ren_ctx_.LoadBuffer(
        "RT Geo Data Buf", Ren::eBufType::Storage, uint32_t(geo_instances.size() * sizeof(RTGeoInstance)));
    Ren::Buffer geo_data_stage_buf("RT Geo Data Stage Buf", api_ctx, Ren::eBufType::Stage,
                                   uint32_t(geo_instances.size() * sizeof(RTGeoInstance)));

    {
        uint8_t *geo_data_stage = geo_data_stage_buf.Map(Ren::eBufMap::Write);
        memcpy(geo_data_stage, geo_instances.data(), geo_instances.size() * sizeof(RTGeoInstance));
        geo_data_stage_buf.Unmap();
    }

    scene_data_.persistent_data.rt_instance_buf =
        ren_ctx_.LoadBuffer("RT Instance Buf", Ren::eBufType::Storage,
                            uint32_t(MAX_RT_OBJ_INSTANCES * sizeof(VkAccelerationStructureInstanceKHR)));
    // Ren::Buffer instance_stage_buf("RT Instance Stage Buf", api_ctx, Ren::eBufType::Stage,
    //                                uint32_t(tlas_instances.size() * sizeof(VkAccelerationStructureInstanceKHR)));

    /*{
        uint8_t *instance_stage = instance_stage_buf.Map(Ren::eBufMap::Write);
        memcpy(instance_stage, tlas_instances.data(),
               tlas_instances.size() * sizeof(VkAccelerationStructureInstanceKHR));
        instance_stage_buf.Unmap();
    }*/

    VkDeviceAddress instance_buf_addr = scene_data_.persistent_data.rt_instance_buf->vk_device_address();

#if 1
    VkCommandBuffer cmd_buf = api_ctx->BegSingleTimeCommands();

    Ren::CopyBufferToBuffer(geo_data_stage_buf, 0, *scene_data_.persistent_data.rt_geo_data_buf, 0,
                            geo_data_stage_buf.size(), cmd_buf);

    { // Make sure compaction copying of BLASes has finished
        VkMemoryBarrier mem_barrier = {VK_STRUCTURE_TYPE_MEMORY_BARRIER};
        mem_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        mem_barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;

        api_ctx->vkCmdPipelineBarrier(cmd_buf, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                      VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &mem_barrier, 0,
                                      nullptr, 0, nullptr);
    }

    const uint32_t max_instance_count = MAX_RT_OBJ_INSTANCES; // allocate for worst case

    VkAccelerationStructureGeometryInstancesDataKHR instances_data = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR};
    instances_data.data.deviceAddress = instance_buf_addr;

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
            ren_ctx_.log()->Error("[SceneManager::InitHWAccStructures]: Failed to create acceleration structure!");
        }

        std::unique_ptr<Ren::AccStructureVK> vk_tlas(new Ren::AccStructureVK);
        if (!vk_tlas->Init(api_ctx, tlas_handle)) {
            ren_ctx_.log()->Error("[SceneManager::InitHWAccStructures]: Failed to init TLAS!");
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
            ren_ctx_.log()->Error("[SceneManager::InitHWAccStructures]: Failed to create acceleration structure!");
        }

        std::unique_ptr<Ren::AccStructureVK> vk_tlas(new Ren::AccStructureVK);
        if (!vk_tlas->Init(api_ctx, tlas_handle)) {
            ren_ctx_.log()->Error("[SceneManager::InitHWAccStructures]: Failed to init TLAS!");
        }
        scene_data_.persistent_data.rt_sh_tlas = std::move(vk_tlas);
    }

    scene_data_.persistent_data.rt_tlas_build_scratch_size = uint32_t(size_info.buildScratchSize);

    api_ctx->EndSingleTimeCommands(cmd_buf);
#else
    VkCommandBuffer cmd_buf = Ren::BegSingleTimeCommands(api_ctx->device, api_ctx->temp_command_pool);

    Ren::CopyBufferToBuffer(geo_data_stage_buf, 0, *scene_data_.persistent_data.rt_geo_data_buf, 0,
                            geo_data_stage_buf.size(), cmd_buf);

    Ren::CopyBufferToBuffer(instance_stage_buf, 0, *scene_data_.persistent_data.rt_instance_buf, 0,
                            instance_stage_buf.size(), cmd_buf);

    { // Make sure compaction copying of BLASes has finished
        VkMemoryBarrier mem_barrier = {VK_STRUCTURE_TYPE_MEMORY_BARRIER};
        mem_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        mem_barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;

        vkCmdPipelineBarrier(cmd_buf, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &mem_barrier, 0, nullptr, 0,
                             nullptr);
    }

    { //
        VkAccelerationStructureGeometryInstancesDataKHR instances_data = {
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR};
        instances_data.data.deviceAddress = instance_buf_addr;

        VkAccelerationStructureGeometryKHR tlas_geo = {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
        tlas_geo.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
        tlas_geo.geometry.instances = instances_data;

        VkAccelerationStructureBuildGeometryInfoKHR tlas_build_info = {
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
        tlas_build_info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
                                VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_NV;
        tlas_build_info.geometryCount = 1;
        tlas_build_info.pGeometries = &tlas_geo;
        tlas_build_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        tlas_build_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        tlas_build_info.srcAccelerationStructure = VK_NULL_HANDLE;

        const uint32_t instance_count = uint32_t(tlas_instances.size());
        const uint32_t max_instance_count = MAX_RT_OBJ_INSTANCES; // allocate for worst case

        VkAccelerationStructureBuildSizesInfoKHR size_info = {
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
        vkGetAccelerationStructureBuildSizesKHR(api_ctx->device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                                &tlas_build_info, &max_instance_count, &size_info);

        scene_data_.persistent_data.rt_tlas_buf =
            ren_ctx_.LoadBuffer("TLAS Buf", Ren::eBufType::AccStructure, uint32_t(size_info.accelerationStructureSize));

        VkAccelerationStructureCreateInfoKHR create_info = {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
        create_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        create_info.buffer = scene_data_.persistent_data.rt_tlas_buf->vk_handle();
        create_info.offset = 0;
        create_info.size = size_info.accelerationStructureSize;

        VkAccelerationStructureKHR tlas_handle;
        VkResult res = vkCreateAccelerationStructureKHR(api_ctx->device, &create_info, nullptr, &tlas_handle);
        if (res != VK_SUCCESS) {
            ren_ctx_.log()->Error("[SceneManager::InitHWAccStructures]: Failed to create acceleration structure!");
        }

        Ren::BufferRef tlas_scratch_buf =
            ren_ctx_.LoadBuffer("TLAS Scratch Buf", Ren::eBufType::Storage, uint32_t(size_info.buildScratchSize));
        VkDeviceAddress tlas_scratch_buf_addr = tlas_scratch_buf->vk_device_address();

        tlas_build_info.srcAccelerationStructure = VK_NULL_HANDLE;
        tlas_build_info.dstAccelerationStructure = tlas_handle;
        tlas_build_info.scratchData.deviceAddress = tlas_scratch_buf_addr;

        VkAccelerationStructureBuildRangeInfoKHR range_info = {};
        range_info.primitiveOffset = 0;
        range_info.primitiveCount = 0;
        // instance_count;
        range_info.firstVertex = 0;
        range_info.transformOffset = 0;

        const VkAccelerationStructureBuildRangeInfoKHR *build_range = &range_info;
        vkCmdBuildAccelerationStructuresKHR(cmd_buf, 1, &tlas_build_info, &build_range);

        std::unique_ptr<Ren::AccStructureVK> vk_tlas(new Ren::AccStructureVK);
        if (!vk_tlas->Init(api_ctx, tlas_handle)) {
            ren_ctx_.log()->Error("[SceneManager::InitHWAccStructures]: Failed to init TLAS!");
        }
        scene_data_.persistent_data.rt_tlas = std::move(vk_tlas);

        scene_data_.persistent_data.rt_tlas_build_scratch_size = uint32_t(size_info.buildScratchSize);
    }

    Ren::EndSingleTimeCommands(api_ctx->device, api_ctx->graphics_queue, cmd_buf, api_ctx->temp_command_pool);
#endif
}
