#include "GSOccTest.h"

#include <ren/Program.h>

namespace GSOccTestInternal {
extern const float CAM_FOV;

extern const float NEAR_CLIP;
extern const float FAR_CLIP;

const char *vs_source = ""
                        "/*\n"
                        "ATTRIBUTES\n"
                        "    aVertexPosition : 0\n"
                        "    aVertexNormal : 1\n"
                        "UNIFORMS\n"
                        "    uMVPMatrix : 0\n"
                        "*/\n"
                        "\n"
                        "attribute vec3 aVertexPosition;\n"
                        "attribute vec3 aVertexNormal;\n"
                        "uniform mat4 uMVPMatrix;\n"
                        "\n"
                        "varying vec3 aVertexNormal_;\n"
                        "\n"
                        "void main(void) {\n"
                        "    aVertexNormal_ = aVertexNormal;\n"
                        "    gl_Position = uMVPMatrix * vec4(aVertexPosition, 1.0);\n"
                        "}\n";

const char *fs_source = ""
                        "#ifdef GL_ES\n"
                        "    precision mediump float;\n"
                        "#else\n"
                        "    #define lowp\n"
                        "    #define mediump\n"
                        "    #define highp\n"
                        "#endif\n"
                        "\n"
                        "/*\n"
                        "UNIFORMS\n"
                        "    col : 1\n"
                        "*/\n"
                        "\n"
                        "uniform vec3 col;\n"
                        "varying vec3 aVertexNormal_;\n"
                        "\n"
                        "void main(void) {\n"
                        "    gl_FragColor = vec4(col, 1.0) * 0.001 + vec4(aVertexNormal_*0.5 + vec3(0.5, 0.5, 0.5), 1.0);\n"
                        "}\n";

enum { A_POS,
       A_NORMAL
     };

enum { U_MVP_MAT, U_COL };

inline GLuint attr(const ren::Program *p, int i) {
    return (GLuint)p->attribute(i).loc;
}
inline GLuint unif(const ren::Program *p, int i) {
    return (GLuint)p->uniform(i).loc;
}
}

void GSOccTest::InitShaders() {
    using namespace GSOccTestInternal;

    ren::eProgLoadStatus status;
    main_prog_ = ctx_->LoadProgramGLSL("main", vs_source, fs_source, &status);
    assert(status == ren::ProgCreatedFromData);
}

void GSOccTest::DrawBoxes(SWcull_surf *surfs, int count) {
    using namespace GSOccTestInternal;
    using namespace math;

    const ren::Program *p = main_prog_.get();

    glUseProgram(p->prog_id());

    glEnableVertexAttribArray(attr(p, A_POS));
    glEnableVertexAttribArray(attr(p, A_NORMAL));

    mat4 world_from_object,
         view_from_world = make_mat4(cam_.view_matrix()),
         proj_from_view = make_mat4(cam_.projection_matrix());

    mat4 view_from_object = view_from_world * world_from_object,
         proj_from_object = proj_from_view * view_from_object;

    glUniformMatrix4fv(unif(p, U_MVP_MAT), 1, GL_FALSE, value_ptr(proj_from_object));

    glUniform3f(unif(p, U_COL), 1.0f, 1.0f, 1.0f);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    if (wireframe_) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }

    for (int i = 0; i < count; i++) {
        if (!surfs[i].visible && cull_) continue;

        glVertexAttribPointer(attr(p, A_POS), 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, (const void *)surfs[i].attribs);
        glVertexAttribPointer(attr(p, A_NORMAL), 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, (const void *)((float *)surfs[i].attribs + 3));

        glDrawElements(GL_TRIANGLES, surfs[i].count, GL_UNSIGNED_BYTE, (const void *)surfs[i].indices);
    }

    if (wireframe_) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    ren::CheckError();
}

