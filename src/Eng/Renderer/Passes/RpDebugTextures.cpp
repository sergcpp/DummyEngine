#include "RpDebugTextures.h"

#include <Ren/Context.h>
#include <Ren/RastState.h>
#include <Ren/Texture.h>

#include "../../Utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

void RpDebugTextures::Setup(
    RpBuilder &builder, const ViewState *view_state, const DrawList &list,
    const int orphan_index, const Ren::Tex2DRef &down_tex_4x,
    const char shared_data_buf_name[], const char cells_buf_name[],
    const char items_buf_name[], const char shadow_map_name[],
    const char main_color_tex_name[], const char main_normal_tex_name[],
    const char main_spec_tex_name[], const char main_depth_tex_name[],
    const char ssao_tex_name[], const char blur_res_name[], const char reduced_tex_name[],
    Ren::TexHandle output_tex) {
    orphan_index_ = orphan_index;
    render_flags_ = list.render_flags;
    view_state_ = view_state;
    draw_cam_ = &list.draw_cam;
    output_tex_ = output_tex;

    if (!list.depth_pixels.empty()) {
        depth_pixels_ = list.depth_pixels.data();
    } else {
        depth_pixels_ = nullptr;
    }

    if (!list.depth_tiles.empty()) {
        depth_tiles_ = list.depth_tiles.data();
    } else {
        depth_tiles_ = nullptr;
    }

    down_tex_4x_ = down_tex_4x;

    shadow_lists_ = list.shadow_lists;
    shadow_regions_ = list.shadow_regions;
    cached_shadow_regions_ = list.cached_shadow_regions;

    nodes_ = list.temp_nodes.data();
    nodes_count_ = uint32_t(list.temp_nodes.size());
    root_node_ = list.root_index;

    shared_data_buf_ = builder.ReadBuffer(shared_data_buf_name, *this);
    cells_buf_ = builder.ReadBuffer(cells_buf_name, *this);
    items_buf_ = builder.ReadBuffer(items_buf_name, *this);

    shadowmap_tex_ = builder.ReadTexture(shadow_map_name, *this);
    color_tex_ = builder.ReadTexture(main_color_tex_name, *this);
    normal_tex_ = builder.ReadTexture(main_normal_tex_name, *this);
    spec_tex_ = builder.ReadTexture(main_spec_tex_name, *this);
    depth_tex_ = builder.ReadTexture(main_depth_tex_name, *this);
    ssao_tex_ = builder.ReadTexture(ssao_tex_name, *this);
    blur_tex_ = builder.ReadTexture(blur_res_name, *this);
    reduced_tex_ = builder.ReadTexture(reduced_tex_name, *this);
}

