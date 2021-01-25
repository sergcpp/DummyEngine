#pragma once

#include <cstdint>

#include <Ren/MVec.h>

#undef Always

namespace Ren {
enum class eCullFace : uint8_t { Front, Back, _Count };
enum class eTestFunc : uint8_t {
    Always,
    Never,
    Less,
    Equal,
    Greater,
    LEqual,
    NotEqual,
    GEqual,
    _Count
};
enum class eBlendFactor : uint8_t {
    Zero,
    One,
    SrcColor,
    OneMinusSrcColor,
    DstColor,
    OneMinusDstColor,
    SrcAlpha,
    OneMinusSrcAlpha,
    DstAlpha,
    OneMinusDstAlpha,
    _Count
};
enum class eStencilOp : uint8_t {
    Keep, Zero, Replace, Incr, Decr, Invert, _Count
};
enum class ePolygonMode : uint8_t {
    Fill, Line, _Count
};

struct RastState {
    struct {
        bool enabled = false;
        eCullFace face = eCullFace::Back;
    } cull_face;

    struct {
        bool enabled = false;
        eTestFunc func = eTestFunc::Always;
    } depth_test;

    bool depth_mask = true;

    struct {
        bool enabled = false;
        eBlendFactor src = eBlendFactor::Zero;
        eBlendFactor dst = eBlendFactor::Zero;
    } blend;

    struct {
        bool enabled = false;
        uint32_t mask = 0xff;
        eStencilOp stencil_fail = eStencilOp::Keep;
        eStencilOp depth_fail = eStencilOp::Keep;
        eStencilOp pass = eStencilOp::Keep;
        eTestFunc test_func = eTestFunc::Always;
        int test_ref = 0;
        uint32_t test_mask = 0xff;
    } stencil;

    ePolygonMode polygon_mode = ePolygonMode::Fill;

    struct {
        bool enabled = false;
        float factor = 0.0f;
        float units = 0.0f;
    } polygon_offset;

    Ren::Vec4i viewport;

    struct {
        bool enabled = false;
        Ren::Vec4i rect;
    } scissor;

    bool multisample = true;

    void Apply() { Apply(nullptr); }
    void ApplyChanged(const RastState& ref) { Apply(&ref); }

private:
    void Apply(const RastState* ref);
};
} // namespace Ren