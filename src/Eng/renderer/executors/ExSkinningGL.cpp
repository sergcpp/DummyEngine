#include "ExSkinning.h"

#include <Ren/Buffer.h>
#include <Ren/Context.h>
#include <Ren/Gl/GL.h>
#include <Sys/ScopeExit.h>

#include "../Renderer_Structs.h"
#include "../framegraph/FgBuilder.h"
#include "../shaders/skinning_interface.h"

void Eng::ExSkinning::Execute(const FgContext &fg) {
    LazyInit(fg);

    const Ren::BufferROHandle skin_vtx = fg.AccessROBuffer(skin_vtx_);
    const Ren::BufferROHandle skin_transforms = fg.AccessROBuffer(skin_transforms_);
    const Ren::BufferROHandle shape_keys = fg.AccessROBuffer(shape_keys_);
    const Ren::BufferROHandle delta = fg.AccessROBuffer(delta_);

    const Ren::BufferHandle vtx_buf1 = fg.AccessRWBuffer(vtx_buf1_);
    const Ren::BufferHandle vtx_buf2 = fg.AccessRWBuffer(vtx_buf2_);

    const Ren::ApiContext &api = fg.ren_ctx().api();
    const Ren::StoragesRef &storages = fg.storages();

    const Ren::PipelineMain &pi = storages.pipelines[pi_skinning_].first;
    const Ren::ProgramMain &pr = storages.programs[pi.prog].first;

    if (!p_list_->skin_regions.empty()) {
        const GLuint vertex_buf1_id = storages.buffers[vtx_buf1].first.buf;
        const GLuint vertex_buf2_id = storages.buffers[vtx_buf2].first.buf;

        glUseProgram(pr.id);
        const Ren::BufferMain &skin_vtx_main = storages.buffers[skin_vtx].first;
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Skinning::IN_VERTICES_SLOT, skin_vtx_main.buf);
        const Ren::BufferMain &skin_transforms_main = storages.buffers[skin_transforms].first;
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Skinning::IN_MATRICES_SLOT, GLuint(skin_transforms_main.buf));
        const Ren::BufferMain &shape_keys_main = storages.buffers[shape_keys].first;
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Skinning::IN_SHAPE_KEYS_SLOT, GLuint(shape_keys_main.buf));
        const Ren::BufferMain &delta_main = storages.buffers[delta].first;
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Skinning::IN_DELTAS_SLOT, delta_main.buf);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Skinning::OUT_VERTICES0, vertex_buf1_id);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Skinning::OUT_VERTICES1, vertex_buf2_id);

        Ren::BufferMain temp_unif_buffer_main = {};
        Ren::BufferCold temp_unif_buffer_cold = {};
        if (!Buffer_Init(api, temp_unif_buffer_main, temp_unif_buffer_cold, Ren::String{"Temp uniform buf"},
                         Ren::eBufType::Uniform, sizeof(Skinning::Params), fg.log())) {
            fg.log()->Error("Failed to initialize temp uniform buffer");
            return;
        }
        SCOPE_EXIT({ Buffer_Destroy(api, temp_unif_buffer_main, temp_unif_buffer_cold); })

        for (uint32_t i = 0; i < uint32_t(p_list_->skin_regions.size()); i++) {
            const skin_region_t &sr = p_list_->skin_regions[i];

            const uint32_t non_shapekeyed_vertex_count = sr.vertex_count - sr.shape_keyed_vertex_count;

            if (non_shapekeyed_vertex_count) {
                Ren::BufferMain temp_stage_buffer_main = {};
                Ren::BufferCold temp_stage_buffer_cold = {};
                if (!Buffer_Init(api, temp_stage_buffer_main, temp_stage_buffer_cold, Ren::String{"Temp upload buf"},
                                 Ren::eBufType::Upload, sizeof(Skinning::Params), fg.log())) {
                    fg.log()->Error("Failed to initialize temp upload buffer");
                    return;
                }
                SCOPE_EXIT({ Buffer_Destroy(api, temp_stage_buffer_main, temp_stage_buffer_cold); })

                {
                    Skinning::Params *stage_data = reinterpret_cast<Skinning::Params *>(
                        Buffer_Map(api, temp_stage_buffer_main, temp_stage_buffer_cold));

                    stage_data->uSkinParams =
                        Ren::Vec4u{sr.in_vtx_offset, non_shapekeyed_vertex_count, sr.xform_offset, sr.out_vtx_offset};
                    stage_data->uShapeParamsCurr = Ren::Vec4u{0};
                    stage_data->uShapeParamsPrev = Ren::Vec4u{0};

                    Buffer_Unmap(api, temp_stage_buffer_main, temp_stage_buffer_cold);
                }
                CopyBufferToBuffer(api, temp_stage_buffer_main, 0, temp_unif_buffer_main, 0, sizeof(Skinning::Params),
                                   nullptr);

                glBindBufferBase(GL_UNIFORM_BUFFER, BIND_PUSH_CONSTANT_BUF, temp_unif_buffer_main.buf);

                glDispatchCompute((sr.vertex_count + Skinning::GRP_SIZE - 1) / Skinning::GRP_SIZE, 1, 1);
            }

            if (sr.shape_keyed_vertex_count) {
                Ren::BufferMain temp_stage_buffer_main = {};
                Ren::BufferCold temp_stage_buffer_cold = {};
                if (!Buffer_Init(api, temp_stage_buffer_main, temp_stage_buffer_cold, Ren::String{"Temp upload buf"},
                                 Ren::eBufType::Upload, sizeof(Skinning::Params), fg.log())) {
                    fg.log()->Error("Failed to initialize temp upload buffer");
                    return;
                }
                SCOPE_EXIT({ Buffer_Destroy(api, temp_stage_buffer_main, temp_stage_buffer_cold); })

                {
                    Skinning::Params *stage_data = reinterpret_cast<Skinning::Params *>(
                        Buffer_Map(api, temp_stage_buffer_main, temp_stage_buffer_cold));

                    stage_data->uSkinParams =
                        Ren::Vec4u{sr.in_vtx_offset + non_shapekeyed_vertex_count, sr.shape_keyed_vertex_count,
                                   sr.xform_offset, sr.out_vtx_offset + non_shapekeyed_vertex_count};
                    stage_data->uShapeParamsCurr =
                        Ren::Vec4u{sr.shape_key_offset_curr, sr.shape_key_count_curr, sr.delta_offset, 0};
                    stage_data->uShapeParamsPrev =
                        Ren::Vec4u{sr.shape_key_offset_prev, sr.shape_key_count_prev, sr.delta_offset, 0};

                    Buffer_Unmap(api, temp_stage_buffer_main, temp_stage_buffer_cold);
                }
                CopyBufferToBuffer(api, temp_stage_buffer_main, 0, temp_unif_buffer_main, 0, sizeof(Skinning::Params),
                                   nullptr);

                glBindBufferBase(GL_UNIFORM_BUFFER, BIND_PUSH_CONSTANT_BUF, temp_unif_buffer_main.buf);

                glDispatchCompute((sr.shape_keyed_vertex_count + Skinning::GRP_SIZE - 1) / Skinning::GRP_SIZE, 1, 1);
            }
        }
    }
}