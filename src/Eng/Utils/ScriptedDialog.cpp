#include "ScriptedDialog.h"

#include "ScriptedSequence.h"

ScriptedDialog::ScriptedDialog(Ren::Context &ctx, SceneManager &scene_manager)
    : ctx_(ctx), scene_manager_(scene_manager) {}

void ScriptedDialog::Clear() { sequences_.clear(); }

bool ScriptedDialog::Load(const char *lookup_name, const JsObject &js_seq,
                          bool (*read_sequence)(const char *name, JsObject &js_seq)) {
    sequences_.emplace_back(ctx_, scene_manager_);
    if (sequences_.back().Load(lookup_name, js_seq)) {
        const int cur_seq_index = (int)sequences_.size() - 1;

        const JsObject &js_ending = js_seq.at("ending").as_obj();
        if (js_ending.Has("choices")) {
            const JsArray &js_choices = js_ending.at("choices").as_arr();

            int choice_index = 0;
            for (const JsElement &js_choice_el : js_choices.elements) {
                const JsObject &js_choice = js_choice_el.as_obj();
                const JsString &js_choice_key = js_choice.at("key").as_str();
                const JsString &js_seq_name = js_choice.at("sequence").as_str();

                int choice_seq_index = -1;
                for (int i = 0; i < (int)sequences_.size(); i++) {
                    const ScriptedSequence &seq = sequences_[i];
                    if (js_seq_name.val == seq.lookup_name()) {
                        choice_seq_index = i;
                        break;
                    }
                }

                if (choice_seq_index == -1) {
                    JsObject js_next_seq;
                    if (!read_sequence(js_seq_name.val.c_str(), js_next_seq)) {
                        ctx_.log()->Error("Failed to read sequence %s",
                                          js_seq_name.val.c_str());
                        return false;
                    }

                    choice_seq_index = (int)sequences_.size();

                    if (!Load(js_seq_name.val.c_str(), js_next_seq, read_sequence)) {
                        ctx_.log()->Error("Failed to load choise %s",
                                          js_choice_key.val.c_str());
                        return false;
                    }
                }

                SeqChoice* choice =
                    sequences_[cur_seq_index].GetChoice(js_choice_key.val.c_str());
                choice->seq_id = choice_seq_index;

                ++choice_index;
            }
        }
        return true;
    }
    return false;
}