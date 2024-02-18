#include "Renderer.h"

#include "Utils.h"

#include "shaders.inl"

int Gui::Renderer::g_instance_count = 0;

const uint8_t Gui::ColorWhite[4] = {255, 255, 255, 255};
const uint8_t Gui::ColorGrey[4] = {127, 127, 127, 255};
const uint8_t Gui::ColorBlack[4] = {0, 0, 0, 255};
const uint8_t Gui::ColorRed[4] = {255, 0, 0, 255};
const uint8_t Gui::ColorGreen[4] = {0, 255, 0, 255};
const uint8_t Gui::ColorBlue[4] = {0, 0, 255, 255};
const uint8_t Gui::ColorCyan[4] = {0, 255, 255, 255};
const uint8_t Gui::ColorMagenta[4] = {255, 0, 255, 255};
const uint8_t Gui::ColorYellow[4] = {255, 255, 0, 255};

namespace UIRendererConstants {
extern const int TexAtlasSlot = TEX_ATLAS_SLOT;
}

bool Gui::Renderer::Init() {
    Ren::ProgramRef ui_program;

#if 0
    { // dump shaders into files
        std::ofstream vs_out("ui.vert.glsl", std::ios::binary);
        vs_out.write(vs_source, strlen(vs_source));

        std::ofstream fs_out("ui.frag.glsl", std::ios::binary);
        fs_out.write(fs_source, strlen(fs_source));
    }

    system("src\\libs\\spirv\\win32\\glslangValidator.exe -V ui.vert.glsl -o ui.vert.spv");
    system("src\\libs\\spirv\\win32\\glslangValidator.exe -V ui.frag.glsl -o ui.frag.spv");
    system("src\\libs\\spirv\\win32\\glslangValidator.exe -G ui.vert.glsl -o ui.vert.spv_ogl");
    system("src\\libs\\spirv\\win32\\glslangValidator.exe -G ui.frag.glsl -o ui.frag.spv_ogl");
    system("bin2c.exe -o temp.h ui.vert.spv ui.frag.spv ui.vert.spv_ogl ui.frag.spv_ogl");
#endif

    { // Load main shader
        using namespace Ren;

        ShaderRef ui_vs_ref, ui_fs_ref;
        if (ctx_.capabilities.spirv) {
            eShaderLoadStatus sh_status;
            ui_vs_ref = ctx_.LoadShaderSPIRV("__ui_vs__",
#if defined(USE_VK_RENDER)
                                             ui_vert_spv, ui_vert_spv_size,
#else
                                             ui_vert_spv_ogl, ui_vert_spv_ogl_size,
#endif
                                             eShaderType::Vert, &sh_status);
            if (sh_status != eShaderLoadStatus::CreatedFromData && sh_status != eShaderLoadStatus::Found) {
                ctx_.log()->Error("[Gui::Renderer::Init]: Failed to compile vertex shader!");
                return false;
            }
            ui_fs_ref = ctx_.LoadShaderSPIRV("__ui_fs__",
#if defined(USE_VK_RENDER)
                                             ui_frag_spv, ui_frag_spv_size,
#else
                                             ui_frag_spv_ogl, ui_frag_spv_ogl_size,
#endif
                                             eShaderType::Frag, &sh_status);
            if (sh_status != eShaderLoadStatus::CreatedFromData && sh_status != eShaderLoadStatus::Found) {
                ctx_.log()->Error("[Gui::Renderer::Init]: Failed to compile fragment shader!");
                return false;
            }
        } else {
            eShaderLoadStatus sh_status;
            ui_vs_ref = ctx_.LoadShaderGLSL("__ui_vs__", vs_source, eShaderType::Vert, &sh_status);
            if (sh_status != eShaderLoadStatus::CreatedFromData && sh_status != eShaderLoadStatus::Found) {
                ctx_.log()->Error("[Gui::Renderer::Init]: Failed to compile vertex shader!");
                return false;
            }
            ui_fs_ref = ctx_.LoadShaderGLSL("__ui_fs__", fs_source, eShaderType::Frag, &sh_status);
            if (sh_status != eShaderLoadStatus::CreatedFromData && sh_status != eShaderLoadStatus::Found) {
                ctx_.log()->Error("[Gui::Renderer::Init]: Failed to compile fragment shader!");
                return false;
            }
        }

        eProgLoadStatus status;
        ui_program = ctx_.LoadProgram("__ui_program__", ui_vs_ref, ui_fs_ref, {}, {}, &status);
        if (status != eProgLoadStatus::CreatedFromData && status != eProgLoadStatus::Found) {
            ctx_.log()->Error("[Gui::Renderer::Init]: Failed to link program!");
            return false;
        }
    }

    // TODO: refactor this
    snprintf(name_, sizeof(name_), "UI_Render [%i]", instance_index_);

    char name_buf[32];

    snprintf(name_buf, sizeof(name_buf), "UI_VertexBuffer [%i]", instance_index_);
    vertex_buf_ = ctx_.LoadBuffer(name_buf, Ren::eBufType::VertexAttribs, MaxVerticesPerRange * sizeof(vertex_t));

    snprintf(name_buf, sizeof(name_buf), "UI_VertexStageBuffer [%i]", instance_index_);
    vertex_stage_buf_ = ctx_.LoadBuffer(name_buf, Ren::eBufType::Stage,
                                        Ren::MaxFramesInFlight * MaxVerticesPerRange * sizeof(vertex_t));

    snprintf(name_buf, sizeof(name_buf), "UI_IndexBuffer [%i]", instance_index_);
    index_buf_ = ctx_.LoadBuffer(name_buf, Ren::eBufType::VertexIndices, MaxIndicesPerRange * sizeof(uint16_t));

    snprintf(name_buf, sizeof(name_buf), "UI_IndexStageBuffer [%i]", instance_index_);
    index_stage_buf_ =
        ctx_.LoadBuffer(name_buf, Ren::eBufType::Stage, Ren::MaxFramesInFlight * MaxIndicesPerRange * sizeof(uint16_t));

    if (ctx_.capabilities.persistent_buf_mapping) {
        // map stage buffers directly
        vtx_stage_data_ = reinterpret_cast<vertex_t *>(vertex_stage_buf_->Map(Ren::eBufMap::Write, true /* persistent */));
        ndx_stage_data_ = reinterpret_cast<uint16_t *>(index_stage_buf_->Map(Ren::eBufMap::Write, true /* persistent */));
    } else {
        // use temporary storage
        stage_vtx_data_ = std::make_unique<vertex_t[]>(MaxVerticesPerRange * Ren::MaxFramesInFlight);
        stage_ndx_data_ = std::make_unique<uint16_t[]>(MaxIndicesPerRange * Ren::MaxFramesInFlight);
        vtx_stage_data_ = stage_vtx_data_.get();
        ndx_stage_data_ = stage_ndx_data_.get();
    }

    for (int i = 0; i < Ren::MaxFramesInFlight; i++) {
        vtx_count_[i] = 0;
        ndx_count_[i] = 0;
    }

    Ren::ApiContext *api_ctx = ctx_.api_ctx();

    { // create renderpass
        const auto &p = ctx_.backbuffer_ref()->params;
        Ren::RenderTargetInfo rt_info = {p.format, p.samples, Ren::eImageLayout::ColorAttachmentOptimal,
                                         Ren::eLoadOp::Load, Ren::eStoreOp::Store};
        rt_info.flags = (p.flags & ~Ren::eTexFlagBits::NoOwnership);

        if (!render_pass_.Setup(ctx_.api_ctx(), {&rt_info, 1}, {}, ctx_.log())) {
            ctx_.log()->Error("[Gui::Renderer::Init]: Failed to create render pass!");
            return false;
        }
    }

    { // initialize vertex input
        const Ren::VtxAttribDesc attribs[] = {
            {vertex_buf_->handle(), VTX_POS_LOC, 3, Ren::eType::Float32, sizeof(vertex_t), offsetof(vertex_t, pos)},
            {vertex_buf_->handle(), VTX_COL_LOC, 4, Ren::eType::Uint8UNorm, sizeof(vertex_t), offsetof(vertex_t, col)},
            {vertex_buf_->handle(), VTX_UVS_LOC, 4, Ren::eType::Uint16UNorm, sizeof(vertex_t),
             offsetof(vertex_t, uvs)}};
        if (!vtx_input_.Setup(attribs, index_buf_)) {
            ctx_.log()->Error("[Gui::Renderer::Init]: Failed to initialize vertex input!");
            return false;
        }
    }

    { // create graphics pipeline
        Ren::RastState rast_state;
        rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

        rast_state.blend.enabled = true;
        rast_state.blend.src = uint8_t(Ren::eBlendFactor::SrcAlpha);
        rast_state.blend.dst = uint8_t(Ren::eBlendFactor::OneMinusSrcAlpha);

        if (!pipeline_.Init(api_ctx, rast_state, std::move(ui_program), &vtx_input_, &render_pass_, 0, ctx_.log())) {
            ctx_.log()->Error("[Gui::Renderer::Init]: Failed to create graphics pipeline!");
            return false;
        }
    }

    return true;
}

