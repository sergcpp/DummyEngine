#include "RpOpaque.h"

#include <Ren/Context.h>

#include "../../Utils/ShaderLoader.h"
#include "../Renderer_Structs.h"

void RpOpaque::Setup(RpBuilder &builder, const DrawList &list, const ViewState *view_state,
                     const Ren::BufferRef &vtx_buf1, const Ren::BufferRef &vtx_buf2, const Ren::BufferRef &ndx_buf,
                     const Ren::BufferRef &materials_buf, const Ren::Pipeline pipelines[],
                     const BindlessTextureData *bindless_tex, const Ren::Tex2DRef &brdf_lut,
                     const Ren::Tex2DRef &noise_tex, const Ren::Tex2DRef &cone_rt_lut, const Ren::Tex2DRef &dummy_black,
                     const Ren::Tex2DRef &dummy_white, const char instances_buf[], const char instance_indices_buf[],
                     const char shared_data_buf[], const char cells_buf[], const char items_buf[],
                     const char lights_buf[], const char decals_buf[], const char shadowmap_tex[],
                     const char ssao_tex[], const char out_color[], const char out_normals[], const char out_spec[],
                     const char out_depth[]) {
    view_state_ = view_state;
    pipelines_ = pipelines;
    bindless_tex_ = bindless_tex;

    materials_ = list.materials;
    decals_atlas_ = list.decals_atlas;
    probe_storage_ = list.probe_storage;

    render_flags_ = list.render_flags;
    main_batches_ = list.custom_batches;
    main_batch_indices_ = list.custom_batch_indices;

    vtx_buf1_ = builder.ReadBuffer(vtx_buf1, Ren::eResState::VertexBuffer, Ren::eStageBits::VertexInput, *this);
    vtx_buf2_ = builder.ReadBuffer(vtx_buf2, Ren::eResState::VertexBuffer, Ren::eStageBits::VertexInput, *this);
    ndx_buf_ = builder.ReadBuffer(ndx_buf, Ren::eResState::IndexBuffer, Ren::eStageBits::VertexInput, *this);
    instances_buf_ =
        builder.ReadBuffer(instances_buf, Ren::eResState::ShaderResource, Ren::eStageBits::VertexShader, *this);
    instance_indices_buf_ =
        builder.ReadBuffer(instance_indices_buf, Ren::eResState::ShaderResource, Ren::eStageBits::VertexShader, *this);
    shared_data_buf_ = builder.ReadBuffer(shared_data_buf, Ren::eResState::UniformBuffer,
                                          Ren::eStageBits::VertexShader | Ren::eStageBits::FragmentShader, *this);
    cells_buf_ = builder.ReadBuffer(cells_buf, Ren::eResState::ShaderResource, Ren::eStageBits::FragmentShader, *this);
    items_buf_ = builder.ReadBuffer(items_buf, Ren::eResState::ShaderResource, Ren::eStageBits::FragmentShader, *this);
    lights_buf_ =
        builder.ReadBuffer(lights_buf, Ren::eResState::ShaderResource, Ren::eStageBits::FragmentShader, *this);
    decals_buf_ =
        builder.ReadBuffer(decals_buf, Ren::eResState::ShaderResource, Ren::eStageBits::FragmentShader, *this);
    shad_tex_ =
        builder.ReadTexture(shadowmap_tex, Ren::eResState::ShaderResource, Ren::eStageBits::FragmentShader, *this);

    materials_buf_ =
        builder.ReadBuffer(materials_buf, Ren::eResState::ShaderResource, Ren::eStageBits::VertexShader, *this);
    if ((render_flags_ & (EnableZFill | EnableSSAO)) == (EnableZFill | EnableSSAO)) {
        ssao_tex_ =
            builder.ReadTexture(ssao_tex, Ren::eResState::ShaderResource, Ren::eStageBits::FragmentShader, *this);
    } else {
        ssao_tex_ = {};
    }

    for (int i = 0; i < 4; ++i) {
        if (list.env.lm_indir_sh[i]) {
            lm_tex_[i] = builder.ReadTexture(list.env.lm_indir_sh[i], Ren::eResState::ShaderResource,
                                             Ren::eStageBits::FragmentShader, *this);
        } else {
            lm_tex_[i] = {};
        }
    }

    brdf_lut_ = builder.ReadTexture(brdf_lut, Ren::eResState::ShaderResource, Ren::eStageBits::FragmentShader, *this);
    noise_tex_ = builder.ReadTexture(noise_tex, Ren::eResState::ShaderResource,
                                     Ren::eStageBits::VertexShader | Ren::eStageBits::FragmentShader, *this);
    cone_rt_lut_ =
        builder.ReadTexture(cone_rt_lut, Ren::eResState::ShaderResource, Ren::eStageBits::FragmentShader, *this);

    dummy_black_ =
        builder.ReadTexture(dummy_black, Ren::eResState::ShaderResource, Ren::eStageBits::FragmentShader, *this);
    dummy_white_ =
        builder.ReadTexture(dummy_white, Ren::eResState::ShaderResource, Ren::eStageBits::FragmentShader, *this);

#if defined(USE_GL_RENDER)
    textures_buf_ = builder.ReadBuffer(bindless_tex->textures_buf, Ren::eResState::ShaderResource,
                                       Ren::eStageBits::VertexShader, *this);
#endif

    { // Main color
        Ren::Tex2DParams params;
        params.w = view_state->scr_res[0];
        params.h = view_state->scr_res[1];
#if (REN_OIT_MODE == REN_OIT_WEIGHTED_BLENDED) || (REN_OIT_MODE == REN_OIT_MOMENT_BASED && REN_OIT_MOMENT_RENORMALIZE)
        // renormalization requires buffer with alpha channel
        params.format = Ren::eTexFormat::RawRGBA16F;
#else
        params.format = Ren::eTexFormat::RawRG11F_B10F;
#endif
        // Ren::eTexUsage::Storage is needed for rt debugging, consider removing this
        params.usage = (Ren::eTexUsage::Storage | Ren::eTexUsage::Sampled | Ren::eTexUsage::RenderTarget);
        params.sampling.wrap = Ren::eTexWrap::ClampToEdge;
        params.samples = view_state->is_multisampled ? 4 : 1;

        color_tex_ = builder.WriteTexture(out_color, params, Ren::eResState::RenderTarget,
                                          Ren::eStageBits::ColorAttachment, *this);
    }
    { // 4-component world-space normal (alpha is roughness)
        Ren::Tex2DParams params;
        params.w = view_state->scr_res[0];
        params.h = view_state->scr_res[1];
        params.format = Ren::eTexFormat::RawRGBA8888;
        params.usage = (Ren::eTexUsage::Transfer | Ren::eTexUsage::Sampled | Ren::eTexUsage::RenderTarget);
        params.sampling.wrap = Ren::eTexWrap::ClampToEdge;
        params.samples = view_state->is_multisampled ? 4 : 1;

        normal_tex_ = builder.WriteTexture(out_normals, params, Ren::eResState::RenderTarget,
                                           Ren::eStageBits::ColorAttachment, *this);
    }
    { // 4-component specular
        Ren::Tex2DParams params;
        params.w = view_state->scr_res[0];
        params.h = view_state->scr_res[1];
        params.format = Ren::eTexFormat::RawRGBA8888;
        params.flags = Ren::TexSRGB;
        params.usage = (Ren::eTexUsage::Sampled | Ren::eTexUsage::RenderTarget);
        params.sampling.wrap = Ren::eTexWrap::ClampToEdge;
        params.samples = view_state->is_multisampled ? 4 : 1;

        spec_tex_ = builder.WriteTexture(out_spec, params, Ren::eResState::RenderTarget,
                                         Ren::eStageBits::ColorAttachment, *this);
    }
    { // 24/32-bit depth
        Ren::Tex2DParams params;
        params.w = view_state->scr_res[0];
        params.h = view_state->scr_res[1];
        params.format = builder.ctx().capabilities.depth24_stencil8_format ? Ren::eTexFormat::Depth24Stencil8
                                                                           : Ren::eTexFormat::Depth32Stencil8;
        params.usage = (Ren::eTexUsage::Transfer | Ren::eTexUsage::Sampled | Ren::eTexUsage::RenderTarget);
        params.sampling.wrap = Ren::eTexWrap::ClampToEdge;
        params.samples = view_state->is_multisampled ? 4 : 1;

        depth_tex_ = builder.WriteTexture(out_depth, params, Ren::eResState::DepthWrite,
                                          Ren::eStageBits::DepthAttachment, *this);
    }
}

