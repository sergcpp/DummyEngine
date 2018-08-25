#include "GSOccTest.h"

#include <Eng/GameBase.h>
#include <Ren/Context.h>
#include <Ren/GL.h>
#include <Ren/Program.h>

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
                        "    gl_FragColor = vec4(col, 1.0) * 0.001 + vec4(aVertexNormal_ * 0.5 + vec3(0.5, 0.5, 0.5), 1.0);\n"
                        "}\n";

enum { A_POS,
       A_NORMAL
     };

enum { U_MVP_MAT, U_COL };

inline GLuint attr(const Ren::Program *p, int i) {
    return (GLuint)p->attribute(i).loc;
}
inline GLuint unif(const Ren::Program *p, int i) {
    return (GLuint)p->uniform(i).loc;
}

template <typename T>
T radians(T deg) {
    const T Pi = T(3.1415926535897932384626433832795);
    return deg * Pi / T(180);
}

}

void GSOccTest::InitShaders() {
    using namespace GSOccTestInternal;

    Ren::eProgLoadStatus status;
    main_prog_ = ctx_->LoadProgramGLSL("main", vs_source, fs_source, &status);
    assert(status == Ren::ProgCreatedFromData);
}

void GSOccTest::DrawBoxes(SWcull_surf *surfs, int count) {
    using namespace GSOccTestInternal;

    const Ren::Program *p = main_prog_.get();

    glUseProgram(p->prog_id());

    glEnableVertexAttribArray(attr(p, A_POS));
    glEnableVertexAttribArray(attr(p, A_NORMAL));

    Ren::Mat4f world_from_object,
               view_from_world = cam_.view_matrix(),
               proj_from_view = cam_.projection_matrix();

    Ren::Mat4f view_from_object = view_from_world * world_from_object,
               proj_from_object = proj_from_view * view_from_object;

    glUniformMatrix4fv(unif(p, U_MVP_MAT), 1, GL_FALSE, Ren::ValuePtr(proj_from_object));

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

    Ren::CheckError();
}

void GSOccTest::DrawCam() {
    using namespace GSOccTestInternal;

    const Ren::Program *p = main_prog_.get();

    glUseProgram(p->prog_id());

    glEnableVertexAttribArray(attr(p, A_POS));
    glEnableVertexAttribArray(attr(p, A_NORMAL));

    Ren::Mat4f world_from_object,
               view_from_world = cam_.view_matrix(),
               proj_from_view = cam_.projection_matrix();

    Ren::Mat4f view_from_object = view_from_world * world_from_object,
               proj_from_object = proj_from_view * view_from_object;

    glUniformMatrix4fv(unif(p, U_MVP_MAT), 1, GL_FALSE, Ren::ValuePtr(proj_from_object));

    glUniform3f(unif(p, U_COL), 1.0f, 1.0f, 1.0f);

    glDisable(GL_DEPTH_TEST);

    glLineWidth(5.0f);

    const Ren::Vec3f pos = cam_.world_position();

    Ren::Vec3f up = { 0, 1, 0 };
    Ren::Vec3f side = Normalize(Cross(view_dir_, up));
    up = Cross(side, view_dir_);

    Ren::Vec3f v1, v2, v3, v4;

    {
        Ren::Mat4f rot;
        rot = Rotate(rot, radians(CAM_FOV / 1), up);
        rot = Rotate(rot, radians(CAM_FOV / 4), side);
        v1 = view_dir_ * Ren::Mat3f(rot);
    }

    {
        Ren::Mat4f rot;
        rot = Rotate(rot, radians(-CAM_FOV / 1), up);
        rot = Rotate(rot, radians(CAM_FOV / 4), side);
        v2 = view_dir_ * Ren::Mat3f(rot);
    }

    {
        Ren::Mat4f rot;
        rot = Rotate(rot, radians(CAM_FOV / 1), up);
        rot = Rotate(rot, radians(-CAM_FOV / 4), side);
        v3 = view_dir_ * Ren::Mat3f(rot);
    }

    {
        Ren::Mat4f rot;
        rot = Rotate(rot, radians(-CAM_FOV / 1), up);
        rot = Rotate(rot, radians(-CAM_FOV / 4), side);
        v4 = view_dir_ * Ren::Mat3f(rot);
    }

    const float attribs[] = { view_origin_[0], view_origin_[1], view_origin_[2],
                              view_origin_[0] + 500 * v1[0], view_origin_[1] + 500 * v1[1], view_origin_[2] + 500 * v1[2],
                              view_origin_[0], view_origin_[1], view_origin_[2],
                              view_origin_[0] + 500 * v2[0], view_origin_[1] + 500 * v2[1], view_origin_[2] + 500 * v2[2],

                              view_origin_[0], view_origin_[1], view_origin_[2],
                              view_origin_[0] + 500 * v3[0], view_origin_[1] + 500 * v3[1], view_origin_[2] + 500 * v3[2],
                              view_origin_[0], view_origin_[1], view_origin_[2],
                              view_origin_[0] + 500 * v4[0], view_origin_[1] + 500 * v4[1], view_origin_[2] + 500 * v4[2]
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

    Ren::CheckError();
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

void GSOccTest::BlitDepthTiles() {
    using namespace GSOccTestInternal;

    int w = cull_ctx_.zbuf.w, h = cull_ctx_.zbuf.h;
    std::vector<uint8_t> pixels(w * h * 4);
    for (int x = 0; x < w; x++) {
        for (int y = 0; y < h; y++) {
            const auto *zr = swZbufGetTileRange(&cull_ctx_.zbuf, x, (h - y - 1));

            float z = zr->min;
            z = (2.0f * NEAR_CLIP) / (FAR_CLIP + NEAR_CLIP - z * (FAR_CLIP - NEAR_CLIP));
            pixels[4 * (y * w + x) + 0] = (uint8_t)(z * 255);
            pixels[4 * (y * w + x) + 1] = (uint8_t)(z * 255);
            pixels[4 * (y * w + x) + 2] = (uint8_t)(z * 255);
            pixels[4 * (y * w + x) + 3] = 255;
        }
    }

    glUseProgram(0);

    glRasterPos2f(-1 + 2* float(w) / game_->width, -1);

    glDisable(GL_DEPTH_TEST);
    glDrawPixels(w, h, GL_RGBA, GL_UNSIGNED_BYTE, &pixels[0]);

    glRasterPos2f(-1, -1);
}