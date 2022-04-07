#include "Renderer.h"

#include <Ren/Context.h>

#include "../Utils/ShaderLoader.h"

#include "../assets/shaders/internal/gbuffer_shade_interface.glsl"

void Renderer::InitPipelines() {
    { // Init skinning pipeline
        Ren::ProgramRef skinning_prog = sh_.LoadProgram(ctx_, "skinning_prog", "internal/skinning.comp.glsl");
        assert(skinning_prog->ready());

        if (!pi_skinning_.Init(ctx_.api_ctx(), std::move(skinning_prog), ctx_.log())) {
            ctx_.log()->Error("Renderer: failed to initialize pipeline!");
        }
    }
    { // Init gbuffer shading pipeline
        Ren::ProgramRef gbuf_shade_prog = sh_.LoadProgram(ctx_, "gbuffer_shade", "internal/gbuffer_shade.comp.glsl");
        assert(gbuf_shade_prog->ready());

        if (!pi_gbuf_shade_.Init(ctx_.api_ctx(), std::move(gbuf_shade_prog), ctx_.log())) {
            ctx_.log()->Error("Renderer: failed to initialize pipeline!");
        }
    }
    { // Quad classification for SSR
        Ren::ProgramRef classify_tiles_prog =
            sh_.LoadProgram(ctx_, "ssr_classify_tiles", "internal/ssr_classify_tiles.comp.glsl");
        assert(classify_tiles_prog->ready());

        if (!pi_ssr_classify_tiles_.Init(ctx_.api_ctx(), std::move(classify_tiles_prog), ctx_.log())) {
            ctx_.log()->Error("Renderer: failed to initialize pipeline!");
        }
    }
    { // Indirect dispatch arguments preparation for SSR
        Ren::ProgramRef write_indirect_prog =
            sh_.LoadProgram(ctx_, "ssr_write_indirect_args", "internal/ssr_write_indirect_args.comp.glsl");
        assert(write_indirect_prog->ready());

        if (!pi_ssr_write_indirect_.Init(ctx_.api_ctx(), std::move(write_indirect_prog), ctx_.log())) {
            ctx_.log()->Error("Renderer: failed to initialize pipeline!");
        }
    }
    { // HQ screen-space ray tracing
        Ren::ProgramRef ssr_trace_hq_prog = sh_.LoadProgram(ctx_, "ssr_trace_hq", "internal/ssr_trace_hq.comp.glsl");
        assert(ssr_trace_hq_prog->ready());

        if (!pi_ssr_trace_hq_.Init(ctx_.api_ctx(), std::move(ssr_trace_hq_prog), ctx_.log())) {
            ctx_.log()->Error("Renderer: failed to initialize pipeline!");
        }
    }
    { // RT dispatch arguments preparation for reflections
        Ren::ProgramRef write_indirect_prog =
            sh_.LoadProgram(ctx_, "ssr_write_indir_rt_dispatch", "internal/ssr_write_indir_rt_dispatch.comp.glsl");
        assert(write_indirect_prog->ready());

        if (!pi_rt_write_indirect_.Init(ctx_.api_ctx(), std::move(write_indirect_prog), ctx_.log())) {
            ctx_.log()->Error("Renderer: failed to initialize pipeline!");
        }
    }
    { // Reflections reprojection
        Ren::ProgramRef ssr_reproject_prog = sh_.LoadProgram(ctx_, "ssr_reproject", "internal/ssr_reproject.comp.glsl");
        assert(ssr_reproject_prog->ready());

        if (!pi_ssr_reproject_.Init(ctx_.api_ctx(), std::move(ssr_reproject_prog), ctx_.log())) {
            ctx_.log()->Error("Renderer: failed to initialize pipeline!");
        }
    }
    { // Reflections prefilter
        Ren::ProgramRef ssr_prefilter_prog = sh_.LoadProgram(ctx_, "ssr_prefilter", "internal/ssr_prefilter.comp.glsl");
        assert(ssr_prefilter_prog->ready());

        if (!pi_ssr_prefilter_.Init(ctx_.api_ctx(), std::move(ssr_prefilter_prog), ctx_.log())) {
            ctx_.log()->Error("Renderer: failed to initialize pipeline!");
        }
    }
    { // Reflections accumulation
        Ren::ProgramRef prog = sh_.LoadProgram(ctx_, "ssr_resolve_temporal", "internal/ssr_resolve_temporal.comp.glsl");
        assert(prog->ready());

        if (!pi_ssr_resolve_temporal_.Init(ctx_.api_ctx(), std::move(prog), ctx_.log())) {
            ctx_.log()->Error("Renderer: failed to initialize pipeline!");
        }
    }
}

