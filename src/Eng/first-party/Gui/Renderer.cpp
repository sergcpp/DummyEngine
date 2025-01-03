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

namespace Gui {
extern const int TexAtlasSlot = TEX_ATLAS_SLOT;
static const uint16_t g_draw_mode_u16[] = {0, 32767, 65535};
} // namespace Gui

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
            ui_vs_ref = ctx_.LoadShaderSPIRV("__ui_vs__",
#if defined(REN_VK_BACKEND)
                                             ui_vert_spv,
#else
                                             ui_vert_spv_ogl,
#endif
                                             eShaderType::Vertex);
            if (!ui_vs_ref->ready()) {
                ctx_.log()->Error("[Gui::Renderer::Init]: Failed to compile vertex shader!");
                return false;
            }
            ui_fs_ref = ctx_.LoadShaderSPIRV("__ui_fs__",
#if defined(REN_VK_BACKEND)
                                             ui_frag_spv,
#else
                                             ui_frag_spv_ogl,
#endif
                                             eShaderType::Fragment);
            if (!ui_fs_ref->ready()) {
                ctx_.log()->Error("[Gui::Renderer::Init]: Failed to compile fragment shader!");
                return false;
            }
        } else {
#if defined(REN_GL_BACKEND)
            ui_vs_ref = ctx_.LoadShaderGLSL("__ui_vs__", vs_source, eShaderType::Vertex);
            if (!ui_vs_ref->ready()) {
                ctx_.log()->Error("[Gui::Renderer::Init]: Failed to compile vertex shader!");
                return false;
            }
            ui_fs_ref = ctx_.LoadShaderGLSL("__ui_fs__", fs_source, eShaderType::Fragment);
            if (!ui_fs_ref->ready()) {
                ctx_.log()->Error("[Gui::Renderer::Init]: Failed to compile fragment shader!");
                return false;
            }
#else
            return false;
#endif
        }

        ui_program = ctx_.LoadProgram(ui_vs_ref, ui_fs_ref, {}, {}, {});
        if (!ui_program) {
            ctx_.log()->Error("[Gui::Renderer::Init]: Failed to link program!");
            return false;
        }
    }

    name_ = "Gui::Renderer[" + std::to_string(instance_index_) + "]";

    char name_buf[32];
    snprintf(name_buf, sizeof(name_buf), "Gui::VertexBuffer[%i]", instance_index_);
    vertex_buf_ = ctx_.LoadBuffer(name_buf, Ren::eBufType::VertexAttribs, MaxVerticesPerRange * sizeof(vertex_t));

    snprintf(name_buf, sizeof(name_buf), "Gui::VertexStageBuffer[%i]", instance_index_);
    vertex_stage_buf_ = ctx_.LoadBuffer(name_buf, Ren::eBufType::Upload,
                                        (Ren::MaxFramesInFlight + 1) * MaxVerticesPerRange * sizeof(vertex_t));

    snprintf(name_buf, sizeof(name_buf), "Gui::IndexBuffer[%i]", instance_index_);
    index_buf_ = ctx_.LoadBuffer(name_buf, Ren::eBufType::VertexIndices, MaxIndicesPerRange * sizeof(uint16_t));

    snprintf(name_buf, sizeof(name_buf), "Gui::IndexStageBuffer[%i]", instance_index_);
    index_stage_buf_ = ctx_.LoadBuffer(name_buf, Ren::eBufType::Upload,
                                       (Ren::MaxFramesInFlight + 1) * MaxIndicesPerRange * sizeof(uint16_t));

    if (ctx_.capabilities.persistent_buf_mapping) {
        // map stage buffers directly
        vtx_stage_data_ = reinterpret_cast<vertex_t *>(vertex_stage_buf_->Map(true /* persistent */));
        ndx_stage_data_ = reinterpret_cast<uint16_t *>(index_stage_buf_->Map(true /* persistent */));
    } else {
        // use temporary storage
        stage_vtx_data_ = std::make_unique<vertex_t[]>(MaxVerticesPerRange);
        stage_ndx_data_ = std::make_unique<uint16_t[]>(MaxIndicesPerRange);
        vtx_stage_data_ = stage_vtx_data_.get();
        ndx_stage_data_ = stage_ndx_data_.get();
    }

    for (int i = 0; i < Ren::MaxFramesInFlight + 1; i++) {
        vtx_count_[i] = ndx_count_[i] = 0;
    }

    Ren::ApiContext *api_ctx = ctx_.api_ctx();

    { // create renderpass
        const auto &p = ctx_.backbuffer_ref()->params;
        Ren::RenderTargetInfo rt_info = {p.format, p.samples, Ren::eImageLayout::ColorAttachmentOptimal,
                                         Ren::eLoadOp::Load, Ren::eStoreOp::Store};
        rt_info.flags = (p.flags & ~Ren::Bitmask(Ren::eTexFlags::NoOwnership));

        render_pass_ = ctx_.LoadRenderPass({}, {&rt_info, 1});
    }

    { // initialize vertex input
        const Ren::VtxAttribDesc attribs[] = {
            {vertex_buf_, VTX_POS_LOC, 3, Ren::eType::Float32, sizeof(vertex_t), offsetof(vertex_t, pos)},
            {vertex_buf_, VTX_COL_LOC, 4, Ren::eType::Uint8_unorm, sizeof(vertex_t), offsetof(vertex_t, col)},
            {vertex_buf_, VTX_UVS_LOC, 4, Ren::eType::Uint16_unorm, sizeof(vertex_t), offsetof(vertex_t, uvs)}};
        vtx_input_ = ctx_.LoadVertexInput(attribs, index_buf_);
    }

    { // create graphics pipeline
        Ren::RastState rast_state;
        rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

        rast_state.blend.enabled = true;
        rast_state.blend.src_color = rast_state.blend.src_alpha = uint8_t(Ren::eBlendFactor::SrcAlpha);
        rast_state.blend.dst_color = rast_state.blend.dst_alpha = uint8_t(Ren::eBlendFactor::OneMinusSrcAlpha);

        if (!pipeline_.Init(api_ctx, rast_state, std::move(ui_program), vtx_input_, render_pass_, 0, ctx_.log())) {
            ctx_.log()->Error("[Gui::Renderer::Init]: Failed to create graphics pipeline!");
            return false;
        }
    }

    return true;
}

