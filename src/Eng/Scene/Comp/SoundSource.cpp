#include "SoundSource.h"

void SoundSource::Read(const JsObject &js_in, SoundSource &snd) {
    const JsArray &js_offset = js_in.at("offset").as_arr();
    snd.offset[0] = float(js_offset[0].as_num());
    snd.offset[1] = float(js_offset[1].as_num());
    snd.offset[2] = float(js_offset[2].as_num());

    if (js_in.Has("bone")) {
        snd.bone_name = Snd::String{js_in.at("bone").as_str().val.c_str()};
    }
    snd.bone_index = -1;
}

void SoundSource::Write(const SoundSource &snd, JsObject &js_out) {
    { // write offset
        JsArray js_offset;

        js_offset.Push(JsNumber{double(snd.offset[0])});
        js_offset.Push(JsNumber{double(snd.offset[1])});
        js_offset.Push(JsNumber{double(snd.offset[2])});

        js_out.Push("offset", std::move(js_offset));
    }

    if (!snd.bone_name.empty()) {
        JsString js_bone_name = JsString{snd.bone_name.c_str()};
        js_out.Push("bone", std::move(js_bone_name));
    }
}
