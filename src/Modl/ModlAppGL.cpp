#include "ModlApp.h"

#include <Ren/GL.h>

#include <Ren/MMat.h>

namespace {
const int A_POS     = 0;
const int A_NORMAL  = 1;
const int A_TANGENT = 2;
const int A_UVS1    = 3;
const int A_UVS2    = 4;
const int A_INDICES = 5;
const int A_WEIGHTS = 6;

const int U_MVP_MATR    = 0;
const int U_M_MATR      = 1;
const int U_MODE        = 2;
const int U_M_PALETTE   = 3;

const int DIFFUSEMAP_SLOT   = 0;
const int NORMALMAP_SLOT    = 1;

inline void BindTexture(int slot, uint32_t tex) {
    glActiveTexture((GLenum)(GL_TEXTURE0 + slot));
    glBindTexture(GL_TEXTURE_2D, (GLuint)tex);
}
}

void ModlApp::DrawMeshSimple(Ren::MeshRef &ref) {
    using namespace Ren;

    auto m		= ref.get();
    auto mat	= m->group(0).mat.get();
    auto p      = mat->program(0);

    glBindBuffer(GL_ARRAY_BUFFER, m->attribs_buf_id());
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m->indices_buf_id());

    p = diag_prog_;
    glUniform1f(U_MODE, (float)view_mode_);

    CheckInitVAOs();

    glBindVertexArray((GLuint)simple_vao_);

    glUseProgram(p->prog_id());

    Mat4f world_from_object = Mat4f{ 1.0f };

    world_from_object = Rotate(world_from_object, angle_x_, { 1, 0, 0 });
    world_from_object = Rotate(world_from_object, angle_y_, { 0, 1, 0 });

    Mat4f view_from_world = cam_.view_matrix(),
          proj_from_view = cam_.proj_matrix();

    Mat4f view_from_object = view_from_world * world_from_object,
          proj_from_object = proj_from_view * view_from_object;

    glUniformMatrix4fv(U_MVP_MATR, 1, GL_FALSE, ValuePtr(proj_from_object));
    glUniformMatrix4fv(U_M_MATR, 1, GL_FALSE, ValuePtr(world_from_object));

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

        glDrawElements(GL_TRIANGLES, s->num_indices, GL_UNSIGNED_INT, (void *)uintptr_t(s->offset));
        ++s;
    }

    Ren::CheckError();
}

void ModlApp::DrawMeshSkeletal(Ren::MeshRef &ref, float dt_s) {
    using namespace Ren;

    auto m	    = ref.get();
    auto mat	= m->group(0).mat.get();

    Ren::Skeleton *skel = m->skel();
    if (!skel->anims.empty()) {
        skel->UpdateAnim(0, dt_s, nullptr);
        skel->ApplyAnim(0);
        skel->UpdateBones();
    }

    CheckInitVAOs();

#if 0
    glBindVertexArray((GLuint)skinned_vao_);

    const Ren::Program *p = diag_skinned_prog_.get();
    glUseProgram(p->prog_id());

    glUniform1f(U_MODE, (float)view_mode_);

    Mat4f world_from_object = Mat4f{ 1.0f };

    //world_from_object = Rotate(world_from_object, angle_x_, { 1, 0, 0 });
    world_from_object = Rotate(world_from_object, angle_y_, { 0, 1, 0 });

    Mat4f view_from_world = cam_.view_matrix(),
          proj_from_view = cam_.proj_matrix();

    Mat4f view_from_object = view_from_world * world_from_object,
          proj_from_object = proj_from_view * view_from_object;

    glUniformMatrix4fv(U_MVP_MATR, 1, GL_FALSE, ValuePtr(proj_from_object));
    glUniformMatrix4fv(U_M_MATR, 1, GL_FALSE, ValuePtr(world_from_object));

    size_t num_bones = skel->matr_palette.size();
    glUniformMatrix4fv(U_M_PALETTE, (GLsizei)num_bones, GL_FALSE, ValuePtr(skel->matr_palette[0]));

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

        glDrawElementsBaseVertex(GL_TRIANGLES, s->num_indices, GL_UNSIGNED_INT, (void *)uintptr_t(s->offset), (GLint)m->sk_indices_buf().offset - m->indices_buf().offset);
        ++s;
    }

    Ren::CheckError();
