#include "RpUpdateBuffers.h"

#include <Ren/Buffer.h>
#include <Ren/GL.h>

#include "../Renderer_Structs.h"

void RpUpdateBuffers::Execute(RpBuilder &builder) {
    if (fences_[orphan_index_]) {
        auto sync = reinterpret_cast<GLsync>(fences_[orphan_index_]);
        const GLenum res = glClientWaitSync(sync, 0, 1000000000);
        if (res != GL_ALREADY_SIGNALED && res != GL_CONDITION_SATISFIED) {
            builder.log()->Error("RpUpdateBuffers: Wait failed!");
        }
        glDeleteSync(sync);
        fences_[orphan_index_] = nullptr;
    }

    const GLbitfield BufferRangeMapFlags =
        GLbitfield(GL_MAP_WRITE_BIT) | GLbitfield(GL_MAP_INVALIDATE_RANGE_BIT) |
        GLbitfield(GL_MAP_UNSYNCHRONIZED_BIT) | GLbitfield(GL_MAP_FLUSH_EXPLICIT_BIT);

    RpAllocBuf &skin_transforms_buf = builder.GetWriteBuffer(input_[0]);

    // Update bone transforms buffer
    if (skin_transforms_.count) {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, GLuint(skin_transforms_buf.ref->id()));

        void *pinned_mem = glMapBufferRange(
            GL_SHADER_STORAGE_BUFFER, orphan_index_ * SkinTransformsBufChunkSize,
            SkinTransformsBufChunkSize, BufferRangeMapFlags);
        if (pinned_mem) {
            const size_t skin_transforms_mem_size =
                skin_transforms_.count * sizeof(SkinTransform);
            memcpy(pinned_mem, skin_transforms_.data, skin_transforms_mem_size);
            glFlushMappedBufferRange(GL_SHADER_STORAGE_BUFFER, 0,
                                     skin_transforms_mem_size);
            glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        } else {
            builder.log()->Error(
                "RpUpdateBuffers: Failed to map skin transforms buffer!");
        }

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }

    RpAllocBuf &shape_keys_buf = builder.GetWriteBuffer(input_[1]);

    if (shape_keys_data_.count) {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, GLuint(shape_keys_buf.ref->id()));

        void *pinned_mem = glMapBufferRange(GL_SHADER_STORAGE_BUFFER,
                                            orphan_index_ * ShapeKeysBufChunkSize,
                                            ShapeKeysBufChunkSize, BufferRangeMapFlags);
        if (pinned_mem) {
            const size_t shape_keys_mem_size =
                shape_keys_data_.count * sizeof(ShapeKeyData);
            memcpy(pinned_mem, shape_keys_data_.data, shape_keys_mem_size);
            glFlushMappedBufferRange(GL_SHADER_STORAGE_BUFFER, 0, shape_keys_mem_size);
            glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        } else {
            builder.log()->Error("RpUpdateBuffers: Failed to map shape keys buffer!");
        }

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }

    Ren::Context &ctx = builder.ctx();

    RpAllocBuf &instances_buf = builder.GetWriteBuffer(input_[2]);

    if (!instances_buf.tbos[orphan_index_]) {
        const uint32_t offset = orphan_index_ * InstanceDataBufChunkSize;
        assert((offset % ctx.capabilities.tex_buf_offset_alignment == 0) &&
               "Offset is not properly aligned!");

        char name_buf[32];
        sprintf(name_buf, "Instances TBO #%i", orphan_index_);

        instances_buf.tbos[orphan_index_] =
            ctx.CreateTexture1D(name_buf, instances_buf.ref, Ren::eTexFormat::RawRGBA32F,
                                offset, InstanceDataBufChunkSize);
    }

    // Update instance buffer
    if (instances_.count) {
        glBindBuffer(GL_TEXTURE_BUFFER, GLuint(instances_buf.ref->id()));

        void *pinned_mem =
            glMapBufferRange(GL_TEXTURE_BUFFER, orphan_index_ * InstanceDataBufChunkSize,
                             InstanceDataBufChunkSize, BufferRangeMapFlags);
        if (pinned_mem) {
            const size_t instance_mem_size = instances_.count * sizeof(InstanceData);
            memcpy(pinned_mem, instances_.data, instance_mem_size);
            glFlushMappedBufferRange(GL_TEXTURE_BUFFER, 0, instance_mem_size);
            glUnmapBuffer(GL_TEXTURE_BUFFER);
        } else {
            builder.log()->Error("RpUpdateBuffers: Failed to map instance buffer!");
        }

        glBindBuffer(GL_TEXTURE_BUFFER, 0);
    }

    RpAllocBuf &cells_buf = builder.GetWriteBuffer(input_[3]);

    if (!cells_buf.tbos[orphan_index_]) {
        const uint32_t offset = orphan_index_ * CellsBufChunkSize;
        assert((offset % ctx.capabilities.tex_buf_offset_alignment == 0) &&
               "Offset is not properly aligned!");

        char name_buf[32];
        sprintf(name_buf, "Cells TBO #%i", orphan_index_);

        cells_buf.tbos[orphan_index_] =
            ctx.CreateTexture1D(name_buf, cells_buf.ref, Ren::eTexFormat::RawRG32U,
                                offset, CellsBufChunkSize);
    }

    // Update cells buffer
    if (cells_.count) {
        glBindBuffer(GL_TEXTURE_BUFFER, GLuint(cells_buf.ref->id()));

        void *pinned_mem =
            glMapBufferRange(GL_TEXTURE_BUFFER, orphan_index_ * CellsBufChunkSize,
                             CellsBufChunkSize, BufferRangeMapFlags);
        if (pinned_mem) {
            const size_t cells_mem_size = cells_.count * sizeof(CellData);
            memcpy(pinned_mem, cells_.data, cells_mem_size);
            glFlushMappedBufferRange(GL_TEXTURE_BUFFER, 0, cells_mem_size);
            glUnmapBuffer(GL_TEXTURE_BUFFER);
        } else {
            builder.log()->Error("RpUpdateBuffers: Failed to map cells buffer!");
        }

        glBindBuffer(GL_TEXTURE_BUFFER, 0);
    }

    RpAllocBuf &lights_buf = builder.GetWriteBuffer(input_[4]);

    if (!lights_buf.tbos[orphan_index_]) { // Create buffer for lights information
        const uint32_t offset = orphan_index_ * LightsBufChunkSize;
        assert((offset % ctx.capabilities.tex_buf_offset_alignment == 0) &&
               "Offset is not properly aligned!");

        char name_buf[32];
        sprintf(name_buf, "Lights TBO #%i", orphan_index_);

        lights_buf.tbos[orphan_index_] =
            ctx.CreateTexture1D(name_buf, lights_buf.ref, Ren::eTexFormat::RawRGBA32F,
                                offset, LightsBufChunkSize);
    }

    // Update lights buffer
    if (light_sources_.count) {
        glBindBuffer(GL_TEXTURE_BUFFER, GLuint(lights_buf.ref->id()));

        void *pinned_mem =
            glMapBufferRange(GL_TEXTURE_BUFFER, orphan_index_ * LightsBufChunkSize,
                             LightsBufChunkSize, BufferRangeMapFlags);
        if (pinned_mem) {
            const size_t lights_mem_size = light_sources_.count * sizeof(LightSourceItem);
            memcpy(pinned_mem, light_sources_.data, lights_mem_size);
            glFlushMappedBufferRange(GL_TEXTURE_BUFFER, 0, lights_mem_size);
            glUnmapBuffer(GL_TEXTURE_BUFFER);
        } else {
            builder.log()->Error("RpUpdateBuffers: Failed to map lights buffer!");
        }

        glBindBuffer(GL_TEXTURE_BUFFER, 0);
    }

    RpAllocBuf &decals_buf = builder.GetWriteBuffer(input_[5]);

    if (!decals_buf.tbos[orphan_index_]) {
        const uint32_t offset = orphan_index_ * DecalsBufChunkSize;
        assert((offset % ctx.capabilities.tex_buf_offset_alignment == 0) &&
               "Offset is not properly aligned!");

        char name_buf[32];
        sprintf(name_buf, "Decals TBO #%i", orphan_index_);

        decals_buf.tbos[orphan_index_] =
            ctx.CreateTexture1D(name_buf, decals_buf.ref, Ren::eTexFormat::RawRGBA32F,
                                offset, DecalsBufChunkSize);
    }

    // Update decals buffer
    if (decals_.count) {
        glBindBuffer(GL_TEXTURE_BUFFER, GLuint(decals_buf.ref->id()));

        void *pinned_mem =
            glMapBufferRange(GL_TEXTURE_BUFFER, orphan_index_ * DecalsBufChunkSize,
                             DecalsBufChunkSize, BufferRangeMapFlags);
        if (pinned_mem) {
            const size_t decals_mem_size = decals_.count * sizeof(DecalItem);
            memcpy(pinned_mem, decals_.data, decals_mem_size);
            glFlushMappedBufferRange(GL_TEXTURE_BUFFER, 0, decals_mem_size);
            glUnmapBuffer(GL_TEXTURE_BUFFER);
        } else {
            builder.log()->Error("RpUpdateBuffers: Failed to map decals buffer!");
        }

        glBindBuffer(GL_TEXTURE_BUFFER, 0);
    }

    RpAllocBuf &items_buf = builder.GetWriteBuffer(input_[6]);

    if (!items_buf.tbos[orphan_index_]) {
        const uint32_t offset = orphan_index_ * ItemsBufChunkSize;
        assert((offset % ctx.capabilities.tex_buf_offset_alignment == 0) &&
               "Offset is not properly aligned!");

        char name_buf[32];
        sprintf(name_buf, "Items TBO #%i", orphan_index_);

        items_buf.tbos[orphan_index_] =
            ctx.CreateTexture1D(name_buf, items_buf.ref, Ren::eTexFormat::RawRG32U,
                                offset, ItemsBufChunkSize);
    }

    // Update items buffer
    if (items_.count) {
        glBindBuffer(GL_TEXTURE_BUFFER, GLuint(items_buf.ref->id()));

        void *pinned_mem =
            glMapBufferRange(GL_TEXTURE_BUFFER, orphan_index_ * ItemsBufChunkSize,
                             ItemsBufChunkSize, BufferRangeMapFlags);
        if (pinned_mem) {
            const size_t items_mem_size = items_.count * sizeof(ItemData);
            memcpy(pinned_mem, items_.data, items_mem_size);
            glFlushMappedBufferRange(GL_TEXTURE_BUFFER, 0, items_mem_size);
            glUnmapBuffer(GL_TEXTURE_BUFFER);
        } else {
            builder.log()->Error("RpUpdateBuffers: Failed to map items buffer!");
        }

        glBindBuffer(GL_TEXTURE_BUFFER, 0);
    } else {
        glBindBuffer(GL_TEXTURE_BUFFER, GLuint(items_buf.ref->id()));
        ItemData dummy = {};
        glBufferSubData(GL_TEXTURE_BUFFER, orphan_index_ * ItemsBufChunkSize,
                        sizeof(ItemData), &dummy);
        glBindBuffer(GL_TEXTURE_BUFFER, 0);
    }

    //
    // Update UBO with data that is shared between passes
    //
    RpAllocBuf &unif_shared_data_buf = builder.GetWriteBuffer(input_[7]);

    { // Prepare data that is shared for all instances
        SharedDataBlock shrd_data;

        shrd_data.uViewMatrix = draw_cam_->view_matrix();
        shrd_data.uProjMatrix = draw_cam_->proj_matrix();

        shrd_data.uTaaInfo[0] = draw_cam_->px_offset()[0];
        shrd_data.uTaaInfo[1] = draw_cam_->px_offset()[1];

        shrd_data.uProjMatrix[2][0] += draw_cam_->px_offset()[0];
        shrd_data.uProjMatrix[2][1] += draw_cam_->px_offset()[1];

        shrd_data.uViewProjMatrix = shrd_data.uProjMatrix * shrd_data.uViewMatrix;
        shrd_data.uViewProjPrevMatrix = view_state_->prev_clip_from_world;
        shrd_data.uInvViewMatrix = Ren::Inverse(shrd_data.uViewMatrix);
        shrd_data.uInvProjMatrix = Ren::Inverse(shrd_data.uProjMatrix);
        shrd_data.uInvViewProjMatrix = Ren::Inverse(shrd_data.uViewProjMatrix);
        // delta matrix between current and previous frame
        shrd_data.uDeltaMatrix =
            view_state_->prev_clip_from_view *
            (view_state_->down_buf_view_from_world * shrd_data.uInvViewMatrix);

        if (shadow_regions_.count) {
            assert(shadow_regions_.count <= REN_MAX_SHADOWMAPS_TOTAL);
            memcpy(&shrd_data.uShadowMapRegions[0], &shadow_regions_.data[0],
                   sizeof(ShadowMapRegion) * shadow_regions_.count);
        }

        shrd_data.uSunDir =
            Ren::Vec4f{env_->sun_dir[0], env_->sun_dir[1], env_->sun_dir[2], 0.0f};
        shrd_data.uSunCol =
            Ren::Vec4f{env_->sun_col[0], env_->sun_col[1], env_->sun_col[2], 0.0f};

        // actual resolution and full resolution
        shrd_data.uResAndFRes =
            Ren::Vec4f{float(view_state_->act_res[0]), float(view_state_->act_res[1]),
                       float(view_state_->scr_res[0]), float(view_state_->scr_res[1])};

        const float near = draw_cam_->near(), far = draw_cam_->far();
        const float time_s = 0.001f * Sys::GetTimeMs();
        const float transparent_near = near;
        const float transparent_far = 16.0f;
        const int transparent_mode =
#if (REN_OIT_MODE == REN_OIT_MOMENT_BASED)
            (render_flags_ & EnableOIT) ? 2 : 0;
#elif (REN_OIT_MODE == REN_OIT_WEIGHTED_BLENDED)
            (render_flags_ & EnableOIT) ? 1 : 0;
#else
            0;
#endif

        shrd_data.uTranspParamsAndTime =
            Ren::Vec4f{std::log(transparent_near),
                       std::log(transparent_far) - std::log(transparent_near),
                       float(transparent_mode), time_s};
        shrd_data.uClipInfo =
            Ren::Vec4f{near * far, near, far, std::log2(1.0f + far / near)};

        const Ren::Vec3f &pos = draw_cam_->world_position();
        shrd_data.uCamPosAndGamma = Ren::Vec4f{pos[0], pos[1], pos[2], 2.2f};
        shrd_data.uWindScroll =
            Ren::Vec4f{env_->curr_wind_scroll_lf[0], env_->curr_wind_scroll_lf[1],
                       env_->curr_wind_scroll_hf[0], env_->curr_wind_scroll_hf[1]};
        shrd_data.uWindScrollPrev =
            Ren::Vec4f{env_->prev_wind_scroll_lf[0], env_->prev_wind_scroll_lf[1],
                       env_->prev_wind_scroll_hf[0], env_->prev_wind_scroll_hf[1]};

        memcpy(&shrd_data.uProbes[0], probes_.data, sizeof(ProbeItem) * probes_.count);
        memcpy(&shrd_data.uEllipsoids[0], ellipsoids_.data,
               sizeof(EllipsItem) * ellipsoids_.count);

        glBindBuffer(GL_UNIFORM_BUFFER, GLuint(unif_shared_data_buf.ref->id()));

        const GLbitfield BufferRangeBindFlags =
            GLbitfield(GL_MAP_WRITE_BIT) | GLbitfield(GL_MAP_INVALIDATE_RANGE_BIT) |
            GLbitfield(GL_MAP_UNSYNCHRONIZED_BIT) | GLbitfield(GL_MAP_FLUSH_EXPLICIT_BIT);

        void *pinned_mem =
            glMapBufferRange(GL_UNIFORM_BUFFER, orphan_index_ * SharedDataBlockSize,
                             sizeof(SharedDataBlock), BufferRangeBindFlags);
        if (pinned_mem) {
            memcpy(pinned_mem, &shrd_data, sizeof(SharedDataBlock));
            glFlushMappedBufferRange(GL_UNIFORM_BUFFER, 0, sizeof(SharedDataBlock));
            glUnmapBuffer(GL_UNIFORM_BUFFER);
        }

        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }
}