void Gui::Renderer::PushClipArea(const Vec2f dims[2]) {
    clip_area_stack_[clip_area_stack_size_][0] = dims[0];
    clip_area_stack_[clip_area_stack_size_][1] = dims[0] + dims[1];
    if (clip_area_stack_size_) {
        clip_area_stack_[clip_area_stack_size_][0] =
            Max(clip_area_stack_[clip_area_stack_size_ - 1][0], clip_area_stack_[clip_area_stack_size_][0]);
        clip_area_stack_[clip_area_stack_size_][1] =
            Min(clip_area_stack_[clip_area_stack_size_ - 1][1], clip_area_stack_[clip_area_stack_size_][1]);
    }
    ++clip_area_stack_size_;
}

void Gui::Renderer::PopClipArea() { --clip_area_stack_size_; }

const Gui::Vec2f *Gui::Renderer::GetClipArea() const {
    if (clip_area_stack_size_) {
        return clip_area_stack_[clip_area_stack_size_ - 1];
    }
    return nullptr;
}

int Gui::Renderer::AcquireVertexData(vertex_t **vertex_data, int *vertex_avail, uint16_t **index_data,
                                     int *index_avail) {
    (*vertex_data) =
        vtx_stage_data_ + size_t(ctx_.frontend_frame) * MaxVerticesPerRange + vtx_count_[ctx_.frontend_frame];
    (*vertex_avail) = MaxVerticesPerRange - vtx_count_[ctx_.frontend_frame];

    (*index_data) =
        ndx_stage_data_ + size_t(ctx_.frontend_frame) * MaxIndicesPerRange + ndx_count_[ctx_.frontend_frame];
    (*index_avail) = MaxIndicesPerRange - ndx_count_[ctx_.frontend_frame];

    return vtx_count_[ctx_.frontend_frame];
}

