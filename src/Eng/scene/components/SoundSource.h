#pragma once

#include <Snd/Source.h>

#include "Common.h"

namespace Eng {
struct SoundSource {
    Snd::Source snd_src;
    float offset[3];
    uint32_t bone_index;
    Snd::String bone_name;

    static void Read(const Sys::JsObjectP &js_in, SoundSource &ls);
    static void Write(const SoundSource &ls, Sys::JsObjectP &js_out);

    static std::string_view name() { return "sound"; }
};
} // namespace Eng