void Renderer::AddBuffersUpdatePass(const DrawList &list, CommonBuffers &common_buffers) {
    auto &update_bufs = rp_builder_.AddPass("UPDATE BUFFERS");

    { // create skin transforms buffer
        RpBufDesc desc;
        desc.type = Ren::eBufType::Texture;
        desc.size = SkinTransformsBufChunkSize;
        common_buffers.skin_transforms_res = update_bufs.AddTransferOutput(SKIN_TRANSFORMS_BUF, desc);
    }
    { // create shape keys buffer
        RpBufDesc desc;
        desc.type = Ren::eBufType::Texture;
        desc.size = ShapeKeysBufChunkSize;
        common_buffers.shape_keys_res = update_bufs.AddTransferOutput(SHAPE_KEYS_BUF, desc);
    }
    { // create instances buffer
        RpBufDesc desc;
        desc.type = Ren::eBufType::Texture;
        desc.size = InstanceDataBufChunkSize;
        common_buffers.instances_res = update_bufs.AddTransferOutput(INSTANCES_BUF, desc);
    }
    { // create instance indices buffer
        RpBufDesc desc;
        desc.type = Ren::eBufType::Texture;
        desc.size = InstanceIndicesBufChunkSize;
        common_buffers.instance_indices_res = update_bufs.AddTransferOutput(INSTANCE_INDICES_BUF, desc);
    }
    { // create cells buffer
        RpBufDesc desc;
        desc.type = Ren::eBufType::Texture;
        desc.size = CellsBufChunkSize;
        common_buffers.cells_res = update_bufs.AddTransferOutput(CELLS_BUF, desc);
    }
    { // create lights buffer
        RpBufDesc desc;
        desc.type = Ren::eBufType::Texture;
        desc.size = LightsBufChunkSize;
        common_buffers.lights_res = update_bufs.AddTransferOutput(LIGHTS_BUF, desc);
    }
    { // create decals buffer
        RpBufDesc desc;
        desc.type = Ren::eBufType::Texture;
        desc.size = DecalsBufChunkSize;
        common_buffers.decals_res = update_bufs.AddTransferOutput(DECALS_BUF, desc);
    }
    { // create items buffer
        RpBufDesc desc;
        desc.type = Ren::eBufType::Texture;
        desc.size = ItemsBufChunkSize;
        common_buffers.items_res = update_bufs.AddTransferOutput(ITEMS_BUF, desc);
    }
    { // create uniform buffer
        RpBufDesc desc;
        desc.type = Ren::eBufType::Uniform;
        desc.size = SharedDataBlockSize;
        common_buffers.shared_data_res = update_bufs.AddTransferOutput(SHARED_DATA_BUF, desc);
    }
    { // create atomic counter buffer
        RpBufDesc desc;
        desc.type = Ren::eBufType::Storage;
        desc.size = sizeof(uint32_t);
        common_buffers.atomic_cnt_res = update_bufs.AddTransferOutput(ATOMIC_CNT_BUF, desc);
    }

    update_bufs.set_execute_cb([this, &common_buffers, &list](RpBuilder &builder) {
        Ren::Context &ctx = builder.ctx();
        RpAllocBuf &skin_transforms_buf = builder.GetWriteBuffer(common_buffers.skin_transforms_res);
        RpAllocBuf &shape_keys_buf = builder.GetWriteBuffer(common_buffers.shape_keys_res);
        RpAllocBuf &instances_buf = builder.GetWriteBuffer(common_buffers.instances_res);
        RpAllocBuf &instance_indices_buf = builder.GetWriteBuffer(common_buffers.instance_indices_res);
        RpAllocBuf &cells_buf = builder.GetWriteBuffer(common_buffers.cells_res);
        RpAllocBuf &lights_buf = builder.GetWriteBuffer(common_buffers.lights_res);
        RpAllocBuf &decals_buf = builder.GetWriteBuffer(common_buffers.decals_res);
        RpAllocBuf &items_buf = builder.GetWriteBuffer(common_buffers.items_res);
        RpAllocBuf &shared_data_buf = builder.GetWriteBuffer(common_buffers.shared_data_res);
        RpAllocBuf &atomic_cnt_buf = builder.GetWriteBuffer(common_buffers.atomic_cnt_res);

        Ren::UpdateBufferContents(list.skin_transforms.data, list.skin_transforms.count * sizeof(SkinTransform),
                                  *list.skin_transforms_stage_buf, ctx.backend_frame() * SkinTransformsBufChunkSize,
                                  SkinTransformsBufChunkSize, *skin_transforms_buf.ref, 0, ctx.current_cmd_buf());

        Ren::UpdateBufferContents(list.shape_keys_data.data, list.shape_keys_data.count * sizeof(ShapeKeyData),
                                  *list.shape_keys_stage_buf, ctx.backend_frame() * ShapeKeysBufChunkSize,
                                  ShapeKeysBufChunkSize, *shape_keys_buf.ref, 0, ctx.current_cmd_buf());

        if (!instances_buf.tbos[0]) {
            instances_buf.tbos[0] = ctx.CreateTexture1D("Instances TBO", instances_buf.ref, Ren::eTexFormat::RawRGBA32F,
                                                        0, InstanceDataBufChunkSize);
        }

        Ren::UpdateBufferContents(list.instances.data, list.instances.count * sizeof(InstanceData),
                                  *list.instances_stage_buf, ctx.backend_frame() * InstanceDataBufChunkSize,
                                  InstanceDataBufChunkSize, *instances_buf.ref, 0, ctx.current_cmd_buf());

        if (!instance_indices_buf.tbos[0]) {
            instance_indices_buf.tbos[0] =
                ctx.CreateTexture1D("Instance Indices TBO", instance_indices_buf.ref, Ren::eTexFormat::RawRG32UI, 0,
                                    InstanceIndicesBufChunkSize);
        }

        Ren::UpdateBufferContents(list.instance_indices.data, list.instance_indices.count * sizeof(Ren::Vec2i),
                                  *list.instance_indices_stage_buf, ctx.backend_frame() * InstanceIndicesBufChunkSize,
                                  InstanceIndicesBufChunkSize, *instance_indices_buf.ref, 0, ctx.current_cmd_buf());

        if (!cells_buf.tbos[0]) {
            cells_buf.tbos[0] =
                ctx.CreateTexture1D("Cells TBO", cells_buf.ref, Ren::eTexFormat::RawRG32UI, 0, CellsBufChunkSize);
        }

        Ren::UpdateBufferContents(list.cells.data, list.cells.count * sizeof(CellData), *list.cells_stage_buf,
                                  ctx.backend_frame() * CellsBufChunkSize, CellsBufChunkSize, *cells_buf.ref, 0,
                                  ctx.current_cmd_buf());

        if (!lights_buf.tbos[0]) {
            lights_buf.tbos[0] =
                ctx.CreateTexture1D("Lights TBO", lights_buf.ref, Ren::eTexFormat::RawRGBA32F, 0, LightsBufChunkSize);
        }

        Ren::UpdateBufferContents(list.lights.data, list.lights.count * sizeof(LightItem), *list.lights_stage_buf,
                                  ctx.backend_frame() * LightsBufChunkSize, LightsBufChunkSize, *lights_buf.ref, 0,
                                  ctx.current_cmd_buf());

        if (!decals_buf.tbos[0]) {
            decals_buf.tbos[0] =
                ctx.CreateTexture1D("Decals TBO", decals_buf.ref, Ren::eTexFormat::RawRGBA32F, 0, DecalsBufChunkSize);
        }

        Ren::UpdateBufferContents(list.decals.data, list.decals.count * sizeof(DecalItem), *list.decals_stage_buf,
                                  ctx.backend_frame() * DecalsBufChunkSize, DecalsBufChunkSize, *decals_buf.ref, 0,
                                  ctx.current_cmd_buf());

        if (!items_buf.tbos[0]) {
            items_buf.tbos[0] =
                ctx.CreateTexture1D("Items TBO", items_buf.ref, Ren::eTexFormat::RawRG32UI, 0, ItemsBufChunkSize);
        }

        if (list.items.count) {
            Ren::UpdateBufferContents(list.items.data, list.items.count * sizeof(ItemData), *list.items_stage_buf,
                                      ctx.backend_frame() * ItemsBufChunkSize, ItemsBufChunkSize, *items_buf.ref, 0,
                                      ctx.current_cmd_buf());
        } else {
            const ItemData dummy = {};
            Ren::UpdateBufferContents(&dummy, sizeof(ItemData), *list.items_stage_buf,
                                      ctx.backend_frame() * ItemsBufChunkSize, ItemsBufChunkSize, *items_buf.ref, 0,
                                      ctx.current_cmd_buf());
        }

        { // Prepare data that is shared for all instances
            SharedDataBlock shrd_data;

            shrd_data.view_matrix = list.draw_cam.view_matrix();
            shrd_data.proj_matrix = list.draw_cam.proj_matrix();

            shrd_data.uTaaInfo[0] = list.draw_cam.px_offset()[0];
#if defined(USE_VK_RENDER)
            shrd_data.uTaaInfo[1] = -list.draw_cam.px_offset()[1];
#else
            shrd_data.uTaaInfo[1] = list.draw_cam.px_offset()[1];
#endif
            shrd_data.uTaaInfo[2] = reinterpret_cast<const float &>(view_state_.frame_index);

            { // Ray Tracing Gems II, Listing 49-1
                const Ren::Plane &l = list.draw_cam.frustum_plane(Ren::eCamPlane::Left);
                const Ren::Plane &r = list.draw_cam.frustum_plane(Ren::eCamPlane::Right);
                const Ren::Plane &b = list.draw_cam.frustum_plane(Ren::eCamPlane::Bottom);
                const Ren::Plane &t = list.draw_cam.frustum_plane(Ren::eCamPlane::Top);

                const float x0 = l.n[2] / l.n[0];
                const float x1 = r.n[2] / r.n[0];
                const float y0 = b.n[2] / b.n[1];
                const float y1 = t.n[2] / t.n[1];

                // View space position from screen space uv [0, 1]
                //          ray.xy = (uFrustumInfo.zw * uv + uFrustumInfo.xy) * mix(zDistanceNeg, -1.0,
                //          bIsOrtho) ray.z = 1.0 * zDistanceNeg

                shrd_data.uFrustumInfo[0] = -x0;
                shrd_data.uFrustumInfo[1] = -y0;
                shrd_data.uFrustumInfo[2] = x0 - x1;
                shrd_data.uFrustumInfo[3] = y0 - y1;
            }

            shrd_data.proj_matrix[2][0] += list.draw_cam.px_offset()[0];
            shrd_data.proj_matrix[2][1] += list.draw_cam.px_offset()[1];

            shrd_data.view_proj_matrix = shrd_data.proj_matrix * shrd_data.view_matrix;
            shrd_data.view_proj_prev_matrix = view_state_.prev_clip_from_world;
            shrd_data.inv_view_matrix = Ren::Inverse(shrd_data.view_matrix);
            shrd_data.inv_proj_matrix = Ren::Inverse(shrd_data.proj_matrix);
            shrd_data.inv_view_proj_matrix = Ren::Inverse(shrd_data.view_proj_matrix);
            // delta matrix between current and previous frame
            shrd_data.delta_matrix =
                view_state_.prev_clip_from_view * (view_state_.down_buf_view_from_world * shrd_data.inv_view_matrix);

            if (list.shadow_regions.count) {
                assert(list.shadow_regions.count <= REN_MAX_SHADOWMAPS_TOTAL);
                memcpy(&shrd_data.shadowmap_regions[0], &list.shadow_regions.data[0],
                       sizeof(ShadowMapRegion) * list.shadow_regions.count);
            }

            shrd_data.sun_dir = Ren::Vec4f{list.env.sun_dir[0], list.env.sun_dir[1], list.env.sun_dir[2], 0.0f};
            shrd_data.sun_col = Ren::Vec4f{list.env.sun_col[0], list.env.sun_col[1], list.env.sun_col[2], 0.0f};

            // actual resolution and full resolution
            shrd_data.res_and_fres = Ren::Vec4f{float(view_state_.act_res[0]), float(view_state_.act_res[1]),
                                                float(view_state_.scr_res[0]), float(view_state_.scr_res[1])};

            const float near = list.draw_cam.near(), far = list.draw_cam.far();
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
            view_state_.clip_info = shrd_data.clip_info;

            const Ren::Vec3f &cam_pos = list.draw_cam.world_position();
            const Ren::Vec3f cam_delta = cam_pos - view_state_.prev_cam_pos;
            shrd_data.cam_delta = Ren::Vec4f{cam_delta[0], cam_delta[1], cam_delta[2], 0.0f};
            shrd_data.cam_pos_and_gamma = Ren::Vec4f{cam_pos[0], cam_pos[1], cam_pos[2], 2.2f};
            shrd_data.wind_scroll = Ren::Vec4f{list.env.curr_wind_scroll_lf[0], list.env.curr_wind_scroll_lf[1],
                                               list.env.curr_wind_scroll_hf[0], list.env.curr_wind_scroll_hf[1]};
            shrd_data.wind_scroll_prev = Ren::Vec4f{list.env.prev_wind_scroll_lf[0], list.env.prev_wind_scroll_lf[1],
                                                    list.env.prev_wind_scroll_hf[0], list.env.prev_wind_scroll_hf[1]};

            memcpy(&shrd_data.probes[0], list.probes.data, sizeof(ProbeItem) * list.probes.count);
            memcpy(&shrd_data.ellipsoids[0], list.ellipsoids.data, sizeof(EllipsItem) * list.ellipsoids.count);

            Ren::UpdateBufferContents(&shrd_data, sizeof(SharedDataBlock), *list.shared_data_stage_buf,
                                      ctx.backend_frame() * SharedDataBlockSize, SharedDataBlockSize,
                                      *shared_data_buf.ref, 0, ctx.current_cmd_buf());
        }

        Ren::FillBuffer(*atomic_cnt_buf.ref, 0, sizeof(uint32_t), 0, ctx.current_cmd_buf());
    });
}

