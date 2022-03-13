#include "RpGBufferFill.h"

#include <Ren/Context.h>

#include "../../Utils/ShaderLoader.h"
#include "../Renderer_Structs.h"

void RpGBufferFill::Setup(RpBuilder &builder, const DrawList &list, const ViewState *view_state,
                          const Ren::BufferRef &vtx_buf1, const Ren::BufferRef &vtx_buf2, const Ren::BufferRef &ndx_buf,
                          const Ren::BufferRef &materials_buf, const BindlessTextureData *bindless_tex,
                          const Ren::Tex2DRef &noise_tex, const Ren::Tex2DRef &dummy_black, const char instances_buf[],
                          const char instance_indices_buf[], const char shared_data_buf[], const char cells_buf[],
                          const char items_buf[], const char decals_buf[], const char out_albedo[],
                          const char out_normals[], const char out_spec[], const char out_depth[]) {
    view_state_ = view_state;
    bindless_tex_ = bindless_tex;

    materials_ = list.materials;
    decals_atlas_ = list.decals_atlas;

    render_flags_ = list.render_flags;
    main_batches_ = list.basic_batches;
    main_batch_indices_ = list.basic_batch_indices;

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
    decals_buf_ =
        builder.ReadBuffer(decals_buf, Ren::eResState::ShaderResource, Ren::eStageBits::FragmentShader, *this);
    materials_buf_ =
        builder.ReadBuffer(materials_buf, Ren::eResState::ShaderResource, Ren::eStageBits::VertexShader, *this);

    noise_tex_ = builder.ReadTexture(noise_tex, Ren::eResState::ShaderResource,
                                     Ren::eStageBits::VertexShader | Ren::eStageBits::FragmentShader, *this);
    dummy_black_ =
        builder.ReadTexture(dummy_black, Ren::eResState::ShaderResource, Ren::eStageBits::FragmentShader, *this);

#if defined(USE_GL_RENDER)
    if (bindless_tex->textures_buf) {
        textures_buf_ = builder.ReadBuffer(bindless_tex->textures_buf, Ren::eResState::ShaderResource,
                                           Ren::eStageBits::VertexShader, *this);
    }
#endif

    { // 4-component albedo (alpha is unused)
        Ren::Tex2DParams params;
        params.w = view_state->scr_res[0];
        params.h = view_state->scr_res[1];
        params.format = Ren::eTexFormat::RawRGBA8888;
        params.flags = Ren::TexSRGB;
        params.usage = (Ren::eTexUsage::Transfer | Ren::eTexUsage::Sampled | Ren::eTexUsage::RenderTarget);
        params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

        out_albedo_tex_ = builder.WriteTexture(out_albedo, params, Ren::eResState::RenderTarget,
                                               Ren::eStageBits::ColorAttachment, *this);
    }
    { // 4-component world-space normal (alpha is roughness)
        Ren::Tex2DParams params;
        params.w = view_state->scr_res[0];
        params.h = view_state->scr_res[1];
        params.format = Ren::eTexFormat::RawRGBA8888;
        params.usage = (Ren::eTexUsage::Transfer | Ren::eTexUsage::Sampled | Ren::eTexUsage::RenderTarget);
        params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

        out_normal_tex_ = builder.WriteTexture(out_normals, params, Ren::eResState::RenderTarget,
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

        out_spec_tex_ = builder.WriteTexture(out_spec, params, Ren::eResState::RenderTarget,
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

        out_depth_tex_ = builder.WriteTexture(out_depth, params, Ren::eResState::DepthWrite,
                                              Ren::eStageBits::DepthAttachment, *this);
    }
}

void RpGBufferFill::Execute(RpBuilder &builder) {
    RpAllocBuf &vtx_buf1 = builder.GetReadBuffer(vtx_buf1_);
    RpAllocBuf &vtx_buf2 = builder.GetReadBuffer(vtx_buf2_);
    RpAllocBuf &ndx_buf = builder.GetReadBuffer(ndx_buf_);

    RpAllocTex &albedo_tex = builder.GetWriteTexture(out_albedo_tex_);
    RpAllocTex &normal_tex = builder.GetWriteTexture(out_normal_tex_);
    RpAllocTex &spec_tex = builder.GetWriteTexture(out_spec_tex_);
    RpAllocTex &depth_tex = builder.GetWriteTexture(out_depth_tex_);

    LazyInit(builder.ctx(), builder.sh(), vtx_buf1, vtx_buf2, ndx_buf, albedo_tex, normal_tex, spec_tex, depth_tex);
    DrawOpaque(builder);
}

void RpGBufferFill::LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocBuf &vtx_buf1, RpAllocBuf &vtx_buf2,
                             RpAllocBuf &ndx_buf, RpAllocTex &albedo_tex, RpAllocTex &normal_tex, RpAllocTex &spec_tex,
                             RpAllocTex &depth_tex) {
    const Ren::RenderTarget color_targets[] = {{albedo_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store},
                                               {normal_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store},
                                               {spec_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store}};
    const Ren::RenderTarget depth_target = {depth_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store, Ren::eLoadOp::Load,
                                            Ren::eStoreOp::Store};

    if (!initialized) {
        Ren::ProgramRef gbuf_simple_prog =
            sh.LoadProgram(ctx, "gbuffer_fill", "internal/gbuffer_fill.vert.glsl", "internal/gbuffer_fill.frag.glsl");
        assert(gbuf_simple_prog->ready());
        Ren::ProgramRef gbuf_vegetation_prog = sh.LoadProgram(
            ctx, "gbuffer_fill_vege", "internal/gbuffer_fill.vert.glsl@VEGETATION", "internal/gbuffer_fill.frag.glsl");
        assert(gbuf_vegetation_prog->ready());

        const bool res =
            rp_main_draw_.Setup(ctx.api_ctx(), color_targets, COUNT_OF(color_targets), depth_target, ctx.log());
        if (!res) {
            ctx.log()->Error("[RpGBufferFill::LazyInit]: Failed to initialize render pass!");
        }

        const int buf1_stride = 16, buf2_stride = 16;

        { // VAO for simple and skinned meshes
            const Ren::VtxAttribDesc attribs[] = {
                // Attributes from buffer 1
                {vtx_buf1.ref, REN_VTX_POS_LOC, 3, Ren::eType::Float32, buf1_stride, 0},
                {vtx_buf1.ref, REN_VTX_UV1_LOC, 2, Ren::eType::Float16, buf1_stride, 3 * sizeof(float)},
                // Attributes from buffer 2
                {vtx_buf2.ref, REN_VTX_NOR_LOC, 4, Ren::eType::Int16SNorm, buf2_stride, 0},
                {vtx_buf2.ref, REN_VTX_TAN_LOC, 2, Ren::eType::Int16SNorm, buf2_stride, 4 * sizeof(uint16_t)}};
            if (!vi_simple_.Setup(attribs, COUNT_OF(attribs), ndx_buf.ref)) {
                ctx.log()->Error("[RpGBufferFill::LazyInit]: vi_simple_ init failed!");
            }
        }

        { // VAO for vegetation meshes (uses additional vertex color attribute)
            const Ren::VtxAttribDesc attribs[] = {
                // Attributes from buffer 1
                {vtx_buf1.ref, REN_VTX_POS_LOC, 3, Ren::eType::Float32, buf1_stride, 0},
                {vtx_buf1.ref, REN_VTX_UV1_LOC, 2, Ren::eType::Float16, buf1_stride, 3 * sizeof(float)},
                // Attributes from buffer 2
                {vtx_buf2.ref, REN_VTX_NOR_LOC, 4, Ren::eType::Int16SNorm, buf2_stride, 0},
                {vtx_buf2.ref, REN_VTX_TAN_LOC, 2, Ren::eType::Int16SNorm, buf2_stride, 4 * sizeof(uint16_t)},
                {vtx_buf2.ref, REN_VTX_AUX_LOC, 1, Ren::eType::Uint32, buf2_stride, 6 * sizeof(uint16_t)}};
            if (!vi_vegetation_.Setup(attribs, COUNT_OF(attribs), ndx_buf.ref)) {
                ctx.log()->Error("[RpGBufferFill::LazyInit]: vi_vegetation_ init failed!");
            }
        }

        { // simple and skinned
            Ren::RastState rast_state;
            rast_state.depth.test_enabled = true;
            rast_state.depth.compare_op = uint8_t(Ren::eCompareOp::Equal);

            if (!pi_simple_[1].Init(ctx.api_ctx(), rast_state, gbuf_simple_prog, &vi_simple_, &rp_main_draw_,
                                    ctx.log())) {
                ctx.log()->Error("[RpGBufferFill::LazyInit]: Failed to initialize pipeline!");
            }

            rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

            if (!pi_simple_[0].Init(ctx.api_ctx(), rast_state, gbuf_simple_prog, &vi_simple_, &rp_main_draw_,
                                    ctx.log())) {
                ctx.log()->Error("[RpGBufferFill::LazyInit]: Failed to initialize pipeline!");
            }
        }

        { // vegetation
            Ren::RastState rast_state;
            rast_state.depth.test_enabled = true;
            rast_state.depth.compare_op = uint8_t(Ren::eCompareOp::Equal);

            if (!pi_vegetation_[1].Init(ctx.api_ctx(), rast_state, gbuf_vegetation_prog, &vi_vegetation_,
                                        &rp_main_draw_, ctx.log())) {
                ctx.log()->Error("[RpGBufferFill::LazyInit]: Failed to initialize pipeline!");
            }

            rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

            if (!pi_vegetation_[0].Init(ctx.api_ctx(), rast_state, gbuf_vegetation_prog, &vi_vegetation_,
                                        &rp_main_draw_, ctx.log())) {
                ctx.log()->Error("[RpGBufferFill::LazyInit]: Failed to initialize pipeline!");
            }
        }

        api_ctx_ = ctx.api_ctx();
#if defined(USE_VK_RENDER)
        InitDescrSetLayout();
#endif

        initialized = true;
    }

    if (!main_draw_fb_[ctx.backend_frame()].Setup(ctx.api_ctx(), rp_main_draw_, depth_tex.desc.w, depth_tex.desc.h,
                                                  depth_target, depth_target, color_targets, COUNT_OF(color_targets))) {
        ctx.log()->Error("[RpGBufferFill::LazyInit]: main_draw_fb_ init failed!");
    }
}