void RpDebugTextures::Execute(RpBuilder &builder) {
    LazyInit(builder.ctx(), builder.sh());

    RpAllocBuf &unif_shared_data_buf = builder.GetReadBuffer(shared_data_buf_);
    RpAllocBuf &cells_buf = builder.GetReadBuffer(cells_buf_);
    RpAllocBuf &items_buf = builder.GetReadBuffer(items_buf_);
    RpAllocTex &shadowmap_tex = builder.GetReadTexture(shadowmap_tex_);
    RpAllocTex &color_tex = builder.GetReadTexture(color_tex_);
    RpAllocTex &normal_tex = builder.GetReadTexture(normal_tex_);
    RpAllocTex &spec_tex = builder.GetReadTexture(spec_tex_);
    RpAllocTex &depth_tex = builder.GetReadTexture(depth_tex_);
    RpAllocTex &ssao_tex = builder.GetReadTexture(ssao_tex_);
    RpAllocTex &blur_tex = builder.GetReadTexture(blur_tex_);
    RpAllocTex &reduced_tex = builder.GetReadTexture(reduced_tex_);

    Ren::RastState rast_state;
    rast_state.cull_face.enabled = true;

    rast_state.viewport[2] = view_state_->scr_res[0];
    rast_state.viewport[3] = view_state_->scr_res[1];

    rast_state.Apply();
    Ren::RastState applied_state = rast_state;

    if (render_flags_ & (DebugLights | DebugDecals)) {
        rast_state.blend.enabled = true;
        rast_state.blend.src = Ren::eBlendFactor::SrcAlpha;
        rast_state.blend.dst = Ren::eBlendFactor::OneMinusSrcAlpha;

        rast_state.ApplyChanged(applied_state);
        applied_state = rast_state;

        ////

        Ren::Program *blit_prog = view_state_->is_multisampled
                                      ? blit_prog = blit_debug_ms_prog_.get()
                                      : blit_prog = blit_debug_prog_.get();

        PrimDraw::Uniform uniforms[] = {
            {0, Ren::Vec4f{0.0f, 0.0f, float(view_state_->act_res[0]),
                           float(view_state_->act_res[1])}},
            {15, Ren::Vec2i{view_state_->scr_res[0], view_state_->scr_res[1]}},
            {16, 0.0f},
            {17, draw_cam_->clip_info()}};

        if (render_flags_ & DebugLights) {
            uniforms[2].fdata[0] = 0.5f;
        } else if (render_flags_ & DebugDecals) {
            uniforms[2].fdata[0] = 1.5f;
        }

        PrimDraw::Binding bindings[3];

        if (view_state_->is_multisampled) {
            bindings[0] = {Ren::eBindTarget::Tex2DMs, REN_BASE0_TEX_SLOT,
                           depth_tex.ref->handle()};
        } else {
            bindings[0] = {Ren::eBindTarget::Tex2D, REN_BASE0_TEX_SLOT,
                           depth_tex.ref->handle()};
        }

        bindings[1] = {Ren::eBindTarget::TexBuf, REN_CELLS_BUF_SLOT,
                       cells_buf.tbos[orphan_index_]->handle()};
        bindings[2] = {Ren::eBindTarget::TexBuf, REN_ITEMS_BUF_SLOT,
                       items_buf.tbos[orphan_index_]->handle()};

        prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, {output_fb_.id(), 0}, blit_prog,
                            bindings, 3, uniforms, 4);
    }

    int x_offset = 0;

    const auto debug_cull = uint32_t(EnableCulling | DebugCulling);
    if (((render_flags_ & debug_cull) == debug_cull) && depth_pixels_ && depth_tiles_) {
        //
        // Depth values
        //

        temp_tex_->SetSubImage(0, 0, 0, REN_CULL_RES_X, REN_CULL_RES_Y,
                               Ren::eTexFormat::RawRGBA8888, depth_pixels_);

        x_offset += BlitTex(applied_state, x_offset, 0, REN_CULL_RES_X, REN_CULL_RES_Y,
                            temp_tex_, 1.0f);

        //
        // Depth tiles (min)
        //

        /*temp_tex_->SetSubImage(0, 0, 0, 256, 128, Ren::eTexFormat::RawRGBA8888,
                               depth_tiles_);

        x_offset += BlitTex(applied_state, x_offset, 0, 256, 128, temp_tex_, 1.0f);*/
    }

    if (render_flags_ & DebugDeferred) {
        x_offset += BlitTex(applied_state, x_offset, 0, 384, -1, color_tex.ref, 1.0f);
        x_offset += BlitTex(applied_state, x_offset, 0, 384, -1, spec_tex.ref, 1.0f);
        x_offset += BlitTex(applied_state, x_offset, 0, 384, -1, normal_tex.ref, 1.0f);
        x_offset += BlitTex(applied_state, x_offset, 0, 384, -1, down_tex_4x_, 1.0f);
    }

    if (render_flags_ & DebugReduce) {
        x_offset += BlitTex(applied_state, x_offset, 0, 256, 128, reduced_tex.ref, 10.0f);
    }

    if (render_flags_ & DebugBlur) {
        x_offset += BlitTex(applied_state, x_offset, 0, 384, -1, blur_tex.ref, 1.0f);
    }

    if (render_flags_ & DebugSSAO) {
        x_offset += BlitTex(applied_state, x_offset, 0, 384, -1, ssao_tex.ref, 1.0f);
    }

    if (render_flags_ & DebugBVH) {
        const uint32_t buf_size = nodes_count_ * sizeof(bvh_node_t);

        if (!nodes_buf_ || buf_size > nodes_buf_->size()) {
            nodes_buf_ = builder.ctx().CreateBuffer(
                "Nodes buf", Ren::eBufferType::Texture, Ren::eBufferAccessType::Draw,
                Ren::eBufferAccessFreq::Dynamic, buf_size);
            const uint32_t off = nodes_buf_->AllocRegion(buf_size, nodes_);
            assert(off == 0);

            nodes_tbo_ = builder.ctx().CreateTexture1D(
                "Nodes TBO", nodes_buf_, Ren::eTexFormat::RawRGBA32F, 0, buf_size);
        } else {
            const bool res = nodes_buf_->FreeRegion(0);
            assert(res);

            const uint32_t off = nodes_buf_->AllocRegion(buf_size, nodes_);
            assert(off == 0);
        }

        ///

        rast_state.blend.enabled = true;
        rast_state.blend.src = Ren::eBlendFactor::SrcAlpha;
        rast_state.blend.dst = Ren::eBlendFactor::OneMinusSrcAlpha;

        rast_state.viewport =
            Ren::Vec4i{0, 0, view_state_->scr_res[0], view_state_->scr_res[1]};
        rast_state.Apply();
        applied_state = rast_state;

        Ren::Program *debug_bvh_prog = view_state_->is_multisampled
                                           ? blit_debug_bvh_ms_prog_.get()
                                           : blit_debug_bvh_prog_.get();

        PrimDraw::Binding bindings[3];
        bindings[0] = {Ren::eBindTarget::UBuf, REN_UB_SHARED_DATA_LOC,
                       orphan_index_ * SharedDataBlockSize, sizeof(SharedDataBlock),
                       unif_shared_data_buf.ref->handle()};
        bindings[1] = {view_state_->is_multisampled ? Ren::eBindTarget::Tex2DMs
                                                    : Ren::eBindTarget::Tex2D,
                       0, depth_tex.ref->handle()};
        bindings[2] = {Ren::eBindTarget::TexBuf, 1, nodes_tbo_->handle()};

        const PrimDraw::Uniform uniforms[] = {
            {0, Ren::Vec4f{0.0f, 0.0f, float(view_state_->act_res[0]),
                           float(view_state_->act_res[1])}},
            {12, int(root_node_)}};

        prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, {output_fb_.id(), 0}, debug_bvh_prog,
                            bindings, 3, uniforms, 2);
    }

    if (render_flags_ & DebugShadow) {
        DrawShadowMaps(builder.ctx(), shadowmap_tex);
    }
}