void Renderer::AddSkydomePass(const DrawList &list, const CommonBuffers &common_buffers, const bool clear,
                              FrameTextures &frame_textures) {
    if (list.env.env_map) {
        auto &skymap = rp_builder_.AddPass("SKYDOME");
        RpResRef shared_data_buf = skymap.AddUniformBufferInput(
            common_buffers.shared_data_res, Ren::eStageBits::VertexShader | Ren::eStageBits::FragmentShader);
        RpResRef env_tex = skymap.AddTextureInput(list.env.env_map, Ren::eStageBits::FragmentShader);
        RpResRef vtx_buf1 = skymap.AddVertexBufferInput(ctx_.default_vertex_buf1());
        RpResRef vtx_buf2 = skymap.AddVertexBufferInput(ctx_.default_vertex_buf2());
        RpResRef ndx_buf = skymap.AddIndexBufferInput(ctx_.default_indices_buf());

        frame_textures.color = skymap.AddColorOutput(MAIN_COLOR_TEX, frame_textures.color_params);
        frame_textures.specular = skymap.AddColorOutput(MAIN_SPEC_TEX, frame_textures.specular_params);
        frame_textures.depth = skymap.AddDepthOutput(MAIN_DEPTH_TEX, frame_textures.depth_params);

        rp_skydome_.Setup(list, &view_state_, clear, vtx_buf1, vtx_buf2, ndx_buf, shared_data_buf, env_tex,
                          frame_textures.color, frame_textures.specular, frame_textures.depth);
        skymap.set_executor(&rp_skydome_);
    } else {
        // TODO: Physical sky
    }
}

