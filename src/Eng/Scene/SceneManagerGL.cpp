#include "SceneManager.h"

#include <Ren/Context.h>
#include <Ren/GL.h>
#include <Ren/Utils.h>

#include "../Renderer/Renderer_Structs.h"

#include <vtune/ittnotify.h>
extern __itt_domain *__g_itt_domain;

namespace SceneManagerConstants {} // namespace SceneManagerConstants

namespace SceneManagerInternal {} // namespace SceneManagerInternal

void SceneManager::UpdateMaterialsBuffer() {
    const uint32_t max_mat_count = scene_data_.materials.capacity();
    const uint32_t req_mat_buf_size = std::max(1u, max_mat_count) * sizeof(MaterialData);

    if (!scene_data_.persistent_data.materials_buf) {
        scene_data_.persistent_data.materials_buf =
            ren_ctx_.LoadBuffer("Materials Buffer", Ren::eBufType::Storage, req_mat_buf_size);
    }

    if (scene_data_.persistent_data.materials_buf->size() < req_mat_buf_size) {
        scene_data_.persistent_data.materials_buf->Resize(req_mat_buf_size);
    }

    const uint32_t max_tex_count = std::max(1u, REN_MAX_TEX_PER_MATERIAL * max_mat_count);
    const uint32_t req_tex_buf_size = max_tex_count * sizeof(GLuint64);

    if (!scene_data_.persistent_data.textures_buf) {
        scene_data_.persistent_data.textures_buf =
            ren_ctx_.LoadBuffer("Textures Buffer", Ren::eBufType::Storage, req_tex_buf_size);
    }

    if (scene_data_.persistent_data.textures_buf->size() < req_tex_buf_size) {
        scene_data_.persistent_data.textures_buf->Resize(req_tex_buf_size);
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
        return;
    }

    const size_t TexSizePerMaterial = REN_MAX_TEX_PER_MATERIAL * sizeof(GLuint64);

    Ren::Buffer materials_stage_buf("Materials Stage Buffer", ren_ctx_.api_ctx(), Ren::eBufType::Stage,
                                    (update_range.second - update_range.first) * sizeof(MaterialData));
    Ren::Buffer textures_stage_buf;
    if (ren_ctx_.capabilities.bindless_texture) {
        textures_stage_buf = Ren::Buffer("Textures Stage Buffer", ren_ctx_.api_ctx(), Ren::eBufType::Stage,
                                         (update_range.second - update_range.first) * TexSizePerMaterial);
    }

    MaterialData *material_data = reinterpret_cast<MaterialData *>(materials_stage_buf.Map(Ren::BufMapWrite));
    GLuint64 *texture_data = nullptr;
    GLuint64 error_handle = 0;
    if (ren_ctx_.capabilities.bindless_texture) {
        texture_data = reinterpret_cast<GLuint64 *>(textures_stage_buf.Map(Ren::BufMapWrite));

        error_handle = glGetTextureHandleARB(error_tex_->id());
        if (!glIsTextureHandleResidentARB(error_handle)) {
            glMakeTextureHandleResidentARB(error_handle);
        }
    }

    for (uint32_t i = update_range.first; i < update_range.second; ++i) {
        const uint32_t rel_i = i - update_range.first;
        const Ren::Material *mat = scene_data_.materials.GetOrNull(i);
        if (mat) {
            int j = 0;
            for (; j < int(mat->textures.size()); ++j) {
                material_data[rel_i].texture_indices[j] = i * REN_MAX_TEX_PER_MATERIAL + j;
                if (texture_data) {
                    const GLuint64 handle =
                        glGetTextureSamplerHandleARB(mat->textures[j]->id(), mat->samplers[j]->id());
                    if (!glIsTextureHandleResidentARB(handle)) {
                        glMakeTextureHandleResidentARB(handle);
                    }
                    texture_data[rel_i * REN_MAX_TEX_PER_MATERIAL + j] = handle;
                }
            }
            for (; j < REN_MAX_TEX_PER_MATERIAL; ++j) {
                material_data[rel_i].texture_indices[j] = i * REN_MAX_TEX_PER_MATERIAL + j;
                if (texture_data) {
                    texture_data[rel_i * REN_MAX_TEX_PER_MATERIAL + j] = error_handle;
                }
            }
            if (!mat->params.empty()) {
                material_data[rel_i].params = mat->params[0];
            }
        } else {
            for (int j = 0; j < REN_MAX_TEX_PER_MATERIAL; ++j) {
                material_data[rel_i].texture_indices[j] = i * REN_MAX_TEX_PER_MATERIAL + j;
                if (texture_data) {
                    texture_data[rel_i * REN_MAX_TEX_PER_MATERIAL + j] = error_handle;
                }
            }
        }
    }

    if (texture_data) {
        textures_stage_buf.FlushMappedRange(0, (update_range.second - update_range.first) * TexSizePerMaterial);
        textures_stage_buf.Unmap();
        scene_data_.persistent_data.textures_buf->UpdateSubRegion(
            update_range.first * TexSizePerMaterial, (update_range.second - update_range.first) * TexSizePerMaterial,
            textures_stage_buf);
    }

    materials_stage_buf.FlushMappedRange(0, (update_range.second - update_range.first) * sizeof(MaterialData));
    materials_stage_buf.Unmap();
    scene_data_.persistent_data.materials_buf->UpdateSubRegion(
        update_range.first * sizeof(MaterialData), (update_range.second - update_range.first) * sizeof(MaterialData),
        materials_stage_buf);

    update_range = std::make_pair(std::numeric_limits<uint32_t>::max(), 0);
}

void SceneManager::InitHWAccStructures() {
    // stub
}