void RpDebugTextures::LazyInit(Ren::Context &ctx, ShaderLoader &sh) {
    Ren::BufferRef vtx_buf1 = ctx.default_vertex_buf1(),
                   vtx_buf2 = ctx.default_vertex_buf2(),
                   ndx_buf = ctx.default_indices_buf();

    if (!initialized) {
        blit_prog_ = sh.LoadProgram(ctx, "blit", "internal/blit.vert.glsl",
                                    "internal/blit.frag.glsl");
        assert(blit_prog_->ready());
        blit_debug_prog_ = sh.LoadProgram(ctx, "blit_debug", "internal/blit.vert.glsl",
                                          "internal/blit_debug.frag.glsl");
        assert(blit_debug_prog_->ready());
        blit_debug_ms_prog_ =
            sh.LoadProgram(ctx, "blit_debug_ms", "internal/blit.vert.glsl",
                           "internal/blit_debug.frag.glsl@MSAA_4");
        assert(blit_debug_ms_prog_->ready());
        blit_debug_bvh_prog_ =
            sh.LoadProgram(ctx, "blit_debug_bvh", "internal/blit.vert.glsl",
                           "internal/blit_debug_bvh.frag.glsl");
        assert(blit_debug_bvh_prog_->ready());
        blit_debug_bvh_ms_prog_ =
            sh.LoadProgram(ctx, "blit_debug_bvh_ms", "internal/blit.vert.glsl",
                           "internal/blit_debug_bvh.frag.glsl@MSAA_4");
        assert(blit_debug_bvh_ms_prog_->ready());
        blit_depth_prog_ = sh.LoadProgram(ctx, "blit_depth", "internal/blit.vert.glsl",
                                          "internal/blit_depth.frag.glsl");
        assert(blit_depth_prog_->ready());

        { // temp texture
            Ren::Tex2DParams params;
            params.w = REN_CULL_RES_X;
            params.h = REN_CULL_RES_Y;
            params.format = Ren::eTexFormat::RawRGBA8888;
            params.filter = Ren::eTexFilter::NoFilter;
            params.repeat = Ren::eTexRepeat::ClampToEdge;

            Ren::eTexLoadStatus status;
            temp_tex_ = ctx.LoadTexture2D("__DEBUG_TEMP_TEXTURE__", params, &status);
            assert(status == Ren::eTexLoadStatus::TexCreatedDefault);
        }

        initialized = true;
    }

    { // setup temp vao
        const Ren::VtxAttribDesc attribs[] = {
            {vtx_buf1->handle(), REN_VTX_POS_LOC, 2, Ren::eType::Float32, 0,
             uintptr_t(prim_draw_.temp_buf1_vtx_offset())},
            {vtx_buf1->handle(), REN_VTX_UV1_LOC, 2, Ren::eType::Float32, 0,
             uintptr_t(prim_draw_.temp_buf1_vtx_offset() + 8 * sizeof(float))}};

        if (!temp_vao_.Setup(attribs, 2, ndx_buf->handle())) {
            ctx.log()->Error("RpDebugTextures: temp_vao_ init failed!");
        }
    }

    if (!output_fb_.Setup(&output_tex_, 1, {}, {}, false)) {
        ctx.log()->Error("RpDebugTextures: output_fb_ init failed!");
    }
}

int RpDebugTextures::BlitTex(Ren::RastState &applied_state, const int x, const int y,
                             const int w, int h, Ren::WeakTex2DRef tex, const float mul) {
    const auto &p = tex->params();
    if (h == -1) {
        h = w * p.h / p.w;
    }

    Ren::RastState rast_state = applied_state;
    rast_state.viewport = Ren::Vec4i{x, y, w, h};
    rast_state.ApplyChanged(applied_state);
    applied_state = rast_state;

    const PrimDraw::Binding bindings[] = {
        {Ren::eBindTarget::Tex2D, REN_BASE0_TEX_SLOT, tex->handle()}};

    const PrimDraw::Uniform uniforms[] = {
        {0, Ren::Vec4f{0.0f, 0.0f, float(p.w), float(p.h)}}, {4, mul}};

    prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, {output_fb_.id(), 0}, blit_prog_.get(),
                        bindings, 1, uniforms, 2);

    return w;
}