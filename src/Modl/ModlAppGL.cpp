#include "ModlApp.h"

#include <Ren/GL.h>

#include <Ren/MMat.h>

namespace {
#define _AS_STR(x) #x
#define AS_STR(x) _AS_STR(x)

const int A_POS = 0;
const int A_NORMAL = 1;
const int A_TANGENT = 2;
const int A_UVS1 = 3;
const int A_ATTRIB = 4;
const int A_INDICES = 5;
const int A_WEIGHTS = 6;

const int U_MVP_MATR = 0;
const int U_M_MATR = 1;
const int U_MODE = 2;
const int U_M_PALETTE = 3;

const int DIFFUSEMAP_SLOT = 0;
const int NORMALMAP_SLOT = 1;

const int BONE_MATRICES_UBO = 0;

const int UniformBufferSize = 16 * 1024;

inline void BindTexture(const int slot, const uint32_t tex) {
    glActiveTexture(GLenum(GL_TEXTURE0 + slot));
    glBindTexture(GL_TEXTURE_2D, GLuint(tex));
}
} // namespace

void ModlApp::DrawMeshSimple(const Ren::MeshRef &ref) {
    using namespace Ren;

    const Mesh *m = ref.get();
    const Material *mat = m->groups()[0].mat.get();
    ProgramRef p = mat->pipelines[0]->prog();

    glBindBuffer(GL_ARRAY_BUFFER, m->attribs_buf1_id());
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m->indices_buf_id());

    p = diag_prog_;
    glUniform1f(U_MODE, float(view_mode_));

    CheckInitVAOs();

    glBindVertexArray(GLuint(simple_vao_));

    glUseProgram(p->id());

    Mat4f world_from_object = Mat4f{1.0f};

    world_from_object = Translate(world_from_object, Vec3f{offset_x_, offset_y_, 0});
    world_from_object = Rotate(world_from_object, angle_x_, Vec3f{1, 0, 0});
    world_from_object = Rotate(world_from_object, angle_y_, Vec3f{0, 1, 0});

    Mat4f view_from_world = cam_.view_matrix(), proj_from_view = cam_.proj_matrix();

    Mat4f view_from_object = view_from_world * world_from_object,
          proj_from_object = proj_from_view * view_from_object;

    glUniformMatrix4fv(U_MVP_MATR, 1, GL_FALSE, ValuePtr(proj_from_object));
    glUniformMatrix4fv(U_M_MATR, 1, GL_FALSE, ValuePtr(world_from_object));

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    for (const auto &grp : m->groups()) {
        const Ren::Material *mat = grp.mat.get();

        if (view_mode_ == eViewMode::DiagUVs1 || view_mode_ == eViewMode::DiagUVs2) {
            BindTexture(DIFFUSEMAP_SLOT, checker_tex_->id());
        } else {
            BindTexture(DIFFUSEMAP_SLOT, mat->textures[0]->id());
        }
        BindTexture(NORMALMAP_SLOT, mat->textures[1]->id());

        glDrawElements(GL_TRIANGLES, grp.num_indices, GL_UNSIGNED_INT,
                       reinterpret_cast<void *>(uintptr_t(grp.offset)));
    }

    Ren::CheckError("", &log_);
}

void ModlApp::DrawMeshColored(const Ren::MeshRef &ref) {
    using namespace Ren;

    const Mesh *m = ref.get();
    const Material *mat = m->groups()[0].mat.get();
    ProgramRef p = mat->pipelines[0]->prog();

    glBindBuffer(GL_ARRAY_BUFFER, m->attribs_buf1_id());
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m->indices_buf_id());

    p = diag_colored_prog_;
    glUniform1f(U_MODE, float(view_mode_));

    CheckInitVAOs();

    glBindVertexArray(GLuint(simple_vao_));

    glUseProgram(p->id());

    Mat4f world_from_object = Mat4f{1.0f};

    world_from_object = Translate(world_from_object, Vec3f{offset_x_, offset_y_, 0});
    world_from_object = Rotate(world_from_object, angle_x_, Vec3f{1, 0, 0});
    world_from_object = Rotate(world_from_object, angle_y_, Vec3f{0, 1, 0});

    const Mat4f view_from_world = cam_.view_matrix(), proj_from_view = cam_.proj_matrix();

    const Mat4f view_from_object = view_from_world * world_from_object,
                proj_from_object = proj_from_view * view_from_object;

    glUniformMatrix4fv(U_MVP_MATR, 1, GL_FALSE, ValuePtr(proj_from_object));
    glUniformMatrix4fv(U_M_MATR, 1, GL_FALSE, ValuePtr(world_from_object));

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    for (const auto &grp : m->groups()) {
        const Ren::Material *mat = grp.mat.get();

        if (view_mode_ == eViewMode::DiagUVs1) {
            BindTexture(DIFFUSEMAP_SLOT, checker_tex_->id());
        } else if (view_mode_ == eViewMode::DiagVtxColor) {
        } else {
            BindTexture(DIFFUSEMAP_SLOT, mat->textures[0]->id());
        }
        BindTexture(NORMALMAP_SLOT, mat->textures[1]->id());

        glDrawElements(GL_TRIANGLES, grp.num_indices, GL_UNSIGNED_INT,
                       reinterpret_cast<const void *>(uintptr_t(grp.offset)));
    }

    Ren::CheckError("", &log_);
}

