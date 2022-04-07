#include "RpUpdateBuffers.h"
#if 0
#include <Ren/Context.h>
#include <Sys/Time_.h>

#include "../Renderer_Structs.h"

void RpUpdateBuffers::Setup(RpBuilder &builder, const DrawList &list, const ViewState *view_state,
                            const char skin_transforms_buf[], const char shape_keys_buf[], const char instances_buf[],
                            const char instance_indices_buf[], const char cells_buf[], const char lights_buf[],
                            const char decals_buf[], const char items_buf[], const char shared_data_buf[],
                            const char atomic_counter_buf[]) {
    assert(list.instances.count < REN_MAX_INSTANCES_TOTAL);
    assert(list.instance_indices.count < REN_MAX_INSTANCES_TOTAL);
    assert(list.skin_transforms.count < REN_MAX_SKIN_XFORMS_TOTAL);
    assert(list.skin_regions.count < REN_MAX_SKIN_REGIONS_TOTAL);
    assert(list.skin_vertices_count < REN_MAX_SKIN_VERTICES_TOTAL);
    assert(list.lights.count < REN_MAX_LIGHTS_TOTAL);
    assert(list.decals.count < REN_MAX_DECALS_TOTAL);
    assert(list.probes.count < REN_MAX_PROBES_TOTAL);
    assert(list.ellipsoids.count < REN_MAX_ELLIPSES_TOTAL);
    assert(list.items.count < REN_MAX_ITEMS_TOTAL);

    skin_transforms_ = list.skin_transforms;
    skin_transforms_stage_buf_ = list.skin_transforms_stage_buf;
    shape_keys_ = list.shape_keys_data;
    shape_keys_stage_buf_ = list.shape_keys_stage_buf;
    instances_ = list.instances;
    instances_stage_buf_ = list.instances_stage_buf;
    instance_indices_ = list.instance_indices;
    instance_indices_stage_buf_ = list.instance_indices_stage_buf;
    cells_ = list.cells;
    cells_stage_buf_ = list.cells_stage_buf;
    lights_ = list.lights;
    lights_stage_buf_ = list.lights_stage_buf;
    decals_ = list.decals;
    decals_stage_buf_ = list.decals_stage_buf;
    items_ = list.items;
    items_stage_buf_ = list.items_stage_buf;
    shadow_regions_ = list.shadow_regions;
    probes_ = list.probes;
    ellipsoids_ = list.ellipsoids;
    render_flags_ = list.render_flags;

    shared_data_stage_buf_ = list.shared_data_stage_buf;

    env_ = &list.env;

    draw_cam_ = &list.draw_cam;
    view_state_ = view_state;

    /*{ // create skin transforms buffer
        RpBufDesc desc;
        desc.type = Ren::eBufType::Texture;
        desc.size = SkinTransformsBufChunkSize;
        skin_transforms_buf_ =
            builder.WriteBuffer(skin_transforms_buf, desc, Ren::eResState::CopyDst, Ren::eStageBits::Transfer, *this);
    }*/
    /*{ // create shape keys buffer
        RpBufDesc desc;
        desc.type = Ren::eBufType::Texture;
        desc.size = ShapeKeysBufChunkSize;
        shape_keys_buf_ =
            builder.WriteBuffer(shape_keys_buf, desc, Ren::eResState::CopyDst, Ren::eStageBits::Transfer, *this);
    }*/
    /*{ // create instances buffer
        RpBufDesc desc;
        desc.type = Ren::eBufType::Texture;
        desc.size = InstanceDataBufChunkSize;
        instances_buf_ =
            builder.WriteBuffer(instances_buf, desc, Ren::eResState::CopyDst, Ren::eStageBits::Transfer, *this);
    }*/
    /*{ // create instance indices buffer
        RpBufDesc desc;
        desc.type = Ren::eBufType::Texture;
        desc.size = InstanceIndicesBufChunkSize;
        instance_indices_buf_ =
            builder.WriteBuffer(instance_indices_buf, desc, Ren::eResState::CopyDst, Ren::eStageBits::Transfer, *this);
    }*/
    /*{ // create cells buffer
        RpBufDesc desc;
        desc.type = Ren::eBufType::Texture;
        desc.size = CellsBufChunkSize;
        cells_buf_ = builder.WriteBuffer(cells_buf, desc, Ren::eResState::CopyDst, Ren::eStageBits::Transfer, *this);
    }*/
    /*{ // create lights buffer
        RpBufDesc desc;
        desc.type = Ren::eBufType::Texture;
        desc.size = LightsBufChunkSize;
        lights_buf_ = builder.WriteBuffer(lights_buf, desc, Ren::eResState::CopyDst, Ren::eStageBits::Transfer, *this);
    }*/
    /*{ // create decals buffer
        RpBufDesc desc;
        desc.type = Ren::eBufType::Texture;
        desc.size = DecalsBufChunkSize;
        decals_buf_ = builder.WriteBuffer(decals_buf, desc, Ren::eResState::CopyDst, Ren::eStageBits::Transfer, *this);
    }*/
    /*{ // create items buffer
        RpBufDesc desc;
        desc.type = Ren::eBufType::Texture;
        desc.size = ItemsBufChunkSize;
        items_buf_ = builder.WriteBuffer(items_buf, desc, Ren::eResState::CopyDst, Ren::eStageBits::Transfer, *this);
    }*/
    /*{ // create uniform buffer
        RpBufDesc desc;
        desc.type = Ren::eBufType::Uniform;
        desc.size = SharedDataBlockSize;
        shared_data_buf_ =
            builder.WriteBuffer(shared_data_buf, desc, Ren::eResState::CopyDst, Ren::eStageBits::Transfer, *this);
    }*/
    /*{ // create atomic counter buffer
        RpBufDesc desc;
        desc.type = Ren::eBufType::Storage;
        desc.size = sizeof(uint32_t);
        atomic_cnt_buf_ =
            builder.WriteBuffer(atomic_counter_buf, desc, Ren::eResState::CopyDst, Ren::eStageBits::Transfer, *this);
    }*/
}

