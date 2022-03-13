#include "RpShadowMaps.h"

#include <Ren/Context.h>

#include "../../Utils/ShaderLoader.h"
#include "../Renderer_Structs.h"

void RpShadowMaps::Setup(RpBuilder &builder, const DrawList &list, const Ren::BufferRef &vtx_buf1,
                         const Ren::BufferRef &vtx_buf2, const Ren::BufferRef &ndx_buf,
                         const Ren::BufferRef &materials_buf, const BindlessTextureData *bindless_tex,
                         const char instances_buf[], const char instance_indices_buf[], const char shared_data_buf[],
                         const char shadowmap_tex[], const Ren::Tex2DRef &noise_tex) {
    materials_ = list.materials;
    bindless_tex_ = bindless_tex;
    shadow_batches_ = list.shadow_batches;
    shadow_batch_indices_ = list.shadow_batch_indices;
    shadow_lists_ = list.shadow_lists;
    shadow_regions_ = list.shadow_regions;

    vtx_buf1_ = builder.ReadBuffer(vtx_buf1, Ren::eResState::VertexBuffer, Ren::eStageBits::VertexInput, *this);
    vtx_buf2_ = builder.ReadBuffer(vtx_buf2, Ren::eResState::VertexBuffer, Ren::eStageBits::VertexInput, *this);
    ndx_buf_ = builder.ReadBuffer(ndx_buf, Ren::eResState::IndexBuffer, Ren::eStageBits::VertexInput, *this);
    instances_buf_ =
        builder.ReadBuffer(instances_buf, Ren::eResState::ShaderResource, Ren::eStageBits::VertexShader, *this);
    instance_indices_buf_ =
        builder.ReadBuffer(instance_indices_buf, Ren::eResState::ShaderResource, Ren::eStageBits::VertexShader, *this);
    shared_data_buf_ = builder.ReadBuffer(shared_data_buf, Ren::eResState::UniformBuffer,
                                          Ren::eStageBits::VertexShader | Ren::eStageBits::FragmentShader, *this);
    materials_buf_ =
        builder.ReadBuffer(materials_buf, Ren::eResState::ShaderResource, Ren::eStageBits::VertexShader, *this);
#if defined(USE_GL_RENDER)
    textures_buf_ = builder.ReadBuffer(bindless_tex->textures_buf, Ren::eResState::ShaderResource,
                                       Ren::eStageBits::VertexShader, *this);
#endif
    noise_tex_ =
        builder.ReadTexture(std::move(noise_tex), Ren::eResState::ShaderResource, Ren::eStageBits::VertexShader, *this);

    { // shadow map buffer
        Ren::Tex2DParams params;
        params.w = w_;
        params.h = h_;
        params.format = Ren::eTexFormat::Depth16;
        params.usage = (Ren::eTexUsage::Sampled | Ren::eTexUsage::RenderTarget);
        params.sampling.min_lod.from_float(0.0f);
        params.sampling.max_lod.from_float(0.0f);
        params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
        params.sampling.wrap = Ren::eTexWrap::ClampToEdge;
        params.sampling.compare = Ren::eTexCompare::LEqual;

        shadowmap_tex_ = builder.WriteTexture(shadowmap_tex, params, Ren::eResState::DepthWrite,
                                              /*Ren::eStageBits::Transfer |*/ Ren::eStageBits::DepthAttachment, *this);
    }
}

void RpShadowMaps::Execute(RpBuilder &builder) {
    RpAllocBuf &vtx_buf1 = builder.GetReadBuffer(vtx_buf1_);
    RpAllocBuf &vtx_buf2 = builder.GetReadBuffer(vtx_buf2_);
    RpAllocBuf &ndx_buf = builder.GetReadBuffer(ndx_buf_);

    RpAllocTex &shadowmap_tex = builder.GetWriteTexture(shadowmap_tex_);

    LazyInit(builder.ctx(), builder.sh(), vtx_buf1, vtx_buf2, ndx_buf, shadowmap_tex);
    DrawShadowMaps(builder, shadowmap_tex);
}

