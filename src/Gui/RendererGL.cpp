#include "Renderer.h"

#include <cassert>

#include <Ren/GL.h>
#include <Sys/Json.h>

namespace UIRendererConstants {
enum {
    U_COL,
    U_TEXTURE,
    U_Z_OFFSET
};

const char vs_source[] =
R"(#version 310 es
/*
ATTRIBUTES
	aVertexPosition : 0
	aVertexUVs : 1
UNIFORMS
	z_offset : 2
*/

uniform float z_offset;

in vec3 aVertexPosition;
in vec2 aVertexUVs;

out vec2 aVertexUVs_;

void main(void) {
    gl_Position = vec4(aVertexPosition + vec3(0.0, 0.0, z_offset), 1.0);
    aVertexUVs_ = aVertexUVs;
}
)";

const char fs_source[] =
R"(#version 310 es
#ifdef GL_ES
	precision mediump float;
#else
	#define lowp
	#define mediump
	#define highp
#endif

/*
UNIFORMS
	col_and_mode : 0
	s_texture : 1
*/

uniform vec4 col_and_mode;
uniform sampler2D s_texture;

in vec2 aVertexUVs_;

out vec4	outColor;

float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

void main(void) {
    vec4 tex_color = texture(s_texture, aVertexUVs_);

    if (col_and_mode.w < 0.5) {
        // Simple texture drawing
	    outColor = vec4(col_and_mode.rgb, 1.0) * tex_color;
    } else {
        // SDF drawing
        float sig_dist = median(tex_color.r, tex_color.g, tex_color.b);
        
        float s = sig_dist - 0.5;
        float v = s / fwidth(s);
        
        vec4 base_color;
        base_color.rgb = vec3(1.0);
        base_color.a = clamp(v + 0.5, 0.0, 1.0);
        
        outColor = vec4(col_and_mode.rgb, 1.0) * base_color;
    }
}
)";

inline void BindTexture(int slot, uint32_t tex) {
    glActiveTexture((GLenum)(GL_TEXTURE0 + slot));
    glBindTexture(GL_TEXTURE_2D, (GLuint)tex);
}

const int MAX_VERTICES = 1024;
const int MAX_INDICES = 2048;
}

Gui::Renderer::Renderer(Ren::Context &ctx, const JsObject &config) : ctx_(ctx) {
    using namespace UIRendererConstants;

    const JsString &js_gl_defines = (const JsString &)config.at(GL_DEFINES_KEY);

    {
        // Load main shader
        Ren::eProgLoadStatus status;
        ui_program_ = ctx_.LoadProgramGLSL(UI_PROGRAM_NAME, vs_source, fs_source, &status);
        assert(status == Ren::ProgCreatedFromData);
    }

    glGenVertexArrays(1, &main_vao_);
    glBindVertexArray(main_vao_);

    glGenBuffers(1, &attribs_buf_id_);
    glBindBuffer(GL_ARRAY_BUFFER, attribs_buf_id_);
    glBufferData(GL_ARRAY_BUFFER, MAX_VERTICES * 5 * sizeof(GLfloat), nullptr, GL_DYNAMIC_DRAW);

    glGenBuffers(1, &indices_buf_id_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices_buf_id_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, MAX_INDICES * sizeof(GLushort), nullptr, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray((GLuint)ui_program_->attribute(0).loc);
    glEnableVertexAttribArray((GLuint)ui_program_->attribute(1).loc);
}

Gui::Renderer::~Renderer() {
    glDeleteVertexArrays(1, &main_vao_);
    glDeleteBuffers(1, &attribs_buf_id_);
    glDeleteBuffers(1, &indices_buf_id_);
}

void Gui::Renderer::BeginDraw() {
    glUseProgram(ui_program_->prog_id());

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_SCISSOR_TEST);
#if !defined(__ANDROID__)
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
#endif

    Vec2i scissor_test[2] = { { 0, 0 }, { ctx_.w(), ctx_.h() } };
    this->EmplaceParams(Vec4f(1.0f, 1.0f, 1.0f, 0.0f), 0.0f, BlAlpha, scissor_test);

    glBindVertexArray(main_vao_);
    glBindBuffer(GL_ARRAY_BUFFER, attribs_buf_id_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices_buf_id_);

#ifndef DISABLE_MARKERS
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "UI DRAW");
#endif
}

