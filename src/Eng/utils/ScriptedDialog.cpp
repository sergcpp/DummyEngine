#include "ScriptedDialog.h"

#include "ScriptedSequence.h"

Eng::ScriptedDialog::ScriptedDialog(Ren::Context &ren_ctx, Snd::Context &snd_ctx, SceneManager &scene_manager)
    : ren_ctx_(ren_ctx), snd_ctx_(snd_ctx), scene_manager_(scene_manager) {}

void Eng::ScriptedDialog::Clear() {
    for (ScriptedSequence &seq : sequences_) {
        seq.Reset();
    }
    sequences_.clear();
}

bool Eng::ScriptedDialog::Load(const std::string_view lookup_name, const Sys::JsObject &js_seq,
                               bool (*read_sequence)(const std::string_view name, Sys::JsObject &js_seq)) {
    sequences_.emplace_back(ren_ctx_, snd_ctx_, scene_manager_);
    if (sequences_.back().Load(lookup_name, js_seq)) {
        const int cur_seq_index = int(sequences_.size()) - 1;

        const Sys::JsObject &js_ending = js_seq.at("ending").as_obj();
        if (const size_t choices_ndx = js_ending.IndexOf("choices"); choices_ndx < js_ending.Size()) {
            const Sys::JsArray &js_choices = js_ending[choices_ndx].second.as_arr();

            for (const Sys::JsElement &js_choice_el : js_choices.elements) {
                const Sys::JsObject &js_choice = js_choice_el.as_obj();
                const Sys::JsString &js_choice_key = js_choice.at("key").as_str();
                const Sys::JsString &js_seq_name = js_choice.at("sequence").as_str();

                int choice_seq_index = -1;
                for (int i = 0; i < int(sequences_.size()); i++) {
                    const ScriptedSequence &seq = sequences_[i];
                    if (js_seq_name.val == seq.lookup_name()) {
                        choice_seq_index = i;
                        break;
                    }
                }

                if (choice_seq_index == -1) {
                    Sys::JsObject js_next_seq;
                    if (!read_sequence(js_seq_name.val, js_next_seq)) {
                        ren_ctx_.log()->Error("Failed to read sequence %s", js_seq_name.val.c_str());
                        return false;
                    }

                    choice_seq_index = int(sequences_.size());

                    if (!Load(js_seq_name.val, js_next_seq, read_sequence)) {
                        ren_ctx_.log()->Error("Failed to load choise %s", js_choice_key.val.c_str());
                        return false;
                    }
                }

                SeqChoice *choice = sequences_[cur_seq_index].GetChoice(js_choice_key.val);
                choice->seq_id = choice_seq_index;
            }
        }
        return true;
    }
    return false;
}