void ModlApp::DrawMeshSkeletal(Ren::MeshRef &ref, const float dt_s) {
    using namespace Ren;

    Ren::Mesh *m = ref.get();
    const Ren::Material *mat = m->groups()[0].mat.get();

    anim_time_ += dt_s;

    Ren::Skeleton *skel = m->skel();
    if (!skel->anims.empty()) {
        skel->UpdateAnim(0, anim_time_);
        skel->ApplyAnim(0);
    }

    skel->UpdateBones(matr_palette_);

    CheckInitVAOs();

#if 0
    glBindVertexArray((GLuint)skinned_vao_);

    const Ren::Program *p = diag_skinned_prog_.get();
    glUseProgram(p->prog_id());

    glUniform1f(U_MODE, (float)view_mode_);

    Mat4f world_from_object = Mat4f{ 1.0f };

    world_from_object = Translate(world_from_object,Vec3f{offset_x_, offset_y_, 0});
    //world_from_object = Rotate(world_from_object, angle_x_, Vec3f{ 1, 0, 0 });
    world_from_object = Rotate(world_from_object, angle_y_, Vec3f{ 0, 1, 0 });

    Mat4f view_from_world = cam_.view_matrix(),
          proj_from_view = cam_.proj_matrix();

    Mat4f view_from_object = view_from_world * world_from_object,
          proj_from_object = proj_from_view * view_from_object;

    glUniformMatrix4fv(U_MVP_MATR, 1, GL_FALSE, ValuePtr(proj_from_object));
    glUniformMatrix4fv(U_M_MATR, 1, GL_FALSE, ValuePtr(world_from_object));

    size_t num_bones = skel->bones.size();
    glUniformMatrix4fv(U_M_PALETTE, (GLsizei)num_bones, GL_FALSE, ValuePtr(matr_palette_[0]));

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    const Ren::TriGroup *s = &m->group(0);
    while (s->offset != -1) {
        const Ren::Material *mat = s->mat.get();
        
        if (view_mode_ == DiagUVs1 || view_mode_ == DiagUVs2) {
            BindTexture(DIFFUSEMAP_SLOT, checker_tex_->tex_id());
        } else {
            BindTexture(DIFFUSEMAP_SLOT, mat->texture(0)->tex_id());
        }
        BindTexture(NORMALMAP_SLOT, mat->texture(1)->tex_id());

        glDrawElementsBaseVertex(GL_TRIANGLES, s->num_indices, GL_UNSIGNED_INT, (void *)uintptr_t(s->offset), (GLint)0);
        ++s;
    }

    Ren::CheckError("", &log_);
#else

    { // update matrices buffer
        Mat3x4f _matr_palette[256];
        for (int i = 0; i < skel->bones_count; i++) {
            const Mat4f tr_mat = Ren::Transpose(matr_palette_[i]);
            memcpy(&_matr_palette[i][0][0], ValuePtr(tr_mat), 12 * sizeof(float));
        }

        glBindBuffer(GL_UNIFORM_BUFFER, GLuint(uniform_buf_));
        glBufferSubData(GL_UNIFORM_BUFFER, 0, UniformBufferSize, ValuePtr(_matr_palette));
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }

    { // transform vertices
        const Ren::Program *p = skinning_prog_.get();

        glUseProgram(p->id());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, GLuint(last_skin_vertex_buffer_));
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, GLuint(last_delta_buffer_));
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, GLuint(last_vertex_buf1_));
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, GLuint(last_vertex_buf2_));
        glBindBufferBase(GL_UNIFORM_BUFFER, BONE_MATRICES_UBO, GLuint(uniform_buf_));

        const Ren::BufferRange &sk_attribs_buf = m->sk_attribs_buf();
        const Ren::BufferRange &sk_deltas_buf = m->sk_deltas_buf();

        const int vertex_offset = int(sk_attribs_buf.offset) / 48,
                  vertex_count = int(sk_attribs_buf.size) / 48;
        const int delta_offset = int(sk_deltas_buf.offset) / 24,
                  delta_count = int(sk_deltas_buf.size) / 24;

        if (skel->shapes_count && (shape_key_index_ != -1 || !skel->anims.empty())) {
            // assume all shape keys have same vertex count
            const int shapekeyed_vertex_count = skel->shapes[0].delta_count;
            const int non_shapekeyed_vertex_count =
                vertex_count - shapekeyed_vertex_count;

            { // transform simple vertices
                glUniform4i(0, vertex_offset, non_shapekeyed_vertex_count,
                            0 /* offset to out buffers */, delta_offset);
                glUniform1i(1, 0);

                const int group_count = (non_shapekeyed_vertex_count + 63) / 64;
                glDispatchCompute(group_count, 1, 1);
            }

            { // transform shapekeyed vertices
                glUniform4i(0, vertex_offset + non_shapekeyed_vertex_count,
                            shapekeyed_vertex_count,
                            non_shapekeyed_vertex_count /* offset to out buffers */,
                            delta_offset);

                uint16_t shape_palette[256];

                if (shape_key_index_ != -1) {
                    static float f = 0.0f, s = 1.0f;
                    f += 0.05f * s;
                    if (f < 0.0f) {
                        s = 1.0f;
                    } else if (f > 1.0f) {
                        s = -1.0f;
                    }
                    f = Ren::Clamp(f, 0.0f, 1.0f);

                    shape_palette[0] = uint16_t(shape_key_index_);
                    shape_palette[1] = uint16_t(f * 65535);

                    glUniform1i(1, 1);
                    glUniform1uiv(2, 1, (const GLuint *)&shape_palette[0]);
                } else {
                    const int shapes_count = skel->UpdateShapes(shape_palette);
                    glUniform1i(1, shapes_count);
                    glUniform1uiv(2, shapes_count, (const GLuint *)&shape_palette[0]);
                }

                const int group_count = (shapekeyed_vertex_count + 63) / 64;
                glDispatchCompute(group_count, 1, 1);
            }
        } else {
            glUniform4i(0, vertex_offset, vertex_count, 0 /* offset to out buffers */,
                        delta_offset);
            glUniform1i(1, 0);

            const int group_count = (vertex_count + 63) / 64;
            glDispatchCompute(group_count, 1, 1);
        }
    }

    glBindVertexArray(GLuint(simple_vao_));

    const Ren::Program *p = diag_prog_.get();
    glUseProgram(p->id());

    glUniform1f(U_MODE, (float)view_mode_);

    Mat4f world_from_object = Mat4f{1.0f};

    world_from_object = Translate(world_from_object, Vec3f{offset_x_, offset_y_, 0});
    // world_from_object = Rotate(world_from_object, angle_x_, { 1, 0, 0 });
    world_from_object = Rotate(world_from_object, angle_y_, Vec3f{0, 1, 0});

    Mat4f view_from_world = cam_.view_matrix(), proj_from_view = cam_.proj_matrix();

    Mat4f view_from_object = view_from_world * world_from_object,
          proj_from_object = proj_from_view * view_from_object;

    glUniformMatrix4fv(U_MVP_MATR, 1, GL_FALSE, ValuePtr(proj_from_object));
    glUniformMatrix4fv(U_M_MATR, 1, GL_FALSE, ValuePtr(world_from_object));

    glCullFace(GL_BACK);

    for (const auto &grp : m->groups()) {
        const Ren::Material *mat = grp.mat.get();

        if ((mat->flags() & uint32_t(Ren::eMatFlags::TwoSided)) != 0) {
            glDisable(GL_CULL_FACE);
        } else {
            glEnable(GL_CULL_FACE);
        }

        if (view_mode_ == eViewMode::DiagUVs1 || view_mode_ == eViewMode::DiagUVs2) {
            BindTexture(DIFFUSEMAP_SLOT, checker_tex_->id());
        } else {
            BindTexture(DIFFUSEMAP_SLOT, mat->textures[0]->id());
        }
        BindTexture(NORMALMAP_SLOT, mat->textures[1]->id());

        glDrawElementsBaseVertex(GL_TRIANGLES, grp.num_indices, GL_UNSIGNED_INT,
                                 reinterpret_cast<void *>(uintptr_t(grp.offset)), 0);
    }

    Ren::CheckError("", &log_);