void Gui::Renderer::EndDraw() {
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);

    this->PopParams();

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

#ifndef DISABLE_MARKERS
    glPopDebugGroup();
#endif
}

void Gui::Renderer::DrawImageQuad(const Ren::Texture2DRef &tex, const Vec2f dims[2], const Vec2f uvs[2]) {
    using namespace UIRendererConstants;

    float vertices[] = { dims[0][0], dims[0][1], 0,
                         uvs[0][0], uvs[0][1],

                         dims[0][0], dims[0][1] + dims[1][1], 0,
                         uvs[0][0], uvs[1][1],

                         dims[0][0] + dims[1][0], dims[0][1] + dims[1][1], 0,
                         uvs[1][0], uvs[1][1],

                         dims[0][0] + dims[1][0], dims[0][1], 0,
                         uvs[1][0], uvs[0][1]
                       };

    unsigned char indices[] = { 2, 1, 0,
                                3, 2, 0 };

    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, sizeof(indices), indices);

    const DrawParams &cur_params = params_.back();
    this->ApplyParams(ui_program_, cur_params);

    BindTexture(0, tex->tex_id());
    glUniform1i(ui_program_->uniform(U_TEXTURE).loc, 0);

    glVertexAttribPointer((GLuint)ui_program_->attribute(0).loc, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)0);
    glVertexAttribPointer((GLuint)ui_program_->attribute(1).loc, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)(3 * sizeof(float)));

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, 0);
}

void Gui::Renderer::DrawUIElement(const Ren::Texture2DRef &tex, ePrimitiveType prim_type,
                                  const std::vector<float> &pos, const std::vector<float> &uvs,
                                  const std::vector<uint16_t> &indices) {
    using namespace UIRendererConstants;

    assert(pos.size() / 5 < MAX_VERTICES);
    assert(indices.size() < MAX_INDICES);
    if (pos.empty()) return;

    const DrawParams &cur_params = params_.back();
    this->ApplyParams(ui_program_, cur_params);

    BindTexture(0, tex->tex_id());
    glUniform1i(ui_program_->uniform(U_TEXTURE).loc, 0);

    glBufferSubData(GL_ARRAY_BUFFER, 0, pos.size() * sizeof(GLfloat), &pos[0]);
    glBufferSubData(GL_ARRAY_BUFFER, pos.size() * sizeof(GLfloat), uvs.size() * sizeof(GLfloat), &uvs[0]);

    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, indices.size() * sizeof(GLushort), &indices[0]);

    glVertexAttribPointer((GLuint)ui_program_->attribute(0).loc, 3, GL_FLOAT, GL_FALSE, 0, (void *)0);
    glVertexAttribPointer((GLuint)ui_program_->attribute(1).loc, 2, GL_FLOAT, GL_FALSE, 0, (void *)((uintptr_t)pos.size() * sizeof(GLfloat)));

    if (prim_type == PrimTriangle) {
        glDrawElements(GL_TRIANGLES, (GLsizei)indices.size(), GL_UNSIGNED_SHORT, 0);
    }
}

void Gui::Renderer::ApplyParams(Ren::ProgramRef &p, const DrawParams &params) {
    using namespace UIRendererConstants;
    //int val = p->uniform(U_COL).loc;
    glUniform4fv(p->uniform(U_COL).loc, 1, ValuePtr(params.col_and_mode_));
    glUniform1f(p->uniform(U_Z_OFFSET).loc, params.z_val_);

    if (params.blend_mode_ == BlAlpha) {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    } else if (params.blend_mode_ == BlColor) {
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_COLOR);
    }

    if (params.scissor_test_[1][0] > 0 && params.scissor_test_[1][1] > 0) {
        glScissor(params.scissor_test_[0][0], params.scissor_test_[0][1],
                  params.scissor_test_[1][0], params.scissor_test_[1][1]);
    }
}