void Renderer::AddGBufferFillPass(const DrawList &list, const CommonBuffers &common_buffers,
                                  const PersistentGpuData &persistent_data, const BindlessTextureData &bindless,
                                  FrameTextures &frame_textures) {
    auto &gbuf_fill = rp_builder_.AddPass("GBUFFER FILL");
    const RpResRef vtx_buf1 = gbuf_fill.AddVertexBufferInput(ctx_.default_vertex_buf1());
    const RpResRef vtx_buf2 = gbuf_fill.AddVertexBufferInput(ctx_.default_vertex_buf2());
    const RpResRef ndx_buf = gbuf_fill.AddIndexBufferInput(ctx_.default_indices_buf());

    const RpResRef materials_buf =
        gbuf_fill.AddStorageReadonlyInput(persistent_data.materials_buf, Ren::eStageBits::VertexShader);
#if defined(USE_GL_RENDER)
    const RpResRef textures_buf =
        gbuf_fill.AddStorageReadonlyInput(bindless.textures_buf, Ren::eStageBits::VertexShader);
#else
    const RpResRef textures_buf = {};
#endif

    const RpResRef noise_tex =
        gbuf_fill.AddTextureInput(noise_tex_, Ren::eStageBits::VertexShader | Ren::eStageBits::FragmentShader);
    const RpResRef dummy_black = gbuf_fill.AddTextureInput(dummy_black_, Ren::eStageBits::FragmentShader);

    const RpResRef instances_buf =
        gbuf_fill.AddStorageReadonlyInput(common_buffers.instances_res, Ren::eStageBits::VertexShader);
    const RpResRef instances_indices_buf =
        gbuf_fill.AddStorageReadonlyInput(common_buffers.instance_indices_res, Ren::eStageBits::VertexShader);

    const RpResRef shared_data_buf = gbuf_fill.AddUniformBufferInput(
        common_buffers.shared_data_res, Ren::eStageBits::VertexShader | Ren::eStageBits::FragmentShader);

    const RpResRef cells_buf =
        gbuf_fill.AddStorageReadonlyInput(common_buffers.cells_res, Ren::eStageBits::FragmentShader);
    const RpResRef items_buf =
        gbuf_fill.AddStorageReadonlyInput(common_buffers.items_res, Ren::eStageBits::FragmentShader);
    const RpResRef decals_buf =
        gbuf_fill.AddStorageReadonlyInput(common_buffers.decals_res, Ren::eStageBits::FragmentShader);

    frame_textures.albedo = gbuf_fill.AddColorOutput(MAIN_ALBEDO_TEX, frame_textures.albedo_params);
    frame_textures.normal = gbuf_fill.AddColorOutput(MAIN_NORMAL_TEX, frame_textures.normal_params);
    frame_textures.specular = gbuf_fill.AddColorOutput(MAIN_SPEC_TEX, frame_textures.specular_params);
    frame_textures.depth = gbuf_fill.AddDepthOutput(MAIN_DEPTH_TEX, frame_textures.depth_params);

    rp_gbuffer_fill_.Setup(list, &view_state_, vtx_buf1, vtx_buf2, ndx_buf, materials_buf, textures_buf, &bindless,
                           noise_tex, dummy_black, instances_buf, instances_indices_buf, shared_data_buf, cells_buf,
                           items_buf, decals_buf, frame_textures.albedo, frame_textures.normal, frame_textures.specular,
                           frame_textures.depth);
    gbuf_fill.set_executor(&rp_gbuffer_fill_);
}