void Gui::Renderer::SubmitVertexData(const int vertex_count, const int index_count) {
    assert((vtx_count_[ctx_.frontend_frame] + vertex_count) <= MaxVerticesPerRange &&
           (ndx_count_[ctx_.frontend_frame] + index_count) <= MaxIndicesPerRange);

    vtx_count_[ctx_.frontend_frame] += vertex_count;
    ndx_count_[ctx_.frontend_frame] += index_count;
}

void Gui::Renderer::PushImageQuad(const eDrawMode draw_mode, const int tex_layer, const Vec2f pos[2],
                                  const Vec2f uvs_px[2]) {
    const Vec2f uvs_scale = 1.0f / Vec2f{float(Ren::TextureAtlasWidth), float(Ren::TextureAtlasHeight)};
    Vec4f pos_uvs[2] = {Vec4f{pos[0][0], pos[0][1], uvs_px[0][0] * uvs_scale[0], uvs_px[0][1] * uvs_scale[1]},
                        Vec4f{pos[1][0], pos[1][1], uvs_px[1][0] * uvs_scale[0], uvs_px[1][1] * uvs_scale[1]}};

    vertex_t *vtx_data;
    int vtx_avail;
    uint16_t *ndx_data;
    int ndx_avail;
    int ndx_offset = AcquireVertexData(&vtx_data, &vtx_avail, &ndx_data, &ndx_avail);
    assert(vtx_avail >= 4 && ndx_avail >= 6);

    vertex_t *cur_vtx = vtx_data;
    uint16_t *cur_ndx = ndx_data;

    const uint16_t u16_tex_layer = f32_to_u16((1.0f / 16.0f) * float(tex_layer));

    static const uint16_t u16_draw_mode[] = {0, 32767, 65535};

    if (clip_area_stack_size_ && !ClipQuadToArea(pos_uvs, clip_area_stack_[clip_area_stack_size_ - 1])) {
        return;
    }

    cur_vtx->pos[0] = pos_uvs[0][0];
    cur_vtx->pos[1] = pos_uvs[0][1];
    cur_vtx->pos[2] = 0.0f;
    cur_vtx->col[0] = cur_vtx->col[1] = cur_vtx->col[2] = cur_vtx->col[3] = 255;
    cur_vtx->uvs[0] = f32_to_u16(pos_uvs[0][2]);
    cur_vtx->uvs[1] = f32_to_u16(pos_uvs[0][3]);
    cur_vtx->uvs[2] = u16_tex_layer;
    cur_vtx->uvs[3] = u16_draw_mode[int(draw_mode)];
    ++cur_vtx;

    cur_vtx->pos[0] = pos_uvs[1][0];
    cur_vtx->pos[1] = pos_uvs[0][1];
    cur_vtx->pos[2] = 0.0f;
    cur_vtx->col[0] = cur_vtx->col[1] = cur_vtx->col[2] = cur_vtx->col[3] = 255;
    cur_vtx->uvs[0] = f32_to_u16(pos_uvs[1][2]);
    cur_vtx->uvs[1] = f32_to_u16(pos_uvs[0][3]);
    cur_vtx->uvs[2] = u16_tex_layer;
    cur_vtx->uvs[3] = u16_draw_mode[int(draw_mode)];
    ++cur_vtx;

    cur_vtx->pos[0] = pos_uvs[1][0];
    cur_vtx->pos[1] = pos_uvs[1][1];
    cur_vtx->pos[2] = 0.0f;
    cur_vtx->col[0] = cur_vtx->col[1] = cur_vtx->col[2] = cur_vtx->col[3] = 255;
    cur_vtx->uvs[0] = f32_to_u16(pos_uvs[1][2]);
    cur_vtx->uvs[1] = f32_to_u16(pos_uvs[1][3]);
    cur_vtx->uvs[2] = u16_tex_layer;
    cur_vtx->uvs[3] = u16_draw_mode[int(draw_mode)];
    ++cur_vtx;

    cur_vtx->pos[0] = pos_uvs[0][0];
    cur_vtx->pos[1] = pos_uvs[1][1];
    cur_vtx->pos[2] = 0.0f;
    cur_vtx->col[0] = cur_vtx->col[1] = cur_vtx->col[2] = cur_vtx->col[3] = 255;
    cur_vtx->uvs[0] = f32_to_u16(pos_uvs[0][2]);
    cur_vtx->uvs[1] = f32_to_u16(pos_uvs[1][3]);
    cur_vtx->uvs[2] = u16_tex_layer;
    cur_vtx->uvs[3] = u16_draw_mode[int(draw_mode)];
    ++cur_vtx;

    (*cur_ndx++) = ndx_offset + 0;
    (*cur_ndx++) = ndx_offset + 1;
    (*cur_ndx++) = ndx_offset + 2;

    (*cur_ndx++) = ndx_offset + 0;
    (*cur_ndx++) = ndx_offset + 2;
    (*cur_ndx++) = ndx_offset + 3;

    SubmitVertexData(int(cur_vtx - vtx_data), int(cur_ndx - ndx_data));
}