void Gui::Renderer::PushClipArea(const Vec2f dims[2]) {
    auto new_clip = Vec4f{dims[0][0], dims[0][1], dims[0][0] + dims[1][0], dims[0][1] + dims[1][1]};
    if (!clip_area_stack_.empty()) {
        new_clip[0] = fmaxf(new_clip[0], clip_area_stack_.back()[0]);
        new_clip[1] = fmaxf(new_clip[1], clip_area_stack_.back()[1]);
        new_clip[2] = fminf(new_clip[2], clip_area_stack_.back()[2]);
        new_clip[3] = fminf(new_clip[3], clip_area_stack_.back()[3]);
    }
    clip_area_stack_.emplace_back(new_clip);
}

void Gui::Renderer::PopClipArea() { clip_area_stack_.pop_back(); }

std::optional<Gui::Vec4f> Gui::Renderer::GetClipArea() const {
    if (!clip_area_stack_.empty()) {
        return clip_area_stack_.back();
    }
    return {};
}

int Gui::Renderer::AcquireVertexData(vertex_t **vertex_data, int *vertex_avail, uint16_t **index_data,
                                     int *index_avail) {
    (*vertex_data) = vtx_stage_data_ + vtx_count_[ctx_.next_frontend_frame];
    (*vertex_avail) = MaxVerticesPerRange - vtx_count_[ctx_.next_frontend_frame];

    (*index_data) = ndx_stage_data_ + ndx_count_[ctx_.next_frontend_frame];
    (*index_avail) = MaxIndicesPerRange - ndx_count_[ctx_.next_frontend_frame];

    if (ctx_.capabilities.persistent_buf_mapping) {
        (*vertex_data) += size_t(ctx_.next_frontend_frame) * MaxVerticesPerRange;
        (*index_data) += size_t(ctx_.next_frontend_frame) * MaxIndicesPerRange;
    }

    return vtx_count_[ctx_.next_frontend_frame];
}

void Gui::Renderer::SubmitVertexData(const int vertex_count, const int index_count) {
    vtx_count_[ctx_.next_frontend_frame] += vertex_count;
    ndx_count_[ctx_.next_frontend_frame] += index_count;
    assert(vtx_count_[ctx_.next_frontend_frame] <= MaxVerticesPerRange &&
           ndx_count_[ctx_.next_frontend_frame] <= MaxIndicesPerRange);
}

