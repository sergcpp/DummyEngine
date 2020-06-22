#pragma once

#include <Snd/Source.h>

#include "Common.h"

struct SoundSource {
    Snd::Source snd_src;
    float offset[3];
    uint32_t bone_index;
    Snd::String bone_name;

    static void Read(const JsObject &js_in, SoundSource &ls);
    static void Write(const SoundSource &ls, JsObject &js_out);

    static const char *name() { return "sound"; }
};