void RpUpdateBuffers::Execute(RpBuilder &builder) {
    Ren::Context &ctx = builder.ctx();

    /*RpAllocBuf &skin_transforms_buf = builder.GetWriteBuffer(skin_transforms_buf_);

    // Update bone transforms buffer
    if (skin_transforms_.count) {
        uint8_t *stage_mem = skin_transforms_stage_buf_->MapRange(
            Ren::BufMapWrite, ctx.backend_frame() * SkinTransformsBufChunkSize, SkinTransformsBufChunkSize);
        const uint32_t skin_transforms_mem_size = skin_transforms_.count * sizeof(SkinTransform);
        if (stage_mem) {
            memcpy(stage_mem, skin_transforms_.data, skin_transforms_mem_size);
            skin_transforms_stage_buf_->FlushMappedRange(
                0, skin_transforms_stage_buf_->AlignMapOffset(skin_transforms_mem_size));
            skin_transforms_stage_buf_->Unmap();
        } else {
            builder.log()->Error("RpUpdateBuffers: Failed to map skin transforms buffer!");
        }

        Ren::CopyBufferToBuffer(*skin_transforms_stage_buf_, ctx.backend_frame() * SkinTransformsBufChunkSize,
                                *skin_transforms_buf.ref, 0, skin_transforms_mem_size, ctx.current_cmd_buf());
    }*/

    /*RpAllocBuf &shape_keys_buf = builder.GetWriteBuffer(shape_keys_buf_);

    if (shape_keys_.count) {
        uint8_t *stage_mem = shape_keys_stage_buf_->MapRange(
            Ren::BufMapWrite, ctx.backend_frame() * ShapeKeysBufChunkSize, ShapeKeysBufChunkSize);
        const uint32_t shape_keys_mem_size = shape_keys_.count * sizeof(ShapeKeyData);
        if (stage_mem) {
            memcpy(stage_mem, shape_keys_.data, shape_keys_mem_size);
            shape_keys_stage_buf_->FlushMappedRange(0, shape_keys_stage_buf_->AlignMapOffset(shape_keys_mem_size));
            shape_keys_stage_buf_->Unmap();
        } else {
            builder.log()->Error("RpUpdateBuffers: Failed to map shape keys buffer!");
        }

        Ren::CopyBufferToBuffer(*shape_keys_stage_buf_, ctx.backend_frame() * ShapeKeysBufChunkSize,
                                *shape_keys_buf.ref, 0, shape_keys_mem_size, ctx.current_cmd_buf());
    }*/

    /*RpAllocBuf &instances_buf = builder.GetWriteBuffer(instances_buf_);
    if (!instances_buf.tbos[0]) {
        instances_buf.tbos[0] = ctx.CreateTexture1D("Instances TBO", instances_buf.ref, Ren::eTexFormat::RawRGBA32F, 0,
                                                    InstanceDataBufChunkSize);
    }

    // Update instance buffer
    if (instances_.count) {
        uint8_t *stage_mem = instances_stage_buf_->MapRange(
            Ren::BufMapWrite, ctx.backend_frame() * InstanceDataBufChunkSize, InstanceDataBufChunkSize);
        const uint32_t instance_mem_size = instances_.count * sizeof(InstanceData);
        if (stage_mem) {
            memcpy(stage_mem, instances_.data, instance_mem_size);
            instances_stage_buf_->FlushMappedRange(0, instances_stage_buf_->AlignMapOffset(instance_mem_size));
            instances_stage_buf_->Unmap();
        } else {
            builder.log()->Error("RpUpdateBuffers: Failed to map instance buffer!");
        }

        Ren::CopyBufferToBuffer(*instances_stage_buf_, ctx.backend_frame() * InstanceDataBufChunkSize,
                                *instances_buf.ref, 0, instance_mem_size, ctx.current_cmd_buf());
    }*/

    /*RpAllocBuf &instance_indices_buf = builder.GetWriteBuffer(instance_indices_buf_);
    if (!instance_indices_buf.tbos[0]) {
        instance_indices_buf.tbos[0] = ctx.CreateTexture1D("Instance Indices TBO", instance_indices_buf.ref,
                                                           Ren::eTexFormat::RawRG32UI, 0, InstanceIndicesBufChunkSize);
    }

    // Update instance indices buffer
    if (instance_indices_.count) {
        uint8_t *stage_mem = instance_indices_stage_buf_->MapRange(
            Ren::BufMapWrite, ctx.backend_frame() * InstanceIndicesBufChunkSize, InstanceIndicesBufChunkSize);
        const uint32_t instance_indices_mem_size = instance_indices_.count * sizeof(Ren::Vec2i);
        if (stage_mem) {
            memcpy(stage_mem, instance_indices_.data, instance_indices_mem_size);
            instance_indices_stage_buf_->FlushMappedRange(
                0, instance_indices_stage_buf_->AlignMapOffset(instance_indices_mem_size));
            instance_indices_stage_buf_->Unmap();
        } else {
            builder.log()->Error("RpUpdateBuffers: Failed to map instance indices buffer!");
        }

        Ren::CopyBufferToBuffer(*instance_indices_stage_buf_, ctx.backend_frame() * InstanceIndicesBufChunkSize,
                                *instance_indices_buf.ref, 0, instance_indices_mem_size, ctx.current_cmd_buf());
    }*/

    /*RpAllocBuf &cells_buf = builder.GetWriteBuffer(cells_buf_);

    if (!cells_buf.tbos[0]) {
        cells_buf.tbos[0] =
            ctx.CreateTexture1D("Cells TBO", cells_buf.ref, Ren::eTexFormat::RawRG32UI, 0, CellsBufChunkSize);
    }

    // Update cells buffer
    if (cells_.count) {
        uint8_t *stage_mem =
            cells_stage_buf_->MapRange(Ren::BufMapWrite, ctx.backend_frame() * CellsBufChunkSize, CellsBufChunkSize);
        const uint32_t cells_mem_size = cells_.count * sizeof(CellData);
        if (stage_mem) {
            memcpy(stage_mem, cells_.data, cells_mem_size);
            cells_stage_buf_->FlushMappedRange(0, cells_stage_buf_->AlignMapOffset(cells_mem_size));
            cells_stage_buf_->Unmap();
        } else {
            builder.log()->Error("RpUpdateBuffers: Failed to map cells buffer!");
        }

        Ren::CopyBufferToBuffer(*cells_stage_buf_, ctx.backend_frame() * CellsBufChunkSize, *cells_buf.ref, 0,
                                cells_mem_size, ctx.current_cmd_buf());
    }*/

    /*RpAllocBuf &lights_buf = builder.GetWriteBuffer(lights_buf_);

    if (!lights_buf.tbos[0]) { // Create buffer for lights information
        lights_buf.tbos[0] =
            ctx.CreateTexture1D("Lights TBO", lights_buf.ref, Ren::eTexFormat::RawRGBA32F, 0, LightsBufChunkSize);
    }

    // Update lights buffer
    if (light_sources_.count) {
        uint8_t *stage_mem =
            lights_stage_buf_->MapRange(Ren::BufMapWrite, ctx.backend_frame() * LightsBufChunkSize, LightsBufChunkSize);
        const uint32_t lights_mem_size = light_sources_.count * sizeof(LightSourceItem);
        if (stage_mem) {
            memcpy(stage_mem, light_sources_.data, lights_mem_size);
            lights_stage_buf_->FlushMappedRange(0, lights_stage_buf_->AlignMapOffset(lights_mem_size));
            lights_stage_buf_->Unmap();
        } else {
            builder.log()->Error("RpUpdateBuffers: Failed to map lights buffer!");
        }

        Ren::CopyBufferToBuffer(*lights_stage_buf_, ctx.backend_frame() * LightsBufChunkSize, *lights_buf.ref, 0,
                                lights_mem_size, ctx.current_cmd_buf());
    }*/

    /*RpAllocBuf &decals_buf = builder.GetWriteBuffer(decals_buf_);

    if (!decals_buf.tbos[0]) {
        decals_buf.tbos[0] =
            ctx.CreateTexture1D("Decals TBO", decals_buf.ref, Ren::eTexFormat::RawRGBA32F, 0, DecalsBufChunkSize);
    }

    // Update decals buffer
    if (decals_.count) {
        uint8_t *stage_mem =
            decals_stage_buf_->MapRange(Ren::BufMapWrite, ctx.backend_frame() * DecalsBufChunkSize, DecalsBufChunkSize);
        const uint32_t decals_mem_size = decals_.count * sizeof(DecalItem);
        if (stage_mem) {
            memcpy(stage_mem, decals_.data, decals_mem_size);
            decals_stage_buf_->FlushMappedRange(0, decals_stage_buf_->AlignMapOffset(decals_mem_size));
            decals_stage_buf_->Unmap();
        } else {
            builder.log()->Error("RpUpdateBuffers: Failed to map decals buffer!");
        }

        Ren::CopyBufferToBuffer(*decals_stage_buf_, ctx.backend_frame() * DecalsBufChunkSize, *decals_buf.ref, 0,
                                decals_mem_size, ctx.current_cmd_buf());
    }*/

    /*RpAllocBuf &items_buf = builder.GetWriteBuffer(items_buf_);

    if (!items_buf.tbos[0]) {
        items_buf.tbos[0] =
            ctx.CreateTexture1D("Items TBO", items_buf.ref, Ren::eTexFormat::RawRG32UI, 0, ItemsBufChunkSize);
    }

    // Update items buffer
    if (items_.count) {
        uint8_t *stage_mem =
            items_stage_buf_->MapRange(Ren::BufMapWrite, ctx.backend_frame() * ItemsBufChunkSize, ItemsBufChunkSize);
        const uint32_t items_mem_size = items_.count * sizeof(ItemData);
        if (stage_mem) {
            memcpy(stage_mem, items_.data, items_mem_size);
            items_stage_buf_->FlushMappedRange(0, items_stage_buf_->AlignMapOffset(items_mem_size));
            items_stage_buf_->Unmap();
        } else {
            builder.log()->Error("RpUpdateBuffers: Failed to map items buffer!");
        }

        Ren::CopyBufferToBuffer(*items_stage_buf_, ctx.backend_frame() * ItemsBufChunkSize, *items_buf.ref, 0,
                                items_mem_size, ctx.current_cmd_buf());
    } else {
        uint8_t *stage_mem =
            items_stage_buf_->MapRange(Ren::BufMapWrite, ctx.backend_frame() * ItemsBufChunkSize, ItemsBufChunkSize);
        if (stage_mem) {
            ItemData dummy = {};
            memcpy(stage_mem, &dummy, sizeof(ItemData));
            items_stage_buf_->FlushMappedRange(0, items_stage_buf_->AlignMapOffset(sizeof(ItemData)));
            items_stage_buf_->Unmap();
        } else {
            builder.log()->Error("RpUpdateBuffers: Failed to map items buffer!");
        }

        Ren::CopyBufferToBuffer(*items_stage_buf_, ctx.backend_frame() * ItemsBufChunkSize, *items_buf.ref, 0,
                                sizeof(ItemData), ctx.current_cmd_buf());
    }*/

    //
    // Update UBO with data that is shared between passes
    //
    /*RpAllocBuf &unif_shared_data_buf = builder.GetWriteBuffer(shared_data_buf_);

    { // Prepare data that is shared for all instances
        SharedDataBlock shrd_data;

        shrd_data.view_matrix = draw_cam_->view_matrix();
        shrd_data.proj_matrix = draw_cam_->proj_matrix();

        shrd_data.uTaaInfo[0] = draw_cam_->px_offset()[0];
#if defined(USE_VK_RENDER)
        shrd_data.uTaaInfo[1] = -draw_cam_->px_offset()[1];
#else
        shrd_data.uTaaInfo[1] = draw_cam_->px_offset()[1];
#endif
        shrd_data.uTaaInfo[2] = reinterpret_cast<const float &>(view_state_->frame_index);

        { // Ray Tracing Gems II, Listing 49-1
            const Ren::Plane &l = draw_cam_->frustum_plane(Ren::eCamPlane::Left);
            const Ren::Plane &r = draw_cam_->frustum_plane(Ren::eCamPlane::Right);
            const Ren::Plane &b = draw_cam_->frustum_plane(Ren::eCamPlane::Bottom);
            const Ren::Plane &t = draw_cam_->frustum_plane(Ren::eCamPlane::Top);

            const float x0 = l.n[2] / l.n[0];
            const float x1 = r.n[2] / r.n[0];
            const float y0 = b.n[2] / b.n[1];
            const float y1 = t.n[2] / t.n[1];

            // View space position from screen space uv [0, 1]
            //          ray.xy = (uFrustumInfo.zw * uv + uFrustumInfo.xy) * mix(zDistanceNeg, -1.0, bIsOrtho)
            //          ray.z = 1.0 * zDistanceNeg

            shrd_data.uFrustumInfo[0] = -x0;
            shrd_data.uFrustumInfo[1] = -y0;
            shrd_data.uFrustumInfo[2] = x0 - x1;
            shrd_data.uFrustumInfo[3] = y0 - y1;
        }

        shrd_data.proj_matrix[2][0] += draw_cam_->px_offset()[0];
        shrd_data.proj_matrix[2][1] += draw_cam_->px_offset()[1];

        shrd_data.view_proj_matrix = shrd_data.proj_matrix * shrd_data.view_matrix;
        shrd_data.view_proj_prev_matrix = view_state_->prev_clip_from_world;
        shrd_data.inv_view_matrix = Ren::Inverse(shrd_data.view_matrix);
        shrd_data.inv_proj_matrix = Ren::Inverse(shrd_data.proj_matrix);
        shrd_data.inv_view_proj_matrix = Ren::Inverse(shrd_data.view_proj_matrix);
        // delta matrix between current and previous frame
        shrd_data.delta_matrix =
            view_state_->prev_clip_from_view * (view_state_->down_buf_view_from_world * shrd_data.inv_view_matrix);

        if (shadow_regions_.count) {
            assert(shadow_regions_.count <= REN_MAX_SHADOWMAPS_TOTAL);
            memcpy(&shrd_data.shadowmap_regions[0], &shadow_regions_.data[0],
                   sizeof(ShadowMapRegion) * shadow_regions_.count);
        }

        shrd_data.sun_dir = Ren::Vec4f{env_->sun_dir[0], env_->sun_dir[1], env_->sun_dir[2], 0.0f};
        shrd_data.sun_col = Ren::Vec4f{env_->sun_col[0], env_->sun_col[1], env_->sun_col[2], 0.0f};

        // actual resolution and full resolution
        shrd_data.res_and_fres = Ren::Vec4f{float(view_state_->act_res[0]), float(view_state_->act_res[1]),
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

        shrd_data.transp_params_and_time =
            Ren::Vec4f{std::log(transparent_near), std::log(transparent_far) - std::log(transparent_near),
                       float(transparent_mode), time_s};
        shrd_data.clip_info = Ren::Vec4f{near * far, near, far, std::log2(1.0f + far / near)};
        view_state_->clip_info = shrd_data.clip_info;

        const Ren::Vec3f &cam_pos = draw_cam_->world_position();
        const Ren::Vec3f cam_delta = cam_pos - view_state_->prev_cam_pos;
        shrd_data.cam_delta = Ren::Vec4f{cam_delta[0], cam_delta[1], cam_delta[2], 0.0f};
        shrd_data.cam_pos_and_gamma = Ren::Vec4f{cam_pos[0], cam_pos[1], cam_pos[2], 2.2f};
        shrd_data.wind_scroll = Ren::Vec4f{env_->curr_wind_scroll_lf[0], env_->curr_wind_scroll_lf[1],
                                           env_->curr_wind_scroll_hf[0], env_->curr_wind_scroll_hf[1]};
        shrd_data.wind_scroll_prev = Ren::Vec4f{env_->prev_wind_scroll_lf[0], env_->prev_wind_scroll_lf[1],
                                                env_->prev_wind_scroll_hf[0], env_->prev_wind_scroll_hf[1]};

        memcpy(&shrd_data.probes[0], probes_.data, sizeof(ProbeItem) * probes_.count);
        memcpy(&shrd_data.ellipsoids[0], ellipsoids_.data, sizeof(EllipsItem) * ellipsoids_.count);

        uint8_t *stage_mem =
            shared_data_stage_buf_->MapRange(Ren::BufMapWrite, ctx.backend_frame() * SharedDataBlockSize,
                                             shared_data_stage_buf_->AlignMapOffset(sizeof(SharedDataBlock)));
        if (stage_mem) {
            memcpy(stage_mem, &shrd_data, sizeof(SharedDataBlock));
            shared_data_stage_buf_->FlushMappedRange(0,
                                                     shared_data_stage_buf_->AlignMapOffset(sizeof(SharedDataBlock)));
            shared_data_stage_buf_->Unmap();
        }

        Ren::CopyBufferToBuffer(*shared_data_stage_buf_, ctx.backend_frame() * SharedDataBlockSize,
                                *unif_shared_data_buf.ref, 0, SharedDataBlockSize, ctx.current_cmd_buf());
    }*/

    //RpAllocBuf &atomic_cnt_buf = builder.GetWriteBuffer(atomic_cnt_buf_);
    //Ren::FillBuffer(*atomic_cnt_buf.ref, 0, sizeof(uint32_t), 0, ctx.current_cmd_buf());
}
#endif