#else

    {   // transform vertices
        const Ren::Program *p = skinning_prog_.get();

        glUseProgram(p->prog_id());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, (GLuint)last_skin_vertex_buffer_);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, (GLuint)last_vertex_buffer_);

        glUniform2i(0, m->attribs_buf().offset / 48, 0);

        size_t num_bones = skel->matr_palette.size();
        glUniformMatrix4fv(2, (GLsizei)num_bones, GL_FALSE, ValuePtr(skel->matr_palette[0]));

        glDispatchCompute((GLuint)m->attribs_buf().size, 1, 1);
    }

    glBindVertexArray((GLuint)simple_vao_);

    const Ren::Program *p = diag_prog_.get();
    glUseProgram(p->prog_id());

    glUniform1f(U_MODE, (float)view_mode_);

    Mat4f world_from_object = Mat4f{ 1.0f };

    //world_from_object = Rotate(world_from_object, angle_x_, { 1, 0, 0 });
    world_from_object = Rotate(world_from_object, angle_y_, { 0, 1, 0 });

    Mat4f view_from_world = cam_.view_matrix(),
          proj_from_view = cam_.proj_matrix();

    Mat4f view_from_object = view_from_world * world_from_object,
        proj_from_object = proj_from_view * view_from_object;

    glUniformMatrix4fv(U_MVP_MATR, 1, GL_FALSE, ValuePtr(proj_from_object));
    glUniformMatrix4fv(U_M_MATR, 1, GL_FALSE, ValuePtr(world_from_object));

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

        glDrawElementsBaseVertex(GL_TRIANGLES, s->num_indices, GL_UNSIGNED_INT, (void *)uintptr_t(s->offset), (GLint)m->sk_indices_buf().offset - m->indices_buf().offset);
        ++s;
    }

    Ren::CheckError();
#endif
}

