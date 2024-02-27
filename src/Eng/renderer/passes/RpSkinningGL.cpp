#include "RpSkinning.h"

#include <Ren/Buffer.h>
#include <Ren/GL.h>

#include "../Renderer_Structs.h"

#include "../shaders/skinning_interface.h"

void Eng::RpSkinningExecutor::Execute(RpBuilder &builder) {
    RpAllocBuf &skin_vtx_buf = builder.GetReadBuffer(skin_vtx_buf_);
    RpAllocBuf &skin_transforms_buf = builder.GetReadBuffer(skin_transforms_buf_);
    RpAllocBuf &shape_keys_buf = builder.GetReadBuffer(shape_keys_buf_);
    RpAllocBuf &delta_buf = builder.GetReadBuffer(delta_buf_);

    RpAllocBuf &vtx_buf1 = builder.GetWriteBuffer(vtx_buf1_);
    RpAllocBuf &vtx_buf2 = builder.GetWriteBuffer(vtx_buf2_);

    if (!p_list_->skin_regions.empty()) {
        const GLuint vertex_buf1_id = vtx_buf1.ref->id();
        const GLuint vertex_buf2_id = vtx_buf2.ref->id();
        const GLuint delta_buf_id = delta_buf.ref->id();
        const GLuint skin_vtx_buf_id = skin_vtx_buf.ref->id();

        glUseProgram(pi_skinning_.prog()->id());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Skinning::IN_VERTICES_SLOT, skin_vtx_buf_id);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Skinning::IN_MATRICES_SLOT, GLuint(skin_transforms_buf.ref->id()));
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Skinning::IN_SHAPE_KEYS_SLOT, GLuint(shape_keys_buf.ref->id()));
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Skinning::IN_DELTAS_SLOT, delta_buf_id);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Skinning::OUT_VERTICES0, vertex_buf1_id);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Skinning::OUT_VERTICES1, vertex_buf2_id);

        Ren::Buffer temp_unif_buffer =
            Ren::Buffer("Temp uniform buf", nullptr, Ren::eBufType::Uniform, sizeof(Skinning::Params), 16);

        for (uint32_t i = 0; i < uint32_t(p_list_->skin_regions.size()); i++) {
            const SkinRegion &sr = p_list_->skin_regions[i];

            const uint32_t non_shapekeyed_vertex_count = sr.vertex_count - sr.shape_keyed_vertex_count;

            if (non_shapekeyed_vertex_count) {
                Ren::Buffer temp_stage_buffer =
                    Ren::Buffer("Temp stage buf", nullptr, Ren::eBufType::Stage, sizeof(Skinning::Params), 16);
                {
                    Skinning::Params *stage_data =
                        reinterpret_cast<Skinning::Params *>(temp_stage_buffer.Map(Ren::eBufMap::Write));

                    stage_data->uSkinParams =
                        Ren::Vec4u{sr.in_vtx_offset, non_shapekeyed_vertex_count, sr.xform_offset, sr.out_vtx_offset};
                    stage_data->uShapeParamsCurr = Ren::Vec4u{0};
                    stage_data->uShapeParamsPrev = Ren::Vec4u{0};

                    temp_stage_buffer.FlushMappedRange(0, sizeof(Skinning::Params));
                    temp_stage_buffer.Unmap();
                }
                Ren::CopyBufferToBuffer(temp_stage_buffer, 0, temp_unif_buffer, 0, sizeof(Skinning::Params), nullptr);

                glBindBufferBase(GL_UNIFORM_BUFFER, BIND_UB_UNIF_PARAM_BUF, temp_unif_buffer.id());

                glDispatchCompute((sr.vertex_count + Skinning::LOCAL_GROUP_SIZE - 1) / Skinning::LOCAL_GROUP_SIZE, 1,
                                  1);
            }

            if (sr.shape_keyed_vertex_count) {
                Ren::Buffer temp_stage_buffer =
                    Ren::Buffer("Temp stage buf", nullptr, Ren::eBufType::Stage, sizeof(Skinning::Params), 16);
                {
                    Skinning::Params *stage_data =
                        reinterpret_cast<Skinning::Params *>(temp_stage_buffer.Map(Ren::eBufMap::Write));

                    stage_data->uSkinParams =
                        Ren::Vec4u{sr.in_vtx_offset + non_shapekeyed_vertex_count, sr.shape_keyed_vertex_count,
                                   sr.xform_offset, sr.out_vtx_offset + non_shapekeyed_vertex_count};
                    stage_data->uShapeParamsCurr =
                        Ren::Vec4u{sr.shape_key_offset_curr, sr.shape_key_count_curr, sr.delta_offset, 0};
                    stage_data->uShapeParamsPrev =
                        Ren::Vec4u{sr.shape_key_offset_prev, sr.shape_key_count_prev, sr.delta_offset, 0};

                    temp_stage_buffer.FlushMappedRange(0, sizeof(Skinning::Params));
                    temp_stage_buffer.Unmap();
                }
                Ren::CopyBufferToBuffer(temp_stage_buffer, 0, temp_unif_buffer, 0, sizeof(Skinning::Params), nullptr);

                glBindBufferBase(GL_UNIFORM_BUFFER, BIND_UB_UNIF_PARAM_BUF, temp_unif_buffer.id());

                glDispatchCompute(
                    (sr.shape_keyed_vertex_count + Skinning::LOCAL_GROUP_SIZE - 1) / Skinning::LOCAL_GROUP_SIZE, 1, 1);
            }
        }
    }
}