#endif
}

void ModlApp::ClearColorAndDepth(const float r, const float g, const float b, const float a) {
    glClearColor(r, g, b, a);
    glClear(GLbitfield(GL_COLOR_BUFFER_BIT) | GLbitfield(GL_DEPTH_BUFFER_BIT));
}

void ModlApp::CheckInitVAOs() {
    Ren::BufferRef vtx_buf1 = ctx_.default_vertex_buf1(),
                   vtx_buf2 = ctx_.default_vertex_buf2();
    Ren::BufferRef skin_vtx_buf = ctx_.default_skin_vertex_buf();
    Ren::BufferRef delta_buf = ctx_.default_delta_buf();
    Ren::BufferRef ndx_buf = ctx_.default_indices_buf();

    const auto gl_vertex_buf1 = GLuint(vtx_buf1->id()),
               gl_vertex_buf2 = GLuint(vtx_buf2->id()),
               gl_skin_vertex_buf = GLuint(skin_vtx_buf->id()),
               gl_delta_buf = GLuint(delta_buf->id()),
               gl_indices_buf = GLuint(ndx_buf->id());

    if (gl_vertex_buf1 != last_vertex_buf1_ || gl_vertex_buf2 != last_vertex_buf2_ ||
        gl_skin_vertex_buf != last_skin_vertex_buffer_ ||
        gl_delta_buf != last_delta_buffer_ || gl_indices_buf != last_index_buffer_) {

        if (last_vertex_buf1_) {
            auto simple_mesh_vao = GLuint(simple_vao_);
            glDeleteVertexArrays(1, &simple_mesh_vao);

            auto skinned_mesh_vao = GLuint(skinned_vao_);
            glDeleteVertexArrays(1, &skinned_mesh_vao);
        }

        GLuint simple_mesh_vao;

        glGenVertexArrays(1, &simple_mesh_vao);
        glBindVertexArray(simple_mesh_vao);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl_indices_buf);

        const int buf1_stride = 16, buf2_stride = 16;

        { // Assign attributes from buf1
            glBindBuffer(GL_ARRAY_BUFFER, gl_vertex_buf1);

            glEnableVertexAttribArray(A_POS);
            glVertexAttribPointer(A_POS, 3, GL_FLOAT, GL_FALSE, buf1_stride, (void *)0);

            glEnableVertexAttribArray(A_UVS1);
            glVertexAttribPointer(A_UVS1, 2, GL_HALF_FLOAT, GL_FALSE, buf1_stride,
                                  (void *)(3 * sizeof(float)));
        }

        { // Assign attributes from buf2
            glBindBuffer(GL_ARRAY_BUFFER, gl_vertex_buf2);

            glEnableVertexAttribArray(A_NORMAL);
            glVertexAttribPointer(A_NORMAL, 4, GL_SHORT, GL_TRUE, buf2_stride, (void *)0);

            glEnableVertexAttribArray(A_TANGENT);
            glVertexAttribPointer(A_TANGENT, 2, GL_SHORT, GL_TRUE, buf2_stride,
                                  (void *)(4 * sizeof(uint16_t)));

            glEnableVertexAttribArray(A_ATTRIB);
            glVertexAttribIPointer(A_ATTRIB, 1, GL_UNSIGNED_INT, buf2_stride,
                                   (void *)(6 * sizeof(uint16_t)));
        }

        glBindVertexArray(0);

        simple_vao_ = uint32_t(simple_mesh_vao);

        GLuint skinned_mesh_vao;
        glGenVertexArrays(1, &skinned_mesh_vao);
        glBindVertexArray(skinned_mesh_vao);

        glBindBuffer(GL_ARRAY_BUFFER, gl_skin_vertex_buf);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl_indices_buf);

        const int stride_skin_buf = 48;
        glEnableVertexAttribArray(A_POS);
        glVertexAttribPointer(A_POS, 3, GL_FLOAT, GL_FALSE, stride_skin_buf, (void *)0);

        glEnableVertexAttribArray(A_NORMAL);
        glVertexAttribPointer(A_NORMAL, 4, GL_SHORT, GL_TRUE, stride_skin_buf,
                              (void *)(3 * sizeof(float)));

        glEnableVertexAttribArray(A_TANGENT);
        glVertexAttribPointer(A_TANGENT, 2, GL_SHORT, GL_TRUE, stride_skin_buf,
                              (void *)(3 * sizeof(float) + 4 * sizeof(int16_t)));

        glEnableVertexAttribArray(A_UVS1);
        glVertexAttribPointer(A_UVS1, 2, GL_HALF_FLOAT, GL_FALSE, stride_skin_buf,
                              (void *)(3 * sizeof(float) + 6 * sizeof(int16_t)));

        glEnableVertexAttribArray(A_ATTRIB);
        glVertexAttribIPointer(A_ATTRIB, 1, GL_UNSIGNED_INT, buf2_stride,
                               (void *)(6 * sizeof(uint16_t)));

        glEnableVertexAttribArray(A_INDICES);
        glVertexAttribPointer(
            A_INDICES, 4, GL_UNSIGNED_SHORT, GL_FALSE, stride_skin_buf,
            (void *)(3 * sizeof(float) + 6 * sizeof(int16_t) + 4 * sizeof(uint16_t)));

        glEnableVertexAttribArray(A_WEIGHTS);
        glVertexAttribPointer(
            A_WEIGHTS, 4, GL_UNSIGNED_SHORT, GL_TRUE, stride_skin_buf,
            (void *)(3 * sizeof(float) + 6 * sizeof(int16_t) + 8 * sizeof(uint16_t)));

        glBindVertexArray(0);

        skinned_vao_ = uint32_t(skinned_mesh_vao);

        last_skin_vertex_buffer_ = uint32_t(gl_skin_vertex_buf);
        last_delta_buffer_ = uint32_t(gl_delta_buf);
        last_vertex_buf1_ = uint32_t(gl_vertex_buf1);
        last_vertex_buf2_ = uint32_t(gl_vertex_buf2);
        last_index_buffer_ = uint32_t(gl_indices_buf);
    }
}

