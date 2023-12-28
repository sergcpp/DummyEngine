#pragma once

#include <Snd/Source.h>

#include "Common.h"

namespace Eng {
struct SoundSource {
    Snd::Source snd_src;
    float offset[3];
    uint32_t bone_index;
    Snd::String bone_name;

    static void Read(const JsObjectP &js_in, SoundSource &ls);
    static void Write(const SoundSource &ls, JsObjectP &js_out);

    static const char *name() { return "sound"; }
};
} // namespace Eng
