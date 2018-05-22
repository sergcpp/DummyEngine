#include "GSDefTest.h"

#include <ren/Program.h>
#include <sys/Log.h>

namespace GSDefTestInternal {
extern const float CAM_FOV;

extern const float NEAR_CLIP;
extern const float FAR_CLIP;

enum { A_POS,
       A_NORMAL,
       A_UVS,
     };

enum { U_MVP_MAT, U_MV_MAT, U_COL, U_TEX_POS, U_TEX_NORM, U_LIGHT_POS, U_LIGHT_COL };

inline GLuint attr(const ren::Program *p, int i) {
    return (GLuint)p->attribute(i).loc;
}
inline GLuint unif(const ren::Program *p, int i) {
    return (GLuint)p->uniform(i).loc;
}
}

void GSDefTest::DrawMesh() {
    using namespace GSDefTestInternal;
    using namespace math;

    const ren::Mesh *m = test_mesh_.get();
    const ren::Material *mat = m->strip(0).mat.get();
    const ren::Program *p = prim_vars_prog_.get();

    glUseProgram(p->prog_id());

    glBindBuffer(GL_ARRAY_BUFFER, (GLuint)m->attribs_buf_id());
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, (GLuint)m->indices_buf_id());
    
    int stride = sizeof(float) * 8;

    glEnableVertexAttribArray((GLuint)attr(p, A_POS));
    glVertexAttribPointer((GLuint)attr(p, A_POS), 3, GL_FLOAT, GL_FALSE, stride, (void *)0);
    
    glEnableVertexAttribArray((GLuint)attr(p, A_NORMAL));
    glVertexAttribPointer((GLuint)attr(p, A_NORMAL), 3, GL_FLOAT, GL_FALSE, stride, (void *)(3 * sizeof(float)));

    mat4 world_from_object,
         view_from_world = make_mat4(cam_.view_matrix()),
         proj_from_view = make_mat4(cam_.projection_matrix());

    world_from_object = rotate(world_from_object, radians(90.0f), { 0.0f, 0.0f, 1.0f });

    mat4 view_from_object = view_from_world * world_from_object,
         proj_from_object = proj_from_view * view_from_object;
    
    glUniformMatrix4fv(unif(p, U_MVP_MAT), 1, GL_FALSE, value_ptr(proj_from_object));
    glUniformMatrix4fv(unif(p, U_MV_MAT), 1, GL_FALSE, value_ptr(world_from_object));
    
    glUniform3f(unif(p, U_COL), 1.0f, 1.0f, 1.0f);
    
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    const ren::TriStrip *s = &m->strip(0);
    while (s->offset != -1) {
        glDrawElements(GL_TRIANGLE_STRIP, s->num_indices, GL_UNSIGNED_SHORT, (void *)uintptr_t(s->offset));
        ++s;
    }

    ren::CheckError();
}

void GSDefTest::DrawInternal() {
    using namespace GSDefTestInternal;
    using namespace math;
    
#if 1
    glBindFramebuffer(GL_FRAMEBUFFER, framebuf_);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, positions_tex_, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, normals_tex_, 0);

    GLuint attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, attachments);
#endif

    glClearColor(0, 0.2f, 0.2f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    DrawMesh();
    //draw_light();
    
#if 1

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glDepthMask(GL_FALSE);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);

    glCullFace(GL_FRONT);

    const ren::Program *p = screen_draw_prog_.get();

    glUseProgram(p->prog_id());

    glBindBuffer(GL_ARRAY_BUFFER, unit_sphere_buf_);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, positions_tex_);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, normals_tex_);

    glUniform1i(unif(p, U_TEX_POS), 0);
    glUniform1i(unif(p, U_TEX_NORM), 1);

    glEnableVertexAttribArray((GLuint)attr(p, A_POS));
    glVertexAttribPointer((GLuint)attr(p, A_POS), 3, GL_FLOAT, GL_FALSE, 0, (void *)0);

    mat4 view_from_world = make_mat4(cam_.view_matrix()),
         proj_from_view = make_mat4(cam_.projection_matrix());

    for (const auto &l : lights_) {
        mat4 world_from_object;

        world_from_object = translate(world_from_object, l.pos);
        //world_from_object = rotate(world_from_object, radians(90.0f), { 0.0f, 0.0f, 1.0f });

        mat4 view_from_object = view_from_world * world_from_object,
             proj_from_object = proj_from_view * view_from_object;

        glUniform3f(unif(p, U_LIGHT_POS), l.pos.x, l.pos.y, l.pos.z);
        glUniform3f(unif(p, U_LIGHT_COL), l.col.x, l.col.y, l.col.z);

        glUniformMatrix4fv(unif(p, U_MVP_MAT), 1, GL_FALSE, value_ptr(proj_from_object));
        glUniformMatrix4fv(unif(p, U_MV_MAT), 1, GL_FALSE, value_ptr(world_from_object));

        glDrawArrays(GL_TRIANGLES, 0, unit_sphere_tris_count_);
    }

    glDepthMask(GL_TRUE);

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    ren::CheckError();
#endif
}