void Gui::Renderer::PushLine(eDrawMode draw_mode, int tex_layer, const uint8_t color[4], const Vec4f &p0,
                             const Vec4f &p1, const Vec2f &d0, const Vec2f &d1, const Vec4f &thickness) {
    const Vec2f uvs_scale = 1.0f / Vec2f{float(Ren::TextureAtlasWidth), float(Ren::TextureAtlasHeight)};

    const uint16_t u16_tex_layer = f32_to_u16((1.0f / 16.0f) * float(tex_layer));

    static const uint16_t u16_draw_mode[] = {0, 32767, 65535};

    const Vec4f perp[2] = {Vec4f{thickness} * Vec4f{-d0[1], d0[0], 1.0f, 0.0f},
                           Vec4f{thickness} * Vec4f{-d1[1], d1[0], 1.0f, 0.0f}};

    Vec4f pos_uvs[8] = {p0 - perp[0], p1 - perp[1], p1 + perp[1], p0 + perp[0]};
    int vertex_count = 4;

    for (int i = 0; i < vertex_count; i++) {
        pos_uvs[i][2] *= uvs_scale[0];
        pos_uvs[i][3] *= uvs_scale[1];
    }

    if (clip_area_stack_size_ &&
        !(vertex_count = ClipPolyToArea(pos_uvs, vertex_count, clip_area_stack_[clip_area_stack_size_ - 1]))) {
        return;
    }
    assert(vertex_count < 8);

    vertex_t *vtx_data;
    int vtx_avail;
    uint16_t *ndx_data;
    int ndx_avail;
    int ndx_offset = AcquireVertexData(&vtx_data, &vtx_avail, &ndx_data, &ndx_avail);
    assert(vtx_avail >= vertex_count && ndx_avail >= 3 * (vertex_count - 2));

    vertex_t *cur_vtx = vtx_data;
    uint16_t *cur_ndx = ndx_data;

    for (int i = 0; i < vertex_count; i++) {
        cur_vtx->pos[0] = pos_uvs[i][0];
        cur_vtx->pos[1] = pos_uvs[i][1];
        cur_vtx->pos[2] = 0.0f;
        memcpy(cur_vtx->col, color, 4);
        cur_vtx->uvs[0] = f32_to_u16(pos_uvs[i][2]);
        cur_vtx->uvs[1] = f32_to_u16(pos_uvs[i][3]);
        cur_vtx->uvs[2] = u16_tex_layer;
        cur_vtx->uvs[3] = u16_draw_mode[int(draw_mode)];
        ++cur_vtx;
    }

    for (int i = 0; i < vertex_count - 2; i++) {
        (*cur_ndx++) = ndx_offset + 0;
        (*cur_ndx++) = ndx_offset + i + 1;
        (*cur_ndx++) = ndx_offset + i + 2;
    }

    SubmitVertexData(int(cur_vtx - vtx_data), int(cur_ndx - ndx_data));
}