void Renderer::AddForwardOpaquePass(const DrawList &list, const CommonBuffers &common_buffers,
                                    const PersistentGpuData &persistent_data, const BindlessTextureData &bindless,
                                    FrameTextures &frame_textures) {
    auto &opaque = rp_builder_.AddPass("OPAQUE");
    const RpResRef vtx_buf1 = opaque.AddVertexBufferInput(ctx_.default_vertex_buf1());
    const RpResRef vtx_buf2 = opaque.AddVertexBufferInput(ctx_.default_vertex_buf2());
    const RpResRef ndx_buf = opaque.AddIndexBufferInput(ctx_.default_indices_buf());

    const RpResRef materials_buf =
        opaque.AddStorageReadonlyInput(persistent_data.materials_buf, Ren::eStageBits::VertexShader);
#if defined(USE_GL_RENDER)
    const RpResRef textures_buf =
        opaque.AddStorageReadonlyInput(bindless.textures_buf, Ren::eStageBits::VertexShader);
#else
    const RpResRef textures_buf = {};
#endif
    const RpResRef brdf_lut = opaque.AddTextureInput(brdf_lut_, Ren::eStageBits::FragmentShader);
    const RpResRef noise_tex =
        opaque.AddTextureInput(noise_tex_, Ren::eStageBits::VertexShader | Ren::eStageBits::FragmentShader);
    const RpResRef cone_rt_lut = opaque.AddTextureInput(cone_rt_lut_, Ren::eStageBits::FragmentShader);

    const RpResRef dummy_black = opaque.AddTextureInput(dummy_black_, Ren::eStageBits::FragmentShader);
    const RpResRef dummy_white = opaque.AddTextureInput(dummy_white_, Ren::eStageBits::FragmentShader);

    const RpResRef instances_buf =
        opaque.AddStorageReadonlyInput(common_buffers.instances_res, Ren::eStageBits::VertexShader);
    const RpResRef instances_indices_buf =
        opaque.AddStorageReadonlyInput(common_buffers.instance_indices_res, Ren::eStageBits::VertexShader);

    const RpResRef shader_data_buf = opaque.AddUniformBufferInput(
        common_buffers.shared_data_res, Ren::eStageBits::VertexShader | Ren::eStageBits::FragmentShader);

    const RpResRef cells_buf =
        opaque.AddStorageReadonlyInput(common_buffers.cells_res, Ren::eStageBits::FragmentShader);
    const RpResRef items_buf =
        opaque.AddStorageReadonlyInput(common_buffers.items_res, Ren::eStageBits::FragmentShader);
    const RpResRef lights_buf =
        opaque.AddStorageReadonlyInput(common_buffers.lights_res, Ren::eStageBits::FragmentShader);
    const RpResRef decals_buf =
        opaque.AddStorageReadonlyInput(common_buffers.decals_res, Ren::eStageBits::FragmentShader);

    const RpResRef shadowmap_tex = opaque.AddTextureInput(frame_textures.shadowmap, Ren::eStageBits::FragmentShader);
    const RpResRef ssao_tex = opaque.AddTextureInput(frame_textures.ssao, Ren::eStageBits::FragmentShader);

    RpResRef lmap_tex[4];
    for (int i = 0; i < 4; ++i) {
        if (list.env.lm_indir_sh[i]) {
            lmap_tex[i] = opaque.AddTextureInput(list.env.lm_indir_sh[i], Ren::eStageBits::FragmentShader);
        }
    }

    frame_textures.color = opaque.AddColorOutput(MAIN_COLOR_TEX, frame_textures.color_params);
    frame_textures.normal = opaque.AddColorOutput(MAIN_NORMAL_TEX, frame_textures.normal_params);
    frame_textures.specular = opaque.AddColorOutput(MAIN_SPEC_TEX, frame_textures.specular_params);
    frame_textures.depth = opaque.AddDepthOutput(MAIN_DEPTH_TEX, frame_textures.depth_params);

    rp_opaque_.Setup(list, &view_state_, vtx_buf1, vtx_buf2, ndx_buf, materials_buf, textures_buf,
                     persistent_data.pipelines.data(), &bindless, brdf_lut, noise_tex, cone_rt_lut, dummy_black,
                     dummy_white, instances_buf, instances_indices_buf, shader_data_buf, cells_buf, items_buf,
                     lights_buf, decals_buf, shadowmap_tex, ssao_tex, lmap_tex, frame_textures.color,
                     frame_textures.normal, frame_textures.specular, frame_textures.depth);
    opaque.set_executor(&rp_opaque_);
}