void ModlApp::InitInternal() {
    static const char diag_vs[] =
        R"(#version 430

layout(location = 0) in vec3 aVertexPosition;
layout(location = 1) in vec4 aVertexNormal;
layout(location = 2) in vec2 aVertexTangent;
layout(location = 3) in vec2 aVertexUVs1;
layout(location = 4) in uint aVertexUVs2Packed;

layout(location = 0) uniform mat4 uMVPMatrix;
layout(location = 1) uniform mat4 uMMatrix;

out mat3 aVertexTBN_;
out vec2 aVertexUVs1_;
out vec4 aVertexAttrib_;

void main(void) {
    vec3 vertex_normal_ws = normalize((uMMatrix * vec4(aVertexNormal.xyz, 0.0)).xyz);
    vec3 vertex_tangent_ws = normalize((uMMatrix * vec4(aVertexNormal.w, aVertexTangent, 0.0)).xyz);

    aVertexTBN_ = mat3(vertex_tangent_ws, cross(vertex_normal_ws, vertex_tangent_ws), vertex_normal_ws);
    aVertexUVs1_ = aVertexUVs1;
    aVertexAttrib_ = vec4(unpackHalf2x16(aVertexUVs2Packed), 0.0, 0.0);

    gl_Position = uMVPMatrix * vec4(aVertexPosition, 1.0);
}
)";

    static const char diag_colored_vs[] =
        R"(#version 430