void Gui::Renderer::PushImageQuad(const eDrawMode draw_mode, const int tex_layer, const uint8_t color[4],
                                  const Vec2f pos[2], const Vec2f uvs_px[2]) {
    const Vec2f uvs_scale = 1.0f / Vec2f{float(Ren::TextureAtlasWidth), float(Ren::TextureAtlasHeight)};
    Vec4f pos_uvs[2] = {Vec4f{pos[0][0], pos[0][1], uvs_px[0][0] * uvs_scale[0], uvs_px[0][1] * uvs_scale[1]},
                        Vec4f{pos[1][0], pos[1][1], uvs_px[1][0] * uvs_scale[0], uvs_px[1][1] * uvs_scale[1]}};

    const std::optional<Vec4f> clip = GetClipArea();
    if (clip && !ClipQuadToArea(pos_uvs, *clip)) {
        return;
    }

    vertex_t *vtx_data;
    int vtx_avail;
    uint16_t *ndx_data;
    int ndx_avail;
    int ndx_offset = AcquireVertexData(&vtx_data, &vtx_avail, &ndx_data, &ndx_avail);
    assert(vtx_avail >= 4 && ndx_avail >= 6);

    vertex_t *cur_vtx = vtx_data;
    uint16_t *cur_ndx = ndx_data;

    const uint16_t u16_tex_layer = f32_to_u16((1.0f / 16.0f) * float(tex_layer));

    cur_vtx->pos[0] = pos_uvs[0][0];
    cur_vtx->pos[1] = pos_uvs[0][1];
    cur_vtx->pos[2] = 0;
    memcpy(cur_vtx->col, color, 4);
    cur_vtx->uvs[0] = f32_to_u16(pos_uvs[0][2]);
    cur_vtx->uvs[1] = f32_to_u16(pos_uvs[0][3]);
    cur_vtx->uvs[2] = u16_tex_layer;
    cur_vtx->uvs[3] = g_draw_mode_u16[int(draw_mode)];
    ++cur_vtx;

    cur_vtx->pos[0] = pos_uvs[1][0];
    cur_vtx->pos[1] = pos_uvs[0][1];
    cur_vtx->pos[2] = 0;
    memcpy(cur_vtx->col, color, 4);
    cur_vtx->uvs[0] = f32_to_u16(pos_uvs[1][2]);
    cur_vtx->uvs[1] = f32_to_u16(pos_uvs[0][3]);
    cur_vtx->uvs[2] = u16_tex_layer;
    cur_vtx->uvs[3] = g_draw_mode_u16[int(draw_mode)];
    ++cur_vtx;

    cur_vtx->pos[0] = pos_uvs[1][0];
    cur_vtx->pos[1] = pos_uvs[1][1];
    cur_vtx->pos[2] = 0;
    memcpy(cur_vtx->col, color, 4);
    cur_vtx->uvs[0] = f32_to_u16(pos_uvs[1][2]);
    cur_vtx->uvs[1] = f32_to_u16(pos_uvs[1][3]);
    cur_vtx->uvs[2] = u16_tex_layer;
    cur_vtx->uvs[3] = g_draw_mode_u16[int(draw_mode)];
    ++cur_vtx;

    cur_vtx->pos[0] = pos_uvs[0][0];
    cur_vtx->pos[1] = pos_uvs[1][1];
    cur_vtx->pos[2] = 0;
    memcpy(cur_vtx->col, color, 4);
    cur_vtx->uvs[0] = f32_to_u16(pos_uvs[0][2]);
    cur_vtx->uvs[1] = f32_to_u16(pos_uvs[1][3]);
    cur_vtx->uvs[2] = u16_tex_layer;
    cur_vtx->uvs[3] = g_draw_mode_u16[int(draw_mode)];
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

    const Vec4f perp[2] = {Vec4f{thickness} * Vec4f{-d0[1], d0[0], 1, 0},
                           Vec4f{thickness} * Vec4f{-d1[1], d1[0], 1, 0}};

    Vec4f pos_uvs[8] = {p0 - perp[0], p1 - perp[1], p1 + perp[1], p0 + perp[0]};
    int vertex_count = 4;

    for (int i = 0; i < vertex_count; i++) {
        pos_uvs[i][2] *= uvs_scale[0];
        pos_uvs[i][3] *= uvs_scale[1];
    }

    const std::optional<Vec4f> clip = GetClipArea();
    if (clip && !(vertex_count = ClipPolyToArea(pos_uvs, vertex_count, *clip))) {
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
        cur_vtx->pos[2] = 0;
        memcpy(cur_vtx->col, color, 4);
        cur_vtx->uvs[0] = f32_to_u16(pos_uvs[i][2]);
        cur_vtx->uvs[1] = f32_to_u16(pos_uvs[i][3]);
        cur_vtx->uvs[2] = u16_tex_layer;
        cur_vtx->uvs[3] = g_draw_mode_u16[int(draw_mode)];
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