void Renderer::AddForwardTransparentPass(const DrawList &list, const CommonBuffers &common_buffers,
                                         const PersistentGpuData &persistent_data, const BindlessTextureData &bindless,
                                         FrameTextures &frame_textures) {
    if (list.alpha_blend_start_index == -1) {
        // There is no transparent objects in the scene
        return;
    }
    auto &transparent = rp_builder_.AddPass("TRANSPARENT");
    const RpResRef vtx_buf1 = transparent.AddVertexBufferInput(ctx_.default_vertex_buf1());
    const RpResRef vtx_buf2 = transparent.AddVertexBufferInput(ctx_.default_vertex_buf2());
    const RpResRef ndx_buf = transparent.AddIndexBufferInput(ctx_.default_indices_buf());

    const RpResRef materials_buf =
        transparent.AddStorageReadonlyInput(persistent_data.materials_buf, Ren::eStageBits::VertexShader);
#if defined(USE_GL_RENDER)
    const RpResRef textures_buf =
        transparent.AddStorageReadonlyInput(bindless.textures_buf, Ren::eStageBits::VertexShader);
#else
    const RpResRef textures_buf = {};
#endif
    const RpResRef brdf_lut = transparent.AddTextureInput(brdf_lut_, Ren::eStageBits::FragmentShader);
    const RpResRef noise_tex =
        transparent.AddTextureInput(noise_tex_, Ren::eStageBits::VertexShader | Ren::eStageBits::FragmentShader);
    const RpResRef cone_rt_lut = transparent.AddTextureInput(cone_rt_lut_, Ren::eStageBits::FragmentShader);

    const RpResRef dummy_black = transparent.AddTextureInput(dummy_black_, Ren::eStageBits::FragmentShader);
    const RpResRef dummy_white = transparent.AddTextureInput(dummy_white_, Ren::eStageBits::FragmentShader);

    const RpResRef instances_buf =
        transparent.AddStorageReadonlyInput(common_buffers.instances_res, Ren::eStageBits::VertexShader);
    const RpResRef instances_indices_buf =
        transparent.AddStorageReadonlyInput(common_buffers.instance_indices_res, Ren::eStageBits::VertexShader);

    const RpResRef shader_data_buf = transparent.AddUniformBufferInput(
        common_buffers.shared_data_res, Ren::eStageBits::VertexShader | Ren::eStageBits::FragmentShader);

    const RpResRef cells_buf =
        transparent.AddStorageReadonlyInput(common_buffers.cells_res, Ren::eStageBits::FragmentShader);
    const RpResRef items_buf =
        transparent.AddStorageReadonlyInput(common_buffers.items_res, Ren::eStageBits::FragmentShader);
    const RpResRef lights_buf =
        transparent.AddStorageReadonlyInput(common_buffers.lights_res, Ren::eStageBits::FragmentShader);
    const RpResRef decals_buf =
        transparent.AddStorageReadonlyInput(common_buffers.decals_res, Ren::eStageBits::FragmentShader);

    const RpResRef shadowmap_tex =
        transparent.AddTextureInput(frame_textures.shadowmap, Ren::eStageBits::FragmentShader);
    const RpResRef ssao_tex = transparent.AddTextureInput(frame_textures.ssao, Ren::eStageBits::FragmentShader);

    RpResRef lmap_tex[4];
    for (int i = 0; i < 4; ++i) {
        if (list.env.lm_indir_sh[i]) {
            lmap_tex[i] = transparent.AddTextureInput(list.env.lm_indir_sh[i], Ren::eStageBits::FragmentShader);
        }
    }

    frame_textures.color = transparent.AddColorOutput(MAIN_COLOR_TEX, frame_textures.color_params);
    frame_textures.normal = transparent.AddColorOutput(MAIN_NORMAL_TEX, frame_textures.normal_params);
    frame_textures.specular = transparent.AddColorOutput(MAIN_SPEC_TEX, frame_textures.specular_params);
    frame_textures.depth = transparent.AddDepthOutput(MAIN_DEPTH_TEX, frame_textures.depth_params);

    rp_transparent_.Setup(list, &view_state_, vtx_buf1, vtx_buf2, ndx_buf, materials_buf, textures_buf,
                          persistent_data.pipelines.data(), &bindless, brdf_lut, noise_tex, cone_rt_lut, dummy_black,
                          dummy_white, instances_buf, instances_indices_buf, shader_data_buf, cells_buf, items_buf,
                          lights_buf, decals_buf, shadowmap_tex, ssao_tex, lmap_tex, frame_textures.color,
                          frame_textures.normal, frame_textures.specular, frame_textures.depth);
    transparent.set_executor(&rp_transparent_);
}