void RpOpaque::Execute(RpBuilder &builder) {
    RpAllocBuf &vtx_buf1 = builder.GetReadBuffer(vtx_buf1_);
    RpAllocBuf &vtx_buf2 = builder.GetReadBuffer(vtx_buf2_);
    RpAllocBuf &ndx_buf = builder.GetReadBuffer(ndx_buf_);

    RpAllocTex &color_tex = builder.GetWriteTexture(color_tex_);
    RpAllocTex &normal_tex = builder.GetWriteTexture(normal_tex_);
    RpAllocTex &spec_tex = builder.GetWriteTexture(spec_tex_);
    RpAllocTex &depth_tex = builder.GetWriteTexture(depth_tex_);

    LazyInit(builder.ctx(), builder.sh(), vtx_buf1, vtx_buf2, ndx_buf, color_tex, normal_tex, spec_tex, depth_tex);
    DrawOpaque(builder);
}

void RpOpaque::LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocBuf &vtx_buf1, RpAllocBuf &vtx_buf2,
                        RpAllocBuf &ndx_buf, RpAllocTex &color_tex, RpAllocTex &normal_tex, RpAllocTex &spec_tex,
                        RpAllocTex &depth_tex) {
    const Ren::RenderTarget color_targets[] = {{color_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store},
                                               {normal_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store},
                                               {spec_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store}};
    const Ren::RenderTarget depth_target = {depth_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store, Ren::eLoadOp::Load,
                                            Ren::eStoreOp::Store};

    if (!initialized) {
        if (!rp_opaque_.Setup(ctx.api_ctx(), color_targets, 3, depth_target, ctx.log())) {
            ctx.log()->Error("[RpOpaque::LazyInit]: Failed to init render pass!");
        }

        api_ctx_ = ctx.api_ctx();
#if defined(USE_VK_RENDER)
        InitDescrSetLayout();
#endif

        initialized = true;
    }

    const int buf1_stride = 16, buf2_stride = 16;

    { // VertexInput for main drawing (uses all attributes)
        const Ren::VtxAttribDesc attribs[] = {
            // Attributes from buffer 1
            {vtx_buf1.ref, REN_VTX_POS_LOC, 3, Ren::eType::Float32, buf1_stride, 0},
            {vtx_buf1.ref, REN_VTX_UV1_LOC, 2, Ren::eType::Float16, buf1_stride, 3 * sizeof(float)},
            // Attributes from buffer 2
            {vtx_buf2.ref, REN_VTX_NOR_LOC, 4, Ren::eType::Int16SNorm, buf2_stride, 0},
            {vtx_buf2.ref, REN_VTX_TAN_LOC, 2, Ren::eType::Int16SNorm, buf2_stride, 4 * sizeof(uint16_t)},
            {vtx_buf2.ref, REN_VTX_AUX_LOC, 1, Ren::eType::Uint32, buf2_stride, 6 * sizeof(uint16_t)}};

        draw_pass_vi_.Setup(attribs, COUNT_OF(attribs), ndx_buf.ref);
    }

    if (!opaque_draw_fb_[ctx.backend_frame()].Setup(ctx.api_ctx(), rp_opaque_, depth_tex.desc.w, depth_tex.desc.h,
                                                    depth_target, depth_target, color_targets,
                                                    COUNT_OF(color_targets))) {
        ctx.log()->Error("RpOpaque: opaque_draw_fb_ init failed!");
    }
}