void GSDefTest::InitInternal(int w, int h) {
    glGenFramebuffers(1, &framebuf_);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuf_);

    glGenTextures(1, &positions_tex_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, positions_tex_);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, w, h, 0, GL_RGB, GL_FLOAT, NULL);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, positions_tex_, 0);

    glGenTextures(1, &normals_tex_);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, normals_tex_);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, w, h, 0, GL_RGB, GL_FLOAT, NULL);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, normals_tex_, 0);

    GLuint attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, attachments);

    ren::CheckError("[Renderer]: create framebuffer 2");

    glGenRenderbuffers(1, &depth_rb_);
    glBindRenderbuffer(GL_RENDERBUFFER, depth_rb_);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, w, h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_rb_);

    auto s = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (s != GL_FRAMEBUFFER_COMPLETE) {
        LOGI("Frambuffer error %i", int(s));
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        throw std::runtime_error("Framebuffer error!");
    }

    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    ren::CheckError("[Renderer]: create framebuffer 3");
    LOGI("Framebuffer created (%ix%i)", w, h);

    auto load_program = [&](const char *name, const char *vs, const char *fs) {
        std::string vs_name = "assets/shaders/";
        vs_name += vs;
        std::string fs_name = "assets/shaders/";
        fs_name += fs;

        sys::AssetFile vs_file(vs_name, sys::AssetFile::IN);
        size_t vs_file_size = vs_file.size();

        std::string vs_file_data;
        vs_file_data.resize(vs_file_size);
        vs_file.Read(&vs_file_data[0], vs_file_size);

        sys::AssetFile fs_file(fs_name, sys::AssetFile::IN);
        size_t fs_file_size = fs_file.size();

        std::string fs_file_data;
        fs_file_data.resize(fs_file_size);
        fs_file.Read(&fs_file_data[0], fs_file_size);

        return ctx_->LoadProgramGLSL(name, &vs_file_data[0], &fs_file_data[0], nullptr);
    };

    prim_vars_prog_ = load_program("prim_vars", "prim_vars.vs", "prim_vars.fs");
    glBindFragDataLocation(prim_vars_prog_->prog_id(), 0, "positions_out");
    glBindFragDataLocation(prim_vars_prog_->prog_id(), 1, "normals_out");

    screen_draw_prog_ = load_program("screen_draw", "screen_draw.vs", "screen_draw.fs");

    {
        using namespace math;

        std::vector<float> vertices;

        const int stacks = 16;
        const int slices = 16;

        for (int t = 0; t < stacks; t++) {
            float theta1 = ((float)(t) / stacks)*pi<float>();
            float theta2 = ((float)(t + 1) / stacks)*pi<float>();

            for (int p = 0; p < slices; p++) {
                float phi1 = ((float)(p) / slices) * 2 * pi<float>(); // azimuth goes around 0 .. 2*PI
                float phi2 = ((float)(p + 1) / slices) * 2 * pi<float>();

                //phi2   phi1
                // |      |
                // 2------1 -- theta1
                // |\ _   |
                // |    \ |
                // 3------4 -- theta2
                //

                float vertex1[3] = { 2.0f * std::sin(theta1) * std::cos(phi1),
                                     2.0f * std::sin(theta1) * std::sin(phi1),
                                     2.0f * std::cos(theta1) };

                float vertex2[3] = { 2.0f * std::sin(theta1) * std::cos(phi2),
                                     2.0f * std::sin(theta1) * std::sin(phi2),
                                     2.0f * std::cos(theta1) };

                float vertex3[3] = { 2.0f * std::sin(theta2) * std::cos(phi2),
                                     2.0f * std::sin(theta2) * std::sin(phi2),
                                     2.0f * std::cos(theta2) };

                float vertex4[3] = { 2.0f * std::sin(theta2) * std::cos(phi1),
                                     2.0f * std::sin(theta2) * std::sin(phi1),
                                     2.0f * std::cos(theta2) };

                if (t == 0) {
                    vertices.insert(vertices.end(), std::begin(vertex1), std::end(vertex1));
                    vertices.insert(vertices.end(), std::begin(vertex4), std::end(vertex4));
                    vertices.insert(vertices.end(), std::begin(vertex3), std::end(vertex3));
                } else if (t + 1 == stacks) {
                    vertices.insert(vertices.end(), std::begin(vertex3), std::end(vertex3));
                    vertices.insert(vertices.end(), std::begin(vertex2), std::end(vertex2));
                    vertices.insert(vertices.end(), std::begin(vertex1), std::end(vertex1));
                } else {
                    vertices.insert(vertices.end(), std::begin(vertex1), std::end(vertex1));
                    vertices.insert(vertices.end(), std::begin(vertex4), std::end(vertex4));
                    vertices.insert(vertices.end(), std::begin(vertex2), std::end(vertex2));

                    vertices.insert(vertices.end(), std::begin(vertex2), std::end(vertex2));
                    vertices.insert(vertices.end(), std::begin(vertex4), std::end(vertex4));
                    vertices.insert(vertices.end(), std::begin(vertex3), std::end(vertex3));
                }
            }
        }

        GLuint gl_buf;
        glGenBuffers(1, &gl_buf);
        glBindBuffer(GL_ARRAY_BUFFER, gl_buf);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

        unit_sphere_buf_ = gl_buf;
        unit_sphere_tris_count_ = vertices.size() / 3;
    }
}