layout(location = 0) in vec3 aVertexPosition;
layout(location = 1) in vec4 aVertexNormal;
layout(location = 2) in vec2 aVertexTangent;
layout(location = 3) in vec2 aVertexUVs1;
layout(location = 4) in uint aVertexColorPacked;

layout(location = 0) uniform mat4 uMVPMatrix;
layout(location = 1) uniform mat4 uMMatrix;

out mat3 aVertexTBN_;
out vec2 aVertexUVs1_;
out vec4 aVertexAttrib_;

void main(void) {
    vec3 vertex_normal_ws = normalize((uMMatrix * vec4(aVertexNormal.xyz, 0.0)).xyz);
    vec3 vertex_tangent_ws = normalize((uMMatrix * vec4(aVertexNormal.w, aVertexTangent, 0.0)).xyz);

    aVertexTBN_ = mat3(vertex_tangent_ws, cross(vertex_normal_ws, vertex_tangent_ws), vertex_normal_ws);
    aVertexUVs1_ = aVertexUVs1;
    aVertexAttrib_ = unpackUnorm4x8(aVertexColorPacked);

    gl_Position = uMVPMatrix * vec4(aVertexPosition, 1.0);
}
)";

    static const char diag_skinned_vs[] =
        R"(#version 430

layout(location = 0) in vec3 aVertexPosition;
layout(location = 1) in mediump vec4 aVertexNormal;
layout(location = 2) in mediump vec2 aVertexTangent;
layout(location = 3) in mediump vec2 aVertexUVs1;
layout(location = 4) in highp uint aVertexColorPacked;//aVertexUVs2Packed;
layout(location = 5) in mediump vec4 aVertexIndices;
layout(location = 6) in mediump vec4 aVertexWeights;