void GSOccTest::DrawCam() {
    using namespace GSOccTestInternal;
    using namespace math;

    const ren::Program *p = main_prog_.get();

    glUseProgram(p->prog_id());

    glEnableVertexAttribArray(attr(p, A_POS));
    glEnableVertexAttribArray(attr(p, A_NORMAL));

    mat4 world_from_object,
         view_from_world = make_mat4(cam_.view_matrix()),
         proj_from_view = make_mat4(cam_.projection_matrix());

    mat4 view_from_object = view_from_world * world_from_object,
         proj_from_object = proj_from_view * view_from_object;

    glUniformMatrix4fv(unif(p, U_MVP_MAT), 1, GL_FALSE, value_ptr(proj_from_object));

    glUniform3f(unif(p, U_COL), 1.0f, 1.0f, 1.0f);

    glDisable(GL_DEPTH_TEST);

    glLineWidth(5.0f);

    const float *pos = cam_.world_position();

    vec3 up = { 0, 1, 0 };
    vec3 side = normalize(cross(view_dir_, up));
    up = cross(side, view_dir_);

    vec3 v1, v2, v3, v4;

    {
        mat4 rot;
        rot = rotate(rot, radians(CAM_FOV / 1), up);
        rot = rotate(rot, radians(CAM_FOV / 4), side);
        v1 = view_dir_ * mat3(rot);
    }

    {
        mat4 rot;
        rot = rotate(rot, radians(-CAM_FOV / 1), up);
        rot = rotate(rot, radians(CAM_FOV / 4), side);
        v2 = view_dir_ * mat3(rot);
    }

    {
        mat4 rot;
        rot = rotate(rot, radians(CAM_FOV / 1), up);
        rot = rotate(rot, radians(-CAM_FOV / 4), side);
        v3 = view_dir_ * mat3(rot);
    }

    {
        mat4 rot;
        rot = rotate(rot, radians(-CAM_FOV / 1), up);
        rot = rotate(rot, radians(-CAM_FOV / 4), side);
        v4 = view_dir_ * mat3(rot);
    }

    const float attribs[] = { view_origin_.x, view_origin_.y, view_origin_.z,
                              view_origin_.x + 500 * v1.x, view_origin_.y + 500 * v1.y, view_origin_.z + 500 * v1.z,
                              view_origin_.x, view_origin_.y, view_origin_.z,
                              view_origin_.x + 500 * v2.x, view_origin_.y + 500 * v2.y, view_origin_.z + 500 * v2.z,

                              view_origin_.x, view_origin_.y, view_origin_.z,
                              view_origin_.x + 500 * v3.x, view_origin_.y + 500 * v3.y, view_origin_.z + 500 * v3.z,
                              view_origin_.x, view_origin_.y, view_origin_.z,
                              view_origin_.x + 500 * v4.x, view_origin_.y + 500 * v4.y, view_origin_.z + 500 * v4.z
                            };

    const float normals[] = { 0, 1, 1,
                              0, 1, 1,
                              0, 1, 1,
                              0, 1, 1,
                              0, 1, 1,
                              0, 1, 1,
                              0, 1, 1,
                              0, 1, 1
                            };

    glVertexAttribPointer(attr(p, A_POS), 3, GL_FLOAT, GL_FALSE, 0, (const void *)&attribs[0]);
    glVertexAttribPointer(attr(p, A_NORMAL), 3, GL_FLOAT, GL_FALSE, 0, (const void *)&normals[0]);

    glDrawArrays(GL_LINES, 0, 8);

    glLineWidth(1.0f);

    ren::CheckError();
}

void GSOccTest::BlitDepthBuf() {
    using namespace GSOccTestInternal;

    int w = cull_ctx_.zbuf.w, h = cull_ctx_.zbuf.h;
    std::vector<uint8_t> pixels(w * h * 4);
    for (int x = 0; x < w; x++) {
        for (int y = 0; y < h; y++) {
            float z = cull_ctx_.zbuf.depth[(h - y - 1) * w + x];
            z = (2.0f * NEAR_CLIP) / (FAR_CLIP + NEAR_CLIP - z * (FAR_CLIP - NEAR_CLIP));
            pixels[4 * (y * w + x) + 0] = (uint8_t)(z * 255);
            pixels[4 * (y * w + x) + 1] = (uint8_t)(z * 255);
            pixels[4 * (y * w + x) + 2] = (uint8_t)(z * 255);
            pixels[4 * (y * w + x) + 3] = 255;
        }
    }

    glUseProgram(0);

    glDisable(GL_DEPTH_TEST);
    glDrawPixels(w, h, GL_RGBA, GL_UNSIGNED_BYTE, &pixels[0]);
}