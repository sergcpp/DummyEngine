#include "SceneManager.h"

#include <Ren/Context.h>
#include <Ren/Utils.h>

#include "../Renderer/Renderer_Structs.h"

#include <vtune/ittnotify.h>
extern __itt_domain *__g_itt_domain;

namespace SceneManagerConstants {} // namespace SceneManagerConstants

namespace SceneManagerInternal {} // namespace SceneManagerInternal

PersistentBuffers SceneManager::persistent_bufs() const {
    PersistentBuffers ret;
    ret.materials_buf = scene_data_.materials_buf->handle();
    ret.materials_buf_range.first = scene_data_.mat_buf_index *
                                    scene_data_.materials.capacity() *
                                    sizeof(MaterialData);
    ret.materials_buf_range.second =
        scene_data_.materials.capacity() * sizeof(MaterialData);
    ret.textures_buf = scene_data_.textures_buf->handle();
    ret.textures_buf_range.first = scene_data_.mat_buf_index * REN_MAX_TEX_PER_MATERIAL *
                                   scene_data_.materials.capacity() * sizeof(uint64_t);
    ret.textures_buf_range.second =
        REN_MAX_TEX_PER_MATERIAL * scene_data_.materials.capacity() * sizeof(uint64_t);
    return ret;
}

void SceneManager::InsertPersistentBuffersFence() {
    if (!scene_data_.mat_buf_sync[scene_data_.mat_buf_index]) {
        scene_data_.mat_buf_sync[scene_data_.mat_buf_index] =
            glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    }
}

void SceneManager::UpdateMaterialsBuffer() {
    const uint32_t max_mat_count = scene_data_.materials.capacity();
    const uint32_t req_mat_buf_size = max_mat_count * sizeof(MaterialData);

    if (!scene_data_.materials_buf) {
        scene_data_.materials_buf = ren_ctx_.CreateBuffer(
            "Materials Buffer", Ren::eBufferType::Storage, Ren::eBufferAccessType::Draw,
            Ren::eBufferAccessFreq::Dynamic, FrameSyncWindow * req_mat_buf_size);
    }

    if (scene_data_.materials_buf->size() < FrameSyncWindow * req_mat_buf_size) {
        scene_data_.materials_buf->Resize(FrameSyncWindow * req_mat_buf_size);
    }

    const uint32_t max_tex_count = REN_MAX_TEX_PER_MATERIAL * max_mat_count;
    const uint32_t req_tex_buf_size = max_tex_count * sizeof(GLuint64);

    if (!scene_data_.textures_buf) {
        scene_data_.textures_buf = ren_ctx_.CreateBuffer(
            "Textures Buffer", Ren::eBufferType::Storage, Ren::eBufferAccessType::Draw,
            Ren::eBufferAccessFreq::Dynamic, FrameSyncWindow * req_tex_buf_size);
    }

    if (scene_data_.textures_buf->size() < FrameSyncWindow * req_tex_buf_size) {
        scene_data_.textures_buf->Resize(FrameSyncWindow * req_tex_buf_size);
    }

    // propagate material changes
    for (const uint32_t i : scene_data_.material_changes) {
        for (int j = 0; j < FrameSyncWindow; ++j) {
            scene_data_.mat_update_ranges[j].first =
                std::min(scene_data_.mat_update_ranges[j].first, i);
            scene_data_.mat_update_ranges[j].second =
                std::max(scene_data_.mat_update_ranges[j].second, i);
        }
    }
    scene_data_.material_changes.clear();

    const uint32_t next_buf_index = (scene_data_.mat_buf_index + 1) % FrameSyncWindow;
    auto &cur_update_range = scene_data_.mat_update_ranges[next_buf_index];

    if (cur_update_range.second < cur_update_range.first) {
        return;
    }

    scene_data_.mat_buf_index = next_buf_index;
    if (scene_data_.mat_buf_sync[scene_data_.mat_buf_index]) {
        auto sync =
            reinterpret_cast<GLsync>(scene_data_.mat_buf_sync[scene_data_.mat_buf_index]);
        const GLenum res = glClientWaitSync(sync, 0, 1000000000);
        if (res != GL_ALREADY_SIGNALED && res != GL_CONDITION_SATISFIED) {
            ren_ctx_.log()->Error("RpUpdateBuffers: Wait failed!");
        }
        glDeleteSync(sync);
        scene_data_.mat_buf_sync[scene_data_.mat_buf_index] = nullptr;
    }

    const uint32_t mat_buf_offset = scene_data_.mat_buf_index * req_mat_buf_size;
    const uint32_t tex_buf_offset = scene_data_.mat_buf_index * req_tex_buf_size;

    MaterialData *material_data =
        reinterpret_cast<MaterialData *>(scene_data_.materials_buf->MapRange(
            mat_buf_offset + cur_update_range.first * sizeof(MaterialData),
            (cur_update_range.second - cur_update_range.first + 1) *
                sizeof(MaterialData)));
    GLuint64 *texture_data = nullptr;
    if (ren_ctx_.capabilities.bindless_texture) {
        texture_data = reinterpret_cast<GLuint64 *>(scene_data_.textures_buf->MapRange(
            tex_buf_offset +
                cur_update_range.first * REN_MAX_TEX_PER_MATERIAL * sizeof(GLuint64),
            (cur_update_range.second - cur_update_range.first + 1) *
                REN_MAX_TEX_PER_MATERIAL * sizeof(GLuint64)));
    }

    for (uint32_t i = cur_update_range.first; i <= cur_update_range.second; ++i) {
        const uint32_t rel_i = i - cur_update_range.first;
        const Ren::Material *mat = scene_data_.materials.GetOrNull(i);
        if (mat) {
            for (int j = 0; j < int(mat->textures.size()); ++j) {
                material_data[rel_i].texture_indices[j] =
                    i * REN_MAX_TEX_PER_MATERIAL + j;
                if (texture_data) {
                    const GLuint64 handle = glGetTextureSamplerHandleARB(
                        mat->textures[j]->id(), mat->samplers[j]->id());
                    if (!glIsTextureHandleResidentARB(handle)) {
                        glMakeTextureHandleResidentARB(handle);
                    }
                    texture_data[rel_i * REN_MAX_TEX_PER_MATERIAL + j] = handle;
                }
            }
            if (!mat->params.empty()) {
                material_data[rel_i].params = mat->params[0];
            }
        }
    }

    if (texture_data) {
        scene_data_.textures_buf->FlushRange(
            0, (cur_update_range.second - cur_update_range.first + 1) *
                   REN_MAX_TEX_PER_MATERIAL * sizeof(GLuint64));
        scene_data_.textures_buf->Unmap();
    }
    scene_data_.materials_buf->FlushRange(
        0, (cur_update_range.second - cur_update_range.first + 1) * sizeof(MaterialData));
    scene_data_.materials_buf->Unmap();

    // reset just updated range
    cur_update_range.first = max_mat_count - 1;
    cur_update_range.second = 0;
}