void Renderer::AddDeferredShadingPass(const CommonBuffers &common_buffers, FrameTextures &frame_textures) {
    auto &gbuf_shade = rp_builder_.AddPass("GBUFFER SHADE");

    struct PassData {
        RpResRef shared_data;
        RpResRef cells_buf, items_buf, lights_buf, decals_buf;
        RpResRef shadowmap_tex, ssao_tex;
        RpResRef depth_tex, albedo_tex, normal_tex, spec_tex;
        RpResRef output_tex;
    };

    auto *data = gbuf_shade.AllocPassData<PassData>();
    data->shared_data =
        gbuf_shade.AddUniformBufferInput(common_buffers.shared_data_res, Ren::eStageBits::ComputeShader);

    data->cells_buf = gbuf_shade.AddStorageReadonlyInput(common_buffers.cells_res, Ren::eStageBits::ComputeShader);
    data->items_buf = gbuf_shade.AddStorageReadonlyInput(common_buffers.items_res, Ren::eStageBits::ComputeShader);
    data->lights_buf = gbuf_shade.AddStorageReadonlyInput(common_buffers.lights_res, Ren::eStageBits::ComputeShader);
    data->decals_buf = gbuf_shade.AddStorageReadonlyInput(common_buffers.decals_res, Ren::eStageBits::ComputeShader);

    data->shadowmap_tex = gbuf_shade.AddTextureInput(frame_textures.shadowmap, Ren::eStageBits::ComputeShader);
    data->ssao_tex = gbuf_shade.AddTextureInput(frame_textures.ssao, Ren::eStageBits::ComputeShader);

    data->depth_tex = gbuf_shade.AddTextureInput(frame_textures.depth, Ren::eStageBits::ComputeShader);
    data->albedo_tex = gbuf_shade.AddTextureInput(frame_textures.albedo, Ren::eStageBits::ComputeShader);
    data->normal_tex = gbuf_shade.AddTextureInput(frame_textures.normal, Ren::eStageBits::ComputeShader);
    data->spec_tex = gbuf_shade.AddTextureInput(frame_textures.specular, Ren::eStageBits::ComputeShader);

    frame_textures.color = data->output_tex =
        gbuf_shade.AddStorageImageOutput(MAIN_COLOR_TEX, frame_textures.color_params, Ren::eStageBits::ComputeShader);

    gbuf_shade.set_execute_cb([this, data](RpBuilder &builder) {
        RpAllocBuf &unif_shared_data_buf = builder.GetReadBuffer(data->shared_data);
        RpAllocBuf &cells_buf = builder.GetReadBuffer(data->cells_buf);
        RpAllocBuf &items_buf = builder.GetReadBuffer(data->items_buf);
        RpAllocBuf &lights_buf = builder.GetReadBuffer(data->lights_buf);
        RpAllocBuf &decals_buf = builder.GetReadBuffer(data->decals_buf);

        RpAllocTex &depth_tex = builder.GetReadTexture(data->depth_tex);
        RpAllocTex &albedo_tex = builder.GetReadTexture(data->albedo_tex);
        RpAllocTex &normal_tex = builder.GetReadTexture(data->normal_tex);
        RpAllocTex &spec_tex = builder.GetReadTexture(data->spec_tex);

        RpAllocTex &shad_tex = builder.GetReadTexture(data->shadowmap_tex);
        RpAllocTex &ssao_tex = builder.GetReadTexture(data->ssao_tex);

        RpAllocTex &out_color_tex = builder.GetWriteTexture(data->output_tex);

        const Ren::Binding bindings[] = {
            {Ren::eBindTarget::UBuf, REN_UB_SHARED_DATA_LOC, *unif_shared_data_buf.ref},
            {Ren::eBindTarget::TBuf, GBufferShade::CELLS_BUF_SLOT, *cells_buf.tbos[0]},
            {Ren::eBindTarget::TBuf, GBufferShade::ITEMS_BUF_SLOT, *items_buf.tbos[0]},
            {Ren::eBindTarget::TBuf, GBufferShade::LIGHT_BUF_SLOT, *lights_buf.tbos[0]},
            {Ren::eBindTarget::TBuf, GBufferShade::DECAL_BUF_SLOT, *decals_buf.tbos[0]},
            {Ren::eBindTarget::Tex2D, GBufferShade::DEPTH_TEX_SLOT, *depth_tex.ref},
            {Ren::eBindTarget::Tex2D, GBufferShade::ALBEDO_TEX_SLOT, *albedo_tex.ref},
            {Ren::eBindTarget::Tex2D, GBufferShade::NORMAL_TEX_SLOT, *normal_tex.ref},
            {Ren::eBindTarget::Tex2D, GBufferShade::SPECULAR_TEX_SLOT, *spec_tex.ref},
            {Ren::eBindTarget::Tex2D, GBufferShade::SHADOW_TEX_SLOT, *shad_tex.ref},
            {Ren::eBindTarget::Tex2D, GBufferShade::SSAO_TEX_SLOT, *ssao_tex.ref},
            {Ren::eBindTarget::Image, GBufferShade::OUT_COLOR_IMG_SLOT, *out_color_tex.ref}};

        const Ren::Vec3u grp_count = Ren::Vec3u{
            (view_state_.act_res[0] + GBufferShade::LOCAL_GROUP_SIZE_X - 1u) / GBufferShade::LOCAL_GROUP_SIZE_X,
            (view_state_.act_res[1] + GBufferShade::LOCAL_GROUP_SIZE_Y - 1u) / GBufferShade::LOCAL_GROUP_SIZE_Y, 1u};

        GBufferShade::Params uniform_params;
        uniform_params.img_size = Ren::Vec2u{uint32_t(view_state_.act_res[0]), uint32_t(view_state_.act_res[1])};

        Ren::DispatchCompute(pi_gbuf_shade_, grp_count, bindings, COUNT_OF(bindings), &uniform_params,
                             sizeof(uniform_params), builder.ctx().default_descr_alloc(), builder.ctx().log());
    });
}

