#include "SoundSource.h"

#include <Sys/Json.h>

void SoundSource::Read(const JsObjectP &js_in, SoundSource &snd) {
    const JsArrayP &js_offset = js_in.at("offset").as_arr();
    snd.offset[0] = float(js_offset[0].as_num().val);
    snd.offset[1] = float(js_offset[1].as_num().val);
    snd.offset[2] = float(js_offset[2].as_num().val);

    if (js_in.Has("bone")) {
        snd.bone_name = Snd::String{js_in.at("bone").as_str().val.c_str()};
    }
    snd.bone_index = -1;
}

void SoundSource::Write(const SoundSource &snd, JsObjectP &js_out) {
    const auto &alloc = js_out.elements.get_allocator();

    { // write offset
        JsArrayP js_offset(alloc);

        js_offset.Push(JsNumber{snd.offset[0]});
        js_offset.Push(JsNumber{snd.offset[1]});
        js_offset.Push(JsNumber{snd.offset[2]});

        js_out.Push("offset", std::move(js_offset));
    }

    if (!snd.bone_name.empty()) {
        auto js_bone_name = JsStringP{snd.bone_name.c_str(), alloc};
        js_out.Push("bone", std::move(js_bone_name));
    }
}