void RpShadowMaps::LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocBuf &vtx_buf1, RpAllocBuf &vtx_buf2,
                            RpAllocBuf &ndx_buf, RpAllocTex &shadowmap_tex) {
    const Ren::RenderTarget depth_target = {shadowmap_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store};

    if (!initialized) {
        Ren::ProgramRef shadow_solid_prog =
            sh.LoadProgram(ctx, "shadow_solid", "internal/shadow.vert.glsl", "internal/shadow.frag.glsl");
        assert(shadow_solid_prog->ready());
        Ren::ProgramRef shadow_vege_solid_prog =
            sh.LoadProgram(ctx, "shadow_vege_solid", "internal/shadow_vege.vert.glsl", "internal/shadow.frag.glsl");
        assert(shadow_vege_solid_prog->ready());
        Ren::ProgramRef shadow_transp_prog =
            sh.LoadProgram(ctx, "shadow_transp", "internal/shadow.vert.glsl@TRANSPARENT_PERM",
                           "internal/shadow.frag.glsl@TRANSPARENT_PERM");
        assert(shadow_transp_prog->ready());
        Ren::ProgramRef shadow_vege_transp_prog =
            sh.LoadProgram(ctx, "shadow_vege_transp", "internal/shadow_vege.vert.glsl@TRANSPARENT_PERM",
                           "internal/shadow.frag.glsl@TRANSPARENT_PERM");
        assert(shadow_vege_transp_prog->ready());

        if (!rp_depth_only_.Setup(ctx.api_ctx(), nullptr, 0, depth_target, ctx.log())) {
            ctx.log()->Error("[RpDepthFill::LazyInit]: Failed to init depth only pass!");
        }

        const int buf1_stride = 16, buf2_stride = 16;

        { // VAO for solid shadow pass (uses position attribute only)
            const Ren::VtxAttribDesc attribs[] = {
                {vtx_buf1.ref->handle(), REN_VTX_POS_LOC, 3, Ren::eType::Float32, buf1_stride, 0}};
            if (!vi_depth_pass_solid_.Setup(attribs, 1, ndx_buf.ref->handle())) {
                ctx.log()->Error("RpShadowMaps: vi_depth_pass_solid_ init failed!");
            }
        }

        { // VAO for solid shadow pass of vegetation (uses position and color attributes)
            const Ren::VtxAttribDesc attribs[] = {
                {vtx_buf1.ref->handle(), REN_VTX_POS_LOC, 3, Ren::eType::Float32, buf1_stride, 0},
                {vtx_buf2.ref->handle(), REN_VTX_AUX_LOC, 1, Ren::eType::Uint32, buf2_stride, 6 * sizeof(uint16_t)}};
            if (!vi_depth_pass_vege_solid_.Setup(attribs, 2, ndx_buf.ref->handle())) {
                ctx.log()->Error("RpShadowMaps: vi_depth_pass_vege_solid_ init failed!");
            }
        }

        { // VAO for alpha-tested shadow pass (uses position and uv attributes)
            const Ren::VtxAttribDesc attribs[] = {
                {vtx_buf1.ref->handle(), REN_VTX_POS_LOC, 3, Ren::eType::Float32, buf1_stride, 0},
                {vtx_buf1.ref->handle(), REN_VTX_UV1_LOC, 2, Ren::eType::Float16, buf1_stride, 3 * sizeof(float)}};
            if (!vi_depth_pass_transp_.Setup(attribs, 2, ndx_buf.ref->handle())) {
                ctx.log()->Error("RpShadowMaps: vi_depth_pass_transp_ init failed!");
            }
        }

        { // VAO for transparent shadow pass of vegetation (uses position, color and
          // uv attributes)
            const Ren::VtxAttribDesc attribs[] = {
                {vtx_buf1.ref->handle(), REN_VTX_POS_LOC, 3, Ren::eType::Float32, buf1_stride, 0},
                {vtx_buf1.ref->handle(), REN_VTX_UV1_LOC, 2, Ren::eType::Float16, buf1_stride, 3 * sizeof(float)},
                {vtx_buf2.ref->handle(), REN_VTX_AUX_LOC, 1, Ren::eType::Uint32, buf2_stride, 6 * sizeof(uint16_t)}};
            if (!vi_depth_pass_vege_transp_.Setup(attribs, 3, ndx_buf.ref->handle())) {
                ctx.log()->Error("RpShadowMaps: depth_pass_vege_transp_vao_ init failed!");
            }
        }

        { // solid/transp
            Ren::RastState rast_state;
            rast_state.poly.cull = uint8_t(Ren::eCullFace::None);
            rast_state.poly.depth_bias_mode = uint8_t(Ren::eDepthBiasMode::Dynamic);

            rast_state.depth.test_enabled = true;
            rast_state.depth.compare_op = unsigned(Ren::eCompareOp::Less);
            rast_state.scissor.enabled = true;

            if (!pi_solid_.Init(ctx.api_ctx(), rast_state, shadow_solid_prog, &vi_depth_pass_solid_, &rp_depth_only_,
                                ctx.log())) {
                ctx.log()->Error("[RpDepthFill::LazyInit]: Failed to initialize pipeline!");
            }

            if (!pi_transp_.Init(ctx.api_ctx(), rast_state, shadow_transp_prog, &vi_depth_pass_transp_, &rp_depth_only_,
                                 ctx.log())) {
                ctx.log()->Error("[RpDepthFill::LazyInit]: Failed to initialize pipeline!");
            }

            if (!pi_vege_solid_.Init(ctx.api_ctx(), rast_state, shadow_vege_solid_prog, &vi_depth_pass_vege_solid_,
                                     &rp_depth_only_, ctx.log())) {
                ctx.log()->Error("[RpDepthFill::LazyInit]: Failed to initialize pipeline!");
            }

            if (!pi_vege_transp_.Init(ctx.api_ctx(), rast_state, shadow_vege_transp_prog, &vi_depth_pass_vege_transp_,
                                      &rp_depth_only_, ctx.log())) {
                ctx.log()->Error("[RpDepthFill::LazyInit]: Failed to initialize pipeline!");
            }
        }

        initialized = true;
    }

    if (!shadow_fb_.Setup(ctx.api_ctx(), rp_depth_only_, w_, h_, nullptr, 0, shadowmap_tex.ref, {}, false)) {
        ctx.log()->Error("RpShadowMaps: shadow_fb_ init failed!");
    }
}
