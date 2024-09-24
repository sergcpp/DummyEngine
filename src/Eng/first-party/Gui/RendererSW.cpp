#include "Renderer.h"

#pragma warning(push)
#pragma warning(disable : 4505)

#include <Ren/Context.h>
#include <Ren/SW/SW.h>
#include <Sys/Json.h>

namespace Gui {
enum {
    A_POS,
    A_UV
};

enum { V_UV };

enum { U_COL };

enum { DIFFUSEMAP_SLOT };

inline void BindTexture(int slot, uint32_t tex) {
    swActiveTexture((SWenum)(SW_TEXTURE0 + slot));
    swBindTexture((SWint)tex);
}
}

extern "C" {
    VSHADER ui_program_vs(VS_IN, VS_OUT) {
        using namespace Ren;

        Vec2f uv = MakeVec2(V_FATTR(A_UV));
        V_FVARYING(V_UV)[0] = uv[0];
        V_FVARYING(V_UV)[1] = uv[1];

        Vec3f pos = MakeVec3(V_FATTR(A_POS));
        V_POS_OUT[0] = pos[0];
        V_POS_OUT[1] = pos[1];
        V_POS_OUT[2] = pos[2];
        V_POS_OUT[3] = 1;

        ((void)uniforms);
    }

    FSHADER ui_program_fs(FS_IN, FS_OUT) {
        using namespace Ren;

        float rgba[4];
        TEXTURE(DIFFUSEMAP_SLOT, F_FVARYING_IN(V_UV), rgba);

        Vec3f temp = MakeVec3(F_UNIFORM(U_COL));
        Vec4f col = MakeVec4(rgba) * Vec4f(temp[0], temp[1], temp[2], 1.0f);
        F_COL_OUT[0] = col[0];
        F_COL_OUT[1] = col[1];
        F_COL_OUT[2] = col[2];
        F_COL_OUT[3] = col[3];

        ((void)b_discard);
    }
}

Gui::Renderer::Renderer(Ren::Context &ctx, const JsObject &/*config*/) : ctx_(ctx) {
    Ren::Attribute attrs[] = { { "pos", A_POS, SW_VEC3, 1 }, { "uvs", A_UV, SW_VEC2, 1 }, {} };
    Ren::Uniform unifs[] = { { "col", U_COL, SW_VEC3, 1 }, {} };
    ui_program_ = ctx.LoadProgramSW(UI_PROGRAM_NAME, (void *)ui_program_vs, (void *)ui_program_fs, 2,
                                    attrs, unifs, nullptr);
}

Gui::Renderer::~Renderer() {

}

void Gui::Renderer::BeginDraw() {
    Ren::Program *p = ui_program_.get();

    swUseProgram(p->prog_id());
    const Ren::Vec3f white = { 1, 1, 1 };
    swSetUniform(U_COL, SW_VEC3, Ren::ValuePtr(white));

    swBindBuffer(SW_ARRAY_BUFFER, 0);
    swBindBuffer(SW_INDEX_BUFFER, 0);

    swDisable(SW_PERSPECTIVE_CORRECTION);
    swDisable(SW_FAST_PERSPECTIVE_CORRECTION);
    swDisable(SW_DEPTH_TEST);
    swEnable(SW_BLEND);

    Ren::Vec2i scissor_test[2] = { { 0, 0 }, { ctx_.w(), ctx_.h() } };
    this->EmplaceParams(Ren::Vec3f{1}, 0, Alpha, scissor_test);
}

void Gui::Renderer::EndDraw() {
    swEnable(SW_FAST_PERSPECTIVE_CORRECTION);
    swEnable(SW_DEPTH_TEST);
    swDisable(SW_BLEND);

    this->PopParams();
}

void Gui::Renderer::PushImageQuad(const Ren::Tex2DRef &tex, const Ren::Vec2f dims[2], const Ren::Vec2f uvs[2]) {
    const float vertices[] = { dims[0][0], dims[0][1], 0,
                               uvs[0][0], uvs[0][1],

                               dims[0][0], dims[0][1] + dims[1][1], 0,
                               uvs[0][0], uvs[1][1],

                               dims[0][0] + dims[1][0], dims[0][1] + dims[1][1], 0,
                               uvs[1][0], uvs[1][1],

                               dims[0][0] + dims[1][0], dims[0][1], 0,
                               uvs[1][0], uvs[0][1]
                             };

    const unsigned char indices[] = { 2, 1, 0,
                                      3, 2, 0
                                    };

    BindTexture(DIFFUSEMAP_SLOT, tex->tex_id());

    swVertexAttribPointer(A_POS, sizeof(float) * 3, sizeof(float) * 5, &vertices[0]);
    swVertexAttribPointer(A_UV, sizeof(float) * 2, sizeof(float) * 5, &vertices[3]);
    swDrawElements(SW_TRIANGLES, 6, SW_UNSIGNED_BYTE, indices);
}

void Gui::Renderer::DrawUIElement(const Ren::Tex2DRef &tex, ePrimitiveType prim_type,
                                  const std::vector<float> &pos, const std::vector<float> &uvs,
                                  const std::vector<unsigned char> &indices) {
    if (pos.empty()) return;

    assert(pos.size() / 5 < 0xff);

    const DrawParams &cur_params = params_.back();
    this->ApplyParams(ui_program_, cur_params);

    BindTexture(DIFFUSEMAP_SLOT, tex->tex_id());

    swVertexAttribPointer(A_POS, sizeof(float) * 3, 0, (void *)&pos[0]);
    swVertexAttribPointer(A_UV, sizeof(float) * 2, 0, (void *)&uvs[0]);

    if (prim_type == PrimTriangle) {
        swDrawElements(SW_TRIANGLES, (SWuint)indices.size(), SW_UNSIGNED_BYTE, &indices[0]);
    }
}

void Gui::Renderer::ApplyParams(Ren::ProgramRef &, const DrawParams &params) {
    swSetUniform(U_COL, SW_VEC3, Ren::ValuePtr(params.col()));
}

#pragma warning(pop)