void ModlApp::ClearColorAndDepth(float r, float g, float b, float a) {
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void ModlApp::CheckInitVAOs() {
    auto vtx_buf = ctx_.default_vertex_buf();
    auto skin_vtx_buf = ctx_.default_skin_vertex_buf();
    auto ndx_buf = ctx_.default_indices_buf();
    auto skin_ndx_buf = ctx_.default_skin_indices_buf();

    GLuint gl_vertex_buf = (GLuint)vtx_buf->buf_id(),
           gl_skin_vertex_buf = (GLuint)skin_vtx_buf->buf_id(),
           gl_indices_buf = (GLuint)ndx_buf->buf_id(),
           gl_skin_indices_buf = (GLuint)skin_ndx_buf->buf_id();

    if (gl_vertex_buf != last_vertex_buffer_ || gl_skin_vertex_buf != last_skin_vertex_buffer_ ||
        gl_indices_buf != last_index_buffer_ || gl_skin_indices_buf != last_skin_index_buffer_) {

        if (last_vertex_buffer_) {
            GLuint simple_mesh_vao = (GLuint)simple_vao_;
            glDeleteVertexArrays(1, &simple_mesh_vao);

            GLuint skinned_mesh_vao = (GLuint)skinned_vao_;
            glDeleteVertexArrays(1, &skinned_mesh_vao);
        }

        GLuint simple_mesh_vao;

        glGenVertexArrays(1, &simple_mesh_vao);
        glBindVertexArray(simple_mesh_vao);

        glBindBuffer(GL_ARRAY_BUFFER, gl_vertex_buf);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl_indices_buf);

        int stride = 32;
        glEnableVertexAttribArray(A_POS);
        glVertexAttribPointer(A_POS, 3, GL_FLOAT, GL_FALSE, stride, (void *)0);

        glEnableVertexAttribArray(A_NORMAL);
        glVertexAttribPointer(A_NORMAL, 4, GL_SHORT, GL_TRUE, stride, (void *)(3 * sizeof(float)));

        glEnableVertexAttribArray(A_TANGENT);
        glVertexAttribPointer(A_TANGENT, 2, GL_SHORT, GL_TRUE, stride, (void *)(3 * sizeof(float) + 4 * sizeof(uint16_t)));

        glEnableVertexAttribArray(A_UVS1);
        glVertexAttribPointer(A_UVS1, 2, GL_HALF_FLOAT, GL_FALSE, stride, (void *)(3 * sizeof(float) + 6 * sizeof(uint16_t)));

        glEnableVertexAttribArray(A_UVS2);
        glVertexAttribPointer(A_UVS2, 2, GL_HALF_FLOAT, GL_FALSE, stride, (void *)(3 * sizeof(float) + 8 * sizeof(uint16_t)));

        glBindVertexArray(0);

        simple_vao_ = (uint32_t)simple_mesh_vao;

        GLuint skinned_mesh_vao;
        glGenVertexArrays(1, &skinned_mesh_vao);
        glBindVertexArray(skinned_mesh_vao);

        glBindBuffer(GL_ARRAY_BUFFER, gl_skin_vertex_buf);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl_skin_indices_buf);

        stride = 48;
        glEnableVertexAttribArray(A_POS);
        glVertexAttribPointer(A_POS, 3, GL_FLOAT, GL_FALSE, stride, (void *)0);

        glEnableVertexAttribArray(A_NORMAL);
        glVertexAttribPointer(A_NORMAL, 4, GL_SHORT, GL_TRUE, stride, (void *)(3 * sizeof(float)));

        glEnableVertexAttribArray(A_TANGENT);
        glVertexAttribPointer(A_TANGENT, 2, GL_SHORT, GL_TRUE, stride, (void *)(3 * sizeof(float) + 4 * sizeof(int16_t)));

        glEnableVertexAttribArray(A_UVS1);
        glVertexAttribPointer(A_UVS1, 2, GL_HALF_FLOAT, GL_FALSE, stride, (void *)(3 * sizeof(float) + 6 * sizeof(int16_t)));

        glEnableVertexAttribArray(A_UVS2);
        glVertexAttribPointer(A_UVS2, 2, GL_HALF_FLOAT, GL_FALSE, stride, (void *)(3 * sizeof(float) + 6 * sizeof(int16_t) + 2 * sizeof(uint16_t)));

        glEnableVertexAttribArray(A_INDICES);
        glVertexAttribPointer(A_INDICES, 4, GL_UNSIGNED_SHORT, GL_FALSE, stride, (void *)(3 * sizeof(float) + 6 * sizeof(int16_t) + 4 * sizeof(uint16_t)));

        glEnableVertexAttribArray(A_WEIGHTS);
        glVertexAttribPointer(A_WEIGHTS, 4, GL_UNSIGNED_SHORT, GL_TRUE, stride, (void *)(3 * sizeof(float) + 6 * sizeof(int16_t) + 8 * sizeof(uint16_t)));

        glBindVertexArray(0);

        skinned_vao_ = (uint32_t)skinned_mesh_vao;

        last_vertex_buffer_ = (uint32_t)gl_vertex_buf;
        last_skin_vertex_buffer_ = (uint32_t)gl_skin_vertex_buf;
        last_index_buffer_ = (uint32_t)gl_indices_buf;
        last_skin_index_buffer_ = (uint32_t)gl_skin_indices_buf;
    }
}

