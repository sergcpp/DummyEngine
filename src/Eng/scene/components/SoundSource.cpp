#include "SoundSource.h"

#include <Sys/Json.h>

void Eng::SoundSource::Read(const Sys::JsObjectP &js_in, SoundSource &snd) {
    const Sys::JsArrayP &js_offset = js_in.at("offset").as_arr();
    snd.offset[0] = float(js_offset[0].as_num().val);
    snd.offset[1] = float(js_offset[1].as_num().val);
    snd.offset[2] = float(js_offset[2].as_num().val);

    if (const size_t bone_ndx = js_in.IndexOf("bone"); bone_ndx < js_in.Size()) {
        snd.bone_name = Snd::String{js_in[bone_ndx].second.as_str().val};
    }
    snd.bone_index = -1;
}

void Eng::SoundSource::Write(const SoundSource &snd, Sys::JsObjectP &js_out) {
    const auto &alloc = js_out.elements.get_allocator();

    { // write offset
        Sys::JsArrayP js_offset(alloc);

        js_offset.Push(Sys::JsNumber{snd.offset[0]});
        js_offset.Push(Sys::JsNumber{snd.offset[1]});
        js_offset.Push(Sys::JsNumber{snd.offset[2]});

        js_out.Insert("offset", std::move(js_offset));
    }

    if (!snd.bone_name.empty()) {
        auto js_bone_name = Sys::JsStringP{snd.bone_name, alloc};
        js_out.Insert("bone", std::move(js_bone_name));
    }
}
