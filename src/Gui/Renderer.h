#pragma once

#include <vector>

#include <Ren/Program.h>
#include <Ren/Texture.h>

#include <Ren/MVec.h>

struct JsObject;

namespace Ren {
class Context;
}

namespace Gui {
const char GL_DEFINES_KEY[] = "gl_defines";
const char UI_PROGRAM_NAME[] = "ui_program";

enum ePrimitiveType { PrimTriangle };
enum eBlendMode { BlAlpha, BlColor };
enum eDrawMode { DrPassthrough, DrDistanceField };

using Ren::Vec2f;
using Ren::Vec2i;
using Ren::Vec3f;
using Ren::Vec4f;

class Renderer {
public:
    Renderer(Ren::Context &ctx, const JsObject &config);
    ~Renderer();

    void BeginDraw();
    void EndDraw();

    struct DrawParams {
        DrawParams(const Vec4f &col_and_mode, float z_val, eBlendMode blend_mode, const Vec2i scissor_test[2])
            : col_and_mode_(col_and_mode), z_val_(z_val), blend_mode_(blend_mode) {
            scissor_test_[0] = scissor_test[0];
            scissor_test_[1] = scissor_test[1];
        }

        const Vec4f &col_and_mode() const {
            return col_and_mode_;
        }
        float z_val() const {
            return z_val_;
        }
        eBlendMode blend_mode() const {
            return blend_mode_;
        }
        const Vec2i *scissor_test() const {
            return scissor_test_;
        }
        const Vec2i &scissor_test(int i) const {
            return scissor_test_[i];
        }

        bool operator==(const DrawParams &rhs) const {
            return col_and_mode_ == rhs.col_and_mode_ &&
                   z_val_ == rhs.z_val_ &&
                   blend_mode_ == rhs.blend_mode_ &&
                   scissor_test_[0] == rhs.scissor_test_[0] &&
                   scissor_test_[1] == rhs.scissor_test_[1];
        }
        bool operator!=(const DrawParams &rhs) const {
            return !(*this == rhs);
        }
    private:
        friend class Renderer;
        Vec4f       col_and_mode_;
        float		z_val_;
        eBlendMode  blend_mode_;
        Vec2i	    scissor_test_[2];
    };

    const DrawParams &GetParams() const {
        return params_.back();
    }

    void PushParams(const DrawParams &params) {
        params_.push_back(params);
    }

    template<class... Args>
    void EmplaceParams(Args &&... args) {
        params_.emplace_back(args...);
    }

    void PopParams() {
        params_.pop_back();
    }

    void DrawImageQuad(const Ren::Texture2DRef &tex,
                       const Vec2f dims[2],
                       const Vec2f uvs[2]);

    void DrawUIElement(const Ren::Texture2DRef &tex, ePrimitiveType prim_type,
                       const std::vector<float> &pos, const std::vector<float> &uvs,
                       const std::vector<uint16_t> &indices);
private:
    Ren::Context &ctx_;
    Ren::ProgramRef ui_program_;
#if defined(USE_GL_RENDER)
    uint32_t main_vao_;
    uint32_t attribs_buf_id_, indices_buf_id_;
#endif
    std::vector<DrawParams> params_;

    void ApplyParams(Ren::ProgramRef &p, const DrawParams &params);
};
}