layout(location = 0) uniform mat4 uMVPMatrix;
layout(location = 1) uniform mat4 uMMatrix;
layout(location = 3) uniform mat4 uMPalette[160];

out mat3 aVertexTBN_;
out vec2 aVertexUVs1_;
out vec4 aVertexAttrib_;

void main(void) {
    uvec4 vtx_indices = uvec4(aVertexIndices);
    vec4 vtx_weights = aVertexWeights;

    mat4 mat = uMPalette[vtx_indices.x] * aVertexWeights.x;
    for(int i = 0; i < 3; i++) {
        vtx_indices = vtx_indices.yzwx;
        vtx_weights = vtx_weights.yzwx;
        if(vtx_weights.x > 0.0) {
            mat = mat + uMPalette[vtx_indices.x] * vtx_weights.x;
        }
    }

    mat = uMMatrix * mat;

    vec3 vertex_normal_ws = normalize((mat * vec4(aVertexNormal.xyz, 0.0)).xyz);
    vec3 vertex_tangent_ws = normalize((mat * vec4(aVertexNormal.w, aVertexTangent, 0.0)).xyz);

    aVertexTBN_ = mat3(vertex_tangent_ws, cross(vertex_normal_ws, vertex_tangent_ws),
                       vertex_normal_ws);
    aVertexUVs1_ = aVertexUVs1;
    //aVertexAttrib_ = vec4(unpackHalf2x16(aVertexUVs2Packed), 0.0, 0.0);
    aVertexAttrib_ = unpackUnorm4x8(aVertexColorPacked);

    gl_Position = (uMVPMatrix * mat) * vec4(aVertexPosition, 1.0);
}
)";

    static const char diag_fs[] =
        R"(#version 430

#ifdef GL_ES
    precision mediump float;
#endif

layout(binding = 0) uniform sampler2D diffuse_texture;
layout(binding = 1) uniform sampler2D normals_texture;

layout(location = 2) uniform float mode;
            
in mat3 aVertexTBN_;
in vec2 aVertexUVs1_;
in vec4 aVertexAttrib_;

out vec4 outColor;

