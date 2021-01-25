#include "RpSkinning.h"

#include <Ren/Buffer.h>
#include <Ren/GL.h>

#include "../DebugMarker.h"
#include "../Renderer_Structs.h"

void RpSkinning::Execute(RpBuilder &builder) {
    LazyInit(builder.ctx(), builder.sh());

    RpAllocBuf& skin_transforms_buf = builder.GetReadBuffer(skin_transforms_buf_);
    RpAllocBuf& shape_keys_buf = builder.GetReadBuffer(shape_keys_buf_);

    if (skin_regions_.count) {
        const GLuint vertex_buf1_id = vtx_buf1_->id();
        const GLuint vertex_buf2_id = vtx_buf2_->id();
        const GLuint delta_buf_id = delta_buf_->id();
        const GLuint skin_vtx_buf_id = skin_vtx_buf_->id();

        const int SkinLocalGroupSize = 128;
        const Ren::Program *p = skinning_prog_.get();

        glUseProgram(p->id());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, skin_vtx_buf_id);
        glBindBufferRange(
            GL_SHADER_STORAGE_BUFFER, 1, (GLuint)skin_transforms_buf.ref->id(),
            orphan_index_ * SkinTransformsBufChunkSize, SkinTransformsBufChunkSize);
        glBindBufferRange(GL_SHADER_STORAGE_BUFFER, 2,
                          (GLuint)shape_keys_buf.ref->id(),
                          orphan_index_ * ShapeKeysBufChunkSize, ShapeKeysBufChunkSize);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, delta_buf_id);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, vertex_buf1_id);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, vertex_buf2_id);

        for (uint32_t i = 0; i < skin_regions_.count; i++) {
            const SkinRegion &sr = skin_regions_.data[i];

            const uint32_t non_shapekeyed_vertex_count =
                sr.vertex_count - sr.shape_keyed_vertex_count;

            if (non_shapekeyed_vertex_count) {
                glUniform4ui(0, sr.in_vtx_offset, non_shapekeyed_vertex_count,
                             sr.xform_offset, sr.out_vtx_offset);
                glUniform4ui(1, 0, 0, 0, 0);
                glUniform4ui(2, 0, 0, 0, 0);

                glDispatchCompute((sr.vertex_count + SkinLocalGroupSize - 1) /
                                      SkinLocalGroupSize,
                                  1, 1);
            }

            if (sr.shape_keyed_vertex_count) {
                glUniform4ui(0, sr.in_vtx_offset + non_shapekeyed_vertex_count,
                             sr.shape_keyed_vertex_count, sr.xform_offset,
                             sr.out_vtx_offset + non_shapekeyed_vertex_count);
                glUniform4ui(1, sr.shape_key_offset_curr, sr.shape_key_count_curr,
                             sr.delta_offset, 0);
                glUniform4ui(2, sr.shape_key_offset_prev, sr.shape_key_count_prev,
                             sr.delta_offset, 0);

                glDispatchCompute((sr.shape_keyed_vertex_count + SkinLocalGroupSize - 1) /
                                      SkinLocalGroupSize,
                                  1, 1);
            }
        }

        glMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);
    }
}