void Renderer::AddSSAOPasses(const RpResRef depth_down_2x, const RpResRef _depth_tex, RpResRef &out_ssao) {
    const Ren::Vec4i cur_res =
        Ren::Vec4i{view_state_.act_res[0], view_state_.act_res[1], view_state_.scr_res[0], view_state_.scr_res[1]};

    RpResRef ssao_raw;
    { // Main SSAO pass
        auto &ssao = rp_builder_.AddPass("SSAO");
        const RpResRef rand_tex = ssao.AddTextureInput(rand2d_dirs_4x4_, Ren::eStageBits::FragmentShader);
        const RpResRef depth_tex = ssao.AddTextureInput(depth_down_2x, Ren::eStageBits::FragmentShader);

        { // Allocate output texture
            Ren::Tex2DParams params;
            params.w = view_state_.scr_res[0] / 2;
            params.h = view_state_.scr_res[1] / 2;
            params.format = Ren::eTexFormat::RawR8;
            params.usage = (Ren::eTexUsage::Sampled | Ren::eTexUsage::RenderTarget);
            params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

            ssao_raw = ssao.AddColorOutput(SSAO_RAW, params);
        }

        rp_ssao_.Setup(rp_builder_, &view_state_, rand_tex, depth_tex, ssao_raw);
        ssao.set_executor(&rp_ssao_);
    }

    RpResRef ssao_blurred1;
    { // Horizontal SSAO blur
        auto &ssao_blur_h = rp_builder_.AddPass("SSAO BLUR H");
        const RpResRef depth_tex = ssao_blur_h.AddTextureInput(depth_down_2x, Ren::eStageBits::FragmentShader);
        const RpResRef ssao_tex = ssao_blur_h.AddTextureInput(ssao_raw, Ren::eStageBits::FragmentShader);

        { // Allocate output texture
            Ren::Tex2DParams params;
            params.w = view_state_.scr_res[0] / 2;
            params.h = view_state_.scr_res[1] / 2;
            params.format = Ren::eTexFormat::RawR8;
            params.usage = (Ren::eTexUsage::Sampled | Ren::eTexUsage::RenderTarget);
            params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

            ssao_blurred1 = ssao_blur_h.AddColorOutput("SSAO BLUR TEMP1", params);
        }

        rp_ssao_blur_h_.Setup(view_state_.act_res / 2, false /* vertical */, depth_tex, ssao_tex, ssao_blurred1);
        ssao_blur_h.set_executor(&rp_ssao_blur_h_);
    }

    RpResRef ssao_blurred2;
    { // Vertical SSAO blur
        auto &ssao_blur_v = rp_builder_.AddPass("SSAO BLUR V");
        const RpResRef depth_tex = ssao_blur_v.AddTextureInput(depth_down_2x, Ren::eStageBits::FragmentShader);
        const RpResRef ssao_tex = ssao_blur_v.AddTextureInput(ssao_blurred1, Ren::eStageBits::FragmentShader);

        { // Allocate output texture
            Ren::Tex2DParams params;
            params.w = view_state_.scr_res[0] / 2;
            params.h = view_state_.scr_res[1] / 2;
            params.format = Ren::eTexFormat::RawR8;
            params.usage = (Ren::eTexUsage::Sampled | Ren::eTexUsage::RenderTarget);
            params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

            ssao_blurred2 = ssao_blur_v.AddColorOutput("SSAO BLUR TEMP2", params);
        }

        rp_ssao_blur_v_.Setup(view_state_.act_res / 2, true /* vertical */, depth_tex, ssao_tex, ssao_blurred2);
        ssao_blur_v.set_executor(&rp_ssao_blur_v_);
    }

    { // Upscale SSAO pass
        auto &ssao_upscale = rp_builder_.AddPass("UPSCALE");
        const RpResRef depth_down_2x_res =
            ssao_upscale.AddTextureInput(depth_down_2x, Ren::eStageBits::FragmentShader);
        const RpResRef depth_tex = ssao_upscale.AddTextureInput(_depth_tex, Ren::eStageBits::FragmentShader);
        const RpResRef ssao_tex = ssao_upscale.AddTextureInput(ssao_blurred2, Ren::eStageBits::FragmentShader);

        { // Allocate output texture
            Ren::Tex2DParams params;
            params.w = view_state_.act_res[0];
            params.h = view_state_.act_res[1];
            params.format = Ren::eTexFormat::RawR8;
            params.usage = (Ren::eTexUsage::Sampled | Ren::eTexUsage::RenderTarget);
            params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

            out_ssao = ssao_upscale.AddColorOutput(SSAO_RES, params);
        }

        rp_ssao_upscale_.Setup(cur_res, view_state_.clip_info, depth_down_2x_res, depth_tex, ssao_tex, out_ssao);
        ssao_upscale.set_executor(&rp_ssao_upscale_);
    }
}
