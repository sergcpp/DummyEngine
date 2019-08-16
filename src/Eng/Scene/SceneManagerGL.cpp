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

    if (!scene_data_.persistant_data.materials_buf) {
        scene_data_.persistant_data.materials_buf =
            ren_ctx_.LoadBuffer("Materials Buffer", Ren::eBufType::Storage, req_mat_buf_size);
    }

    if (scene_data_.persistant_data.materials_buf->size() < req_mat_buf_size) {
        scene_data_.persistant_data.materials_buf->Resize(req_mat_buf_size);
    }

    const uint32_t max_tex_count = std::max(1u, REN_MAX_TEX_PER_MATERIAL * max_mat_count);
    const uint32_t req_tex_buf_size = max_tex_count * sizeof(GLuint64);

    if (!scene_data_.persistant_data.textures_buf) {
        scene_data_.persistant_data.textures_buf =
            ren_ctx_.LoadBuffer("Textures Buffer", Ren::eBufType::Storage, req_tex_buf_size);
    }

    if (scene_data_.persistant_data.textures_buf->size() < req_tex_buf_size) {
        scene_data_.persistant_data.textures_buf->Resize(req_tex_buf_size);
    }

    auto &update_range = scene_data_.mat_update_ranges[0];
    for (const uint32_t i : scene_data_.material_changes) {
        update_range.first = std::min(update_range.first, i);
        update_range.second = std::max(update_range.second, i + 1);
    }
    scene_data_.material_changes.clear();

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
    if (ren_ctx_.capabilities.bindless_texture) {
        texture_data = reinterpret_cast<GLuint64 *>(textures_stage_buf.Map(Ren::BufMapWrite));
    }

    for (uint32_t i = update_range.first; i < update_range.second; ++i) {
        const uint32_t rel_i = i - update_range.first;
        const Ren::Material *mat = scene_data_.materials.GetOrNull(i);
        if (mat) {
            for (int j = 0; j < int(mat->textures.size()); ++j) {
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
            if (!mat->params.empty()) {
                material_data[rel_i].params = mat->params[0];
            }
        }
    }

    if (texture_data) {
        textures_stage_buf.FlushMappedRange(0, (update_range.second - update_range.first) * TexSizePerMaterial);
        textures_stage_buf.Unmap();
        scene_data_.persistant_data.textures_buf->UpdateSubRegion(
            update_range.first * TexSizePerMaterial, (update_range.second - update_range.first) * TexSizePerMaterial,
            textures_stage_buf);
    }

    materials_stage_buf.FlushMappedRange(0, (update_range.second - update_range.first) * sizeof(MaterialData));
    materials_stage_buf.Unmap();
    scene_data_.persistant_data.materials_buf->UpdateSubRegion(
        update_range.first * sizeof(MaterialData), (update_range.second - update_range.first) * sizeof(MaterialData),
        materials_stage_buf);

    update_range = std::make_pair(std::numeric_limits<uint32_t>::max(), 0);
}

void SceneManager::InitPipelinesForProgram(const Ren::ProgramRef &prog, const uint32_t mat_flags,
                                           Ren::SmallVectorImpl<Ren::PipelineRef> &out_pipelines) {
    Ren::RastState rast_state;
    rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);
    rast_state.poly.mode = uint8_t(Ren::ePolygonMode::Fill);

    rast_state.depth.test_enabled = true;
    rast_state.depth.compare_op = unsigned(Ren::eCompareOp::Equal);

    const uint32_t new_index = scene_data_.persistant_data.pipelines.emplace();
    Ren::Pipeline &new_pipeline = scene_data_.persistant_data.pipelines.at(new_index);

    const bool res =
        new_pipeline.Init(ren_ctx_.api_ctx(), rast_state, prog, &draw_pass_vi_, &rp_main_draw_, ren_ctx_.log());
    if (!res) {
        ren_ctx_.log()->Error("Failed to initialize pipeline!");
    }

    out_pipelines.emplace_back(&scene_data_.persistant_data.pipelines, new_index);
}
