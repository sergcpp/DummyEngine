#include "SceneManager.h"

#include <Ren/Context.h>
#include <Ren/Gl/GL.h>
#include <Ren/utils/Utils.h>
#include <Sys/ScopeExit.h>

#include "../renderer/Renderer_Structs.h"

#include <vtune/ittnotify.h>
extern __itt_domain *__g_itt_domain;

namespace SceneManagerConstants {} // namespace SceneManagerConstants

namespace SceneManagerInternal {} // namespace SceneManagerInternal

bool Eng::SceneManager::UpdateMaterialsBuffer() {
    const Ren::ApiContext &api = ren_ctx_.api();
    const Ren::StoragesRef &storages = ren_ctx_.storages();

    const uint32_t max_mat_count = storages.materials.capacity();
    const uint32_t req_mat_buf_size = std::max(1u, max_mat_count) * sizeof(material_data_t);

    const uint32_t max_tex_count = std::max(1u, MAX_TEX_PER_MATERIAL * max_mat_count);
    const uint32_t req_tex_buf_size = max_tex_count * sizeof(GLuint64);

    if (!scene_data_.persistent_data->textures_buf) {
        scene_data_.persistent_data->textures_buf =
            ren_ctx_.CreateBuffer(Ren::String{"Textures Buffer"}, Ren::eBufType::Storage, req_tex_buf_size);
    }

    const auto &[mat_main, mat_cold] = storages.buffers[scene_data_.persistent_data->materials];
    if (mat_cold.size < req_mat_buf_size) {
        if (!Buffer_Resize(api, mat_main, mat_cold, req_mat_buf_size, ren_ctx_.log())) {
            return false;
        }
    }

    const auto &[textures_buf_main, textures_buf_cold] = storages.buffers[scene_data_.persistent_data->textures_buf];
    if (textures_buf_cold.size < req_tex_buf_size) {
        Buffer_Resize(api, textures_buf_main, textures_buf_cold, req_tex_buf_size, ren_ctx_.log());
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

    const size_t TexSizePerMaterial = MAX_TEX_PER_MATERIAL * sizeof(GLuint64);

    Ren::BufferMain materials_upload_buf_main = {};
    Ren::BufferCold materials_upload_buf_cold = {};
    if (!Buffer_Init(api, materials_upload_buf_main, materials_upload_buf_cold, Ren::String{"Materials Upload Buffer"},
                     Ren::eBufType::Upload, (update_range.second - update_range.first) * sizeof(material_data_t),
                     ren_ctx_.log())) {
        return false;
    }
    SCOPE_EXIT({ Buffer_Destroy(api, materials_upload_buf_main, materials_upload_buf_cold); })

    Ren::BufferMain textures_upload_buf_main = {};
    Ren::BufferCold textures_upload_buf_cold = {};
    if (ren_ctx_.capabilities.bindless_texture) {
        if (!Buffer_Init(api, textures_upload_buf_main, textures_upload_buf_cold, Ren::String{"Textures Upload Buffer"},
                         Ren::eBufType::Upload, (update_range.second - update_range.first) * TexSizePerMaterial,
                         ren_ctx_.log())) {
            return false;
        }
    }

    auto *material_data =
        reinterpret_cast<material_data_t *>(Buffer_Map(api, materials_upload_buf_main, materials_upload_buf_cold));
    GLuint64 *texture_data = nullptr;
    GLuint64 white_tex_handle = 0, error_tex_handle = 0;
    if (ren_ctx_.capabilities.bindless_texture) {
        texture_data =
            reinterpret_cast<GLuint64 *>(Buffer_Map(api, textures_upload_buf_main, textures_upload_buf_cold));

        white_tex_handle = glGetTextureHandleARB(storages.images[white_tex_].first.img);
        if (!glIsTextureHandleResidentARB(white_tex_handle)) {
            glMakeTextureHandleResidentARB(white_tex_handle);
        }
        error_tex_handle = glGetTextureHandleARB(storages.images[error_tex_].first.img);
        if (!glIsTextureHandleResidentARB(error_tex_handle)) {
            glMakeTextureHandleResidentARB(error_tex_handle);
        }
    }

    const Ren::Sampler &sampler = storages.samplers[scene_data_.persistent_data->trilinear_sampler];

    for (uint32_t i = update_range.first; i < update_range.second; ++i) {
        const uint32_t rel_i = i - update_range.first;
        if (storages.materials.IsOccupied(i)) {
            const auto &[mat_main2, mat_cold2] = storages.materials.GetUnsafe(i);

            int j = 0;
            for (; j < int(mat_main2.textures.size()); ++j) {
                material_data[rel_i].texture_indices[j] = i * MAX_TEX_PER_MATERIAL + j;
                if (texture_data) {
                    const GLuint64 handle =
                        glGetTextureSamplerHandleARB(storages.images[mat_main2.textures[j]].first.img, sampler.id);
                    if (!glIsTextureHandleResidentARB(handle)) {
                        glMakeTextureHandleResidentARB(handle);
                    }
                    texture_data[rel_i * MAX_TEX_PER_MATERIAL + j] = handle;
                }
            }
            for (; j < MAX_TEX_PER_MATERIAL; ++j) {
                material_data[rel_i].texture_indices[j] = i * MAX_TEX_PER_MATERIAL + j;
                if (texture_data) {
                    texture_data[rel_i * MAX_TEX_PER_MATERIAL + j] = white_tex_handle;
                }
            }

            int k = 0;
            for (; k < int(mat_cold2.params.size()); ++k) {
                material_data[rel_i].params[k] = mat_cold2.params[k];
            }
            for (; k < MAX_MATERIAL_PARAMS; ++k) {
                material_data[rel_i].params[k] = Ren::Vec4f{0.0f};
            }
        } else {
            for (int j = 0; j < MAX_TEX_PER_MATERIAL; ++j) {
                material_data[rel_i].texture_indices[j] = i * MAX_TEX_PER_MATERIAL + j;
                if (texture_data) {
                    texture_data[rel_i * MAX_TEX_PER_MATERIAL + j] = error_tex_handle;
                }
            }
        }
    }

    if (texture_data) {
        Buffer_Unmap(api, textures_upload_buf_main, textures_upload_buf_cold);
        Buffer_UpdateSubRegion(api, textures_buf_main, textures_buf_cold, update_range.first * TexSizePerMaterial,
                               (update_range.second - update_range.first) * TexSizePerMaterial,
                               textures_upload_buf_main);
    }

    Buffer_Unmap(api, materials_upload_buf_main, materials_upload_buf_cold);

    Buffer_UpdateSubRegion(api, mat_main, mat_cold, update_range.first * sizeof(material_data_t),
                           (update_range.second - update_range.first) * sizeof(material_data_t),
                           materials_upload_buf_main);

    update_range = std::make_pair(std::numeric_limits<uint32_t>::max(), 0);

    return false;
}

// stubs
void Eng::SceneManager::Alloc_HWRT_TLAS() {}
Ren::AccStructHandle Eng::SceneManager::Build_HWRT_BLAS(const AccStructure &acc) { return {}; }