void main(void) {
    if (mode < 0.5) {
        vec4 color = texture(diffuse_texture, aVertexUVs1_);
        if (color.a < 0.1) discard;
        outColor = color;
    } else if (mode < 1.5) {
        vec3 normal = aVertexTBN_[2] * 0.5 + vec3(0.5);
        outColor = vec4(normal, 1.0);
    } else if (mode < 2.5) {
        vec3 tangent = aVertexTBN_[0] * 0.5 + vec3(0.5);
        outColor = vec4(tangent, 1.0);
    } else if (mode < 3.5) {
        vec3 tex_normal = texture(normals_texture, aVertexUVs1_).wyz * 2.0 - 1.0;
        outColor = vec4((aVertexTBN_ * tex_normal) * 0.5 + vec3(0.5), 1.0);
    } else if (mode < 4.5) {
        outColor = texture(diffuse_texture, aVertexUVs1_);
    } else if (mode < 5.5) {
        outColor = texture(diffuse_texture, aVertexAttrib_.xy);
    } else if (mode < 6.5) {
        outColor = vec4(aVertexAttrib_.xxx, 1.0);
    } else if (mode < 7.5) {
        outColor = vec4(aVertexAttrib_.yyy, 1.0);
    } else if (mode < 8.5) {
        outColor = vec4(aVertexAttrib_.zzz, 1.0);
    } else if (mode < 9.5) {
        outColor = vec4(aVertexAttrib_.www, 1.0);
    }
}
)";

    Ren::eShaderLoadStatus sh_status;
    Ren::ShaderRef diag_vs_ref =
        ctx_.LoadShaderGLSL("__diag_vs", diag_vs, Ren::eShaderType::Vert, &sh_status);
    assert(sh_status == Ren::eShaderLoadStatus::CreatedFromData);
    Ren::ShaderRef diag_colored_vs_ref = ctx_.LoadShaderGLSL(
        "__diag_colored_vs", diag_colored_vs, Ren::eShaderType::Vert, &sh_status);
    assert(sh_status == Ren::eShaderLoadStatus::CreatedFromData);
    Ren::ShaderRef diag_skinned_vs_ref = ctx_.LoadShaderGLSL(
        "__diag_skinned_vs", diag_skinned_vs, Ren::eShaderType::Vert, &sh_status);
    assert(sh_status == Ren::eShaderLoadStatus::CreatedFromData);
    Ren::ShaderRef diag_fs_ref =
        ctx_.LoadShaderGLSL("__diag_fs", diag_fs, Ren::eShaderType::Frag, &sh_status);
    assert(sh_status == Ren::eShaderLoadStatus::CreatedFromData);

    Ren::eProgLoadStatus status;
    diag_prog_ = ctx_.LoadProgram("__diag", diag_vs_ref, diag_fs_ref, {}, {}, &status);
    assert(status == Ren::eProgLoadStatus::CreatedFromData);
    diag_colored_prog_ = ctx_.LoadProgram("__diag_colored", diag_colored_vs_ref,
                                          diag_fs_ref, {}, {}, &status);
    assert(status == Ren::eProgLoadStatus::CreatedFromData);
    diag_skinned_prog_ = ctx_.LoadProgram("__diag_skinned", diag_skinned_vs_ref,
                                          diag_fs_ref, {}, {}, &status);
    assert(status == Ren::eProgLoadStatus::CreatedFromData);

    static const char skinning_cs[] = R"(
            #version 430

            /*
            UNIFORM_BLOCKS
                SharedDataBlock : )" AS_STR(BONE_MATRICES_UBO) R"(
            */

            layout (std140) uniform BoneMatricesBlock {
                mat3x4 bone_matrices[256];
            };

            struct InVertex {
                highp vec4 p_and_nxy;
                highp uvec2 nz_and_b;
                highp uvec2 t0_and_t1;
                highp uvec2 bone_indices;
                highp uvec2 bone_weights;
            };

            struct InDelta {
                highp vec2 dpxy;
                highp vec2 dpz_dnxy;
                highp uvec2 dnz_and_db;
            };

            struct OutVertexData0 {
                highp vec4 p_and_t0;
            };

            struct OutVertexData1 {
                highp uvec2 n_and_bx;
                highp uvec2 byz_and_t1;
            };

            layout(std430, binding = 0) readonly buffer Input0 {
                InVertex vertices[];
            } in_data0;

            layout(std430, binding = 1) readonly buffer Input1 {
                InDelta deltas[];
            } in_data1;

            layout(std430, binding = 2) writeonly buffer Output0 {
                OutVertexData0 vertices[];
            } out_data0;

            layout(std430, binding = 3) writeonly buffer Output1 {
                OutVertexData1 vertices[];
            } out_data1;

            layout(location = 0) uniform ivec4 uOffsets;
            layout(location = 1) uniform int uShapeCount;
            layout(location = 2) uniform uint uShapePalette[16];
            
            layout (local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

            void main() {
                if (gl_GlobalInvocationID.x >= uOffsets[1]) return;
                int i = int(uOffsets[0] + gl_GlobalInvocationID.x);

                highp vec3 p = in_data0.vertices[i].p_and_nxy.xyz;

                highp uint _nxy = floatBitsToUint(in_data0.vertices[i].p_and_nxy.w);
                highp vec2 nxy = unpackSnorm2x16(_nxy);

                highp uint _nz_and_bx = in_data0.vertices[i].nz_and_b.x;
                highp vec2 nz_and_bx = unpackSnorm2x16(_nz_and_bx);

                highp uint _byz = in_data0.vertices[i].nz_and_b.y;
                highp vec2 byz = unpackSnorm2x16(_byz);

                highp vec3 n = vec3(nxy, nz_and_bx.x),
                           b = vec3(nz_and_bx.y, byz);

                for (int j = 0; j < uShapeCount; j++) {
                    highp uint shape_data = uShapePalette[j];
                    mediump uint shape_index = bitfieldExtract(shape_data, 0, 16);
                    mediump float shape_weight = unpackUnorm2x16(shape_data).y;

                    int sh_i = int(uOffsets[3] + shape_index * uOffsets[1] + gl_GlobalInvocationID.x);
                    p += shape_weight * vec3(in_data1.deltas[sh_i].dpxy,
                                               in_data1.deltas[sh_i].dpz_dnxy.x);
                    highp uint _dnxy = floatBitsToUint(in_data1.deltas[sh_i].dpz_dnxy.y);
                    mediump vec2 _dnz_and_dbx = unpackSnorm2x16(in_data1.deltas[sh_i].dnz_and_db.x);
                    n += shape_weight * vec3(unpackSnorm2x16(_dnxy), _dnz_and_dbx.x);
                    mediump vec2 _dbyz = unpackSnorm2x16(in_data1.deltas[sh_i].dnz_and_db.y);
                    b += shape_weight * vec3(_dnz_and_dbx.y, _dbyz);
                }

                mediump uvec4 vtx_indices =
                    uvec4(bitfieldExtract(in_data0.vertices[i].bone_indices.x, 0, 16),
                          bitfieldExtract(in_data0.vertices[i].bone_indices.x, 16, 16),
                          bitfieldExtract(in_data0.vertices[i].bone_indices.y, 0, 16),
                          bitfieldExtract(in_data0.vertices[i].bone_indices.y, 16, 16));
                mediump vec4 vtx_weights =
                    vec4(unpackUnorm2x16(in_data0.vertices[i].bone_weights.x),
                         unpackUnorm2x16(in_data0.vertices[i].bone_weights.y));

                highp mat3x4 mat = mat3x4(0.0);

                for (int j = 0; j < 4; j++) {
                    if (vtx_weights[j] > 0.0) {
                        mat = mat + bone_matrices[vtx_indices[j]] * vtx_weights[j];
                    }
                }

                highp mat4x3 tr_mat = transpose(mat);

                highp vec3 tr_p = tr_mat * vec4(p, 1.0);
                mediump vec3 tr_n = tr_mat * vec4(n, 0.0);
                mediump vec3 tr_b = tr_mat * vec4(b, 0.0);

                int out_ndx = int(uOffsets[2] + gl_GlobalInvocationID.x);
                
                out_data0.vertices[out_ndx].p_and_t0.xyz = tr_p;
                // copy texture coordinates unchanged
                out_data0.vertices[out_ndx].p_and_t0.w = uintBitsToFloat(in_data0.vertices[i].t0_and_t1.x);

                out_data1.vertices[out_ndx].n_and_bx.x = packSnorm2x16(tr_n.xy);
                out_data1.vertices[out_ndx].n_and_bx.y = packSnorm2x16(vec2(tr_n.z, tr_b.x));
                out_data1.vertices[out_ndx].byz_and_t1.x = packSnorm2x16(tr_b.yz);
                // copy texture coordinates unchanged
                out_data1.vertices[out_ndx].byz_and_t1.y = in_data0.vertices[i].t0_and_t1.y;
            }
        )";

    Ren::ShaderRef skinning_cs_ref =
        ctx_.LoadShaderGLSL("__skin_cs", skinning_cs, Ren::eShaderType::Comp, &sh_status);
    assert(sh_status == Ren::eShaderLoadStatus::CreatedFromData);

    skinning_prog_ = ctx_.LoadProgram("__skin", skinning_cs_ref, &status);
    assert(status == Ren::eProgLoadStatus::CreatedFromData);

    ////////////////////////////////////////////////////////////////////////////////////////

    GLuint unif_buf;
    glGenBuffers(1, &unif_buf);
    glBindBuffer(GL_UNIFORM_BUFFER, unif_buf);
    glBufferData(GL_UNIFORM_BUFFER, UniformBufferSize, nullptr, GL_DYNAMIC_COPY);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    uniform_buf_ = uint32_t(unif_buf);
}

void ModlApp::DestroyInternal() {
    auto simple_mesh_vao = GLuint(simple_vao_);
    glDeleteVertexArrays(1, &simple_mesh_vao);

    auto skinned_mesh_vao = GLuint(skinned_vao_);
    glDeleteVertexArrays(1, &skinned_mesh_vao);

    auto unif_buf = GLuint(uniform_buf_);
    glDeleteBuffers(1, &unif_buf);
}

#undef _AS_STR
#undef AS_STR