void ModlApp::InitInternal() {
    static const char diag_vs[] = R"(
            #version 430

            layout(location = 0) in vec3 aVertexPosition;
            layout(location = 1) in vec4 aVertexNormal;
            layout(location = 2) in vec2 aVertexTangent;
            layout(location = 3) in vec2 aVertexUVs1;
            layout(location = 4) in vec2 aVertexUVs2;

            layout(location = 0) uniform mat4 uMVPMatrix;
            layout(location = 1) uniform mat4 uMMatrix;

            out mat3 aVertexTBN_;
            out vec2 aVertexUVs1_;
            out vec2 aVertexUVs2_;

            void main(void) {
                vec3 vertex_normal_ws = normalize((uMMatrix * vec4(aVertexNormal.xyz, 0.0)).xyz);
                vec3 vertex_tangent_ws = normalize((uMMatrix * vec4(aVertexNormal.w, aVertexTangent, 0.0)).xyz);

                aVertexTBN_ = mat3(vertex_tangent_ws, cross(vertex_normal_ws, vertex_tangent_ws), vertex_normal_ws);
                aVertexUVs1_ = aVertexUVs1;
                aVertexUVs2_ = aVertexUVs2;

                gl_Position = uMVPMatrix * vec4(aVertexPosition, 1.0);
            }
        )";

    static const char diag_skinned_vs[] = R"(
            #version 430

            layout(location = 0) in vec3 aVertexPosition;
            layout(location = 1) in mediump vec4 aVertexNormal;
            layout(location = 2) in mediump vec2 aVertexTangent;
            layout(location = 3) in mediump vec2 aVertexUVs1;
            layout(location = 4) in mediump vec2 aVertexUVs2;
            layout(location = 5) in mediump vec4 aVertexIndices;
            layout(location = 6) in mediump vec4 aVertexWeights;

            layout(location = 0) uniform mat4 uMVPMatrix;
            layout(location = 1) uniform mat4 uMMatrix;
            layout(location = 3) uniform mat4 uMPalette[64];

            out mat3 aVertexTBN_;
            out vec2 aVertexUVs1_;
            out vec2 aVertexUVs2_;

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

                aVertexTBN_ = mat3(vertex_tangent_ws, cross(vertex_normal_ws, vertex_tangent_ws), vertex_normal_ws);
                aVertexUVs1_ = aVertexUVs1;
                aVertexUVs2_ = aVertexUVs2;

                gl_Position = (uMVPMatrix * mat) * vec4(aVertexPosition, 1.0);
            }
        )";

    static const char diag_fs[] = R"(
            #version 430

            #ifdef GL_ES
                precision mediump float;
            #endif

            layout(binding = 0) uniform sampler2D diffuse_texture;
            layout(binding = 1) uniform sampler2D normals_texture;

            layout(location = 2) uniform float mode;
            
            in mat3 aVertexTBN_;
            in vec2 aVertexUVs1_;
            in vec2 aVertexUVs2_;

            out vec4 outColor;

            void main(void) {
                if (mode < 0.5) {
                    outColor = texture(diffuse_texture, aVertexUVs1_);
                } else if (mode < 1.5) {
                    vec3 normal = aVertexTBN_[2] * 0.5 + vec3(0.5);
                    outColor = vec4(normal, 1.0);
                } else if (mode < 2.5) {
                    vec3 tangent = aVertexTBN_[0] * 0.5 + vec3(0.5);
                    outColor = vec4(tangent, 1.0);
                } else if (mode < 3.5) {
                    vec3 tex_normal = texture(normals_texture, aVertexUVs1_).xyz * 2.0 - 1.0;
                    outColor = vec4((aVertexTBN_ * tex_normal) * 0.5 + vec3(0.5), 1.0);
                } else if (mode < 4.5) {
                    outColor = texture(diffuse_texture, aVertexUVs1_);
                } else if (mode < 5.5) {
                    outColor = texture(diffuse_texture, aVertexUVs2_);
                }
            }
        )";

    Ren::eProgLoadStatus status;
    diag_prog_ = ctx_.LoadProgramGLSL("__diag", diag_vs, diag_fs, &status);
    assert(status == Ren::ProgCreatedFromData);
    diag_skinned_prog_ = ctx_.LoadProgramGLSL("__diag_skinned", diag_skinned_vs, diag_fs, &status);
    assert(status == Ren::ProgCreatedFromData);

    static const char skinning_cs[] = R"(
            #version 430

            struct InVertex {
                highp vec4 p_and_nxy;
                highp uvec2 nz_and_b;
                highp uvec2 t0_and_t1;
                highp uvec2 bone_indices;
                highp uvec2 bone_weights;
            };

            struct OutVertex {
                highp vec4 p_and_nxy;
                highp uvec2 nz_and_b;
                highp uvec2 t0_and_t1;
            };

            layout(std430, binding = 0) readonly buffer Input0 {
                InVertex vertices[];
            } in_data;

            layout(std430, binding = 1) writeonly buffer Output {
                OutVertex vertices[];
            } out_data;

            layout(location = 0) uniform ivec2 uOffsets;
            layout(location = 2) uniform mat4 uMPalette[64];
            
            layout (local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

            void main() {
                int i = int(uOffsets[0] + gl_GlobalInvocationID.x);

                highp vec3 p = in_data.vertices[i].p_and_nxy.xyz;

                highp uint _nxy = floatBitsToUint(in_data.vertices[i].p_and_nxy.w);
                highp vec2 nxy = unpackSnorm2x16(_nxy);

                highp uint _nz_and_bx = in_data.vertices[i].nz_and_b.x;
                highp vec2 nz_and_bx = unpackSnorm2x16(_nz_and_bx);

                highp uint _byz = in_data.vertices[i].nz_and_b.y;
                highp vec2 byz = unpackSnorm2x16(_byz);

                highp vec3 n = vec3(nxy, nz_and_bx.x),
                           b = vec3(nz_and_bx.y, byz);

                mediump uvec4 vtx_indices = uvec4(bitfieldExtract(in_data.vertices[i].bone_indices.x, 0, 16),
                                                  bitfieldExtract(in_data.vertices[i].bone_indices.x, 16, 16),
                                                  bitfieldExtract(in_data.vertices[i].bone_indices.y, 0, 16),
                                                  bitfieldExtract(in_data.vertices[i].bone_indices.y, 16, 16));
                mediump vec4 vtx_weights = vec4(unpackUnorm2x16(in_data.vertices[i].bone_weights.x),
                                                unpackUnorm2x16(in_data.vertices[i].bone_weights.y));

                highp mat4 mat = mat4(0.0);

                for (int j = 0; j < 4; j++) {
                    if (vtx_weights[j] > 0.0) {
                        mat = mat + uMPalette[vtx_indices[j]] * vtx_weights[j];
                    }
                }

                highp vec4 _p = mat * vec4(p, 1.0);

                highp vec3 tr_p = _p.xyz / _p.w;
                mediump vec3 tr_n = normalize((mat * vec4(n, 0.0)).xyz);
                mediump vec3 tr_b = normalize((mat * vec4(b, 0.0)).xyz);

                int k = int(uOffsets[1] + gl_GlobalInvocationID.x);
                out_data.vertices[k].p_and_nxy.xyz = tr_p;
                out_data.vertices[k].p_and_nxy.w = uintBitsToFloat(packSnorm2x16(tr_n.xy));
                out_data.vertices[k].nz_and_b.x = packSnorm2x16(vec2(tr_n.z, tr_b.x));
                out_data.vertices[k].nz_and_b.y = packSnorm2x16(tr_b.yz);
            }
        )";

    skinning_prog_ = ctx_.LoadProgramGLSL("__skin", skinning_cs, &status);
    assert(status == Ren::ProgCreatedFromData);
}

void ModlApp::DestroyInternal() {
    GLuint simple_mesh_vao = (GLuint)simple_vao_;
    glDeleteVertexArrays(1, &simple_mesh_vao);

    GLuint skinned_mesh_vao = (GLuint)skinned_vao_;
    glDeleteVertexArrays(1, &skinned_mesh_vao);
}