void Gui::Renderer::PushCurve(eDrawMode draw_mode, int tex_layer, const uint8_t color[4], const Vec4f &p0,
                              const Vec4f &p1, const Vec4f &p2, const Vec4f &p3, const Vec4f &thickness) {
    const float tolerance = 0.000001f;

    const Vec4f p01 = 0.5f * (p0 + p1), p12 = 0.5f * (p1 + p2), p23 = 0.5f * (p2 + p3), p012 = 0.5f * (p01 + p12),
                p123 = 0.5f * (p12 + p23), p0123 = 0.5f * (p012 + p123);

    const Vec2f d = Vec2f{p3} - Vec2f{p0};
    const float d2 = std::abs((p1[0] - p3[0]) * d[1] - (p1[1] - p3[1]) * d[0]),
                d3 = std::abs((p2[0] - p3[0]) * d[1] - (p2[1] - p3[1]) * d[0]);

    if ((d2 + d3) * (d2 + d3) < tolerance * (d[0] * d[0] + d[1] * d[1])) {
        PushLine(draw_mode, tex_layer, color, p0, p3, Normalize(Vec2f{p1 - p0}), Normalize(Vec2f{p3 - p2}), thickness);
    } else {
        PushCurve(draw_mode, tex_layer, color, p0, p01, p012, p0123, thickness);
        PushCurve(draw_mode, tex_layer, color, p0123, p123, p23, p3, thickness);
    }
}

#undef VTX_POS_LOC
#undef VTX_COL_LOC
#undef VTX_UVS_LOC

#undef TEX_ATLAS_SLOT