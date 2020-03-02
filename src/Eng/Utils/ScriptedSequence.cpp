#include "ScriptedSequence.h"

#include <limits>

#include <Sys/Json.h>

#include "../Scene/SceneManager.h"

namespace ScriptedSequenceInternal {
const char *TrackTypeNames[] = {"actor", "camera"};

#if defined(__ANDROID__)
const char *MODELS_PATH = "./assets/models/";
#else
const char *MODELS_PATH = "./assets_pc/models/";
#endif
} // namespace ScriptedSequenceInternal

const char *ScriptedSequence::ActionTypeNames[] = {"play", "look"};

ScriptedSequence::ScriptedSequence(Ren::Context &ctx, SceneManager &scene_manager)
    : ctx_(ctx), scene_manager_(scene_manager) {
    Reset();
}

void ScriptedSequence::Clear() {
    name_.clear();
    tracks_.clear();
    actions_.clear();
    choices_count_ = 0;
}

bool ScriptedSequence::Load(const JsObject &js_seq) {
    using namespace ScriptedSequenceInternal;

    Clear();

    if (js_seq.Has("name")) {
        const JsString& js_name = js_seq.at("name").as_str();
        name_ = js_name.val;
    }

    if (js_seq.Has("tracks")) {
        const JsArray &js_tracks = js_seq.at("tracks").as_arr();
        tracks_.reserve(js_tracks.Size());
        for (const JsElement &js_track_el : js_tracks.elements) {
            const JsObject &js_track = js_track_el.as_obj();

            tracks_.emplace_back();
            Track &track = tracks_.back();

            track.name = js_track.at("name").as_str().val;
            track.type = eTrackType::Invalid;
            track.target = js_track.at("target").as_str().val;

            const JsString &js_track_type = js_track.at("type").as_str();
            for (int i = 0; i < int(eTrackType::Invalid); i++) {
                if (js_track_type.val == TrackTypeNames[i]) {
                    track.type = eTrackType(i);
                    break;
                }
            }

            if (track.type == eTrackType::Invalid) {
                ctx_.log()->Error("Unknown track type %s.", js_track_type.val.c_str());
                return false;
            }

            const JsArray &js_track_actions = js_track.at("actions").as_arr();

            track.action_start = (int)actions_.size();
            track.action_count = (int)js_track_actions.Size();
            actions_.reserve(actions_.size() + track.action_count);

            for (const JsElement &js_act_el : js_track_actions.elements) {
                const JsObject &js_action = js_act_el.as_obj();

                actions_.emplace_back();
                SeqAction &action = actions_.back();
                action.type = eActionType::Invalid;

                const JsString &js_action_type = js_action.at("type").as_str();
                for (int i = 0; i < int(eActionType::Invalid); i++) {
                    if (js_action_type.val == ActionTypeNames[i]) {
                        action.type = eActionType(i);
                        break;
                    }
                }

                if (action.type == eActionType::Invalid) {
                    ctx_.log()->Error("Unknown action type %s.",
                                      js_action_type.val.c_str());
                    return false;
                }

                action.time_beg = js_action.at("time_beg").as_num().val;

                if (js_action.Has("time_end")) {
                    action.time_end = js_action.at("time_end").as_num().val;
                } else {
                    // TODO: derive from audio length etc.
                    ctx_.log()->Error("Action end_time is not set");
                    return false;
                }

                if (js_action.Has("anim")) {
                    const JsString &js_action_anim = js_action.at("anim").as_str();
                    std::string anim_path = std::string(MODELS_PATH) + js_action_anim.val;

                    Sys::AssetFile in_file(anim_path.c_str());
                    size_t in_file_size = in_file.size();

                    std::unique_ptr<uint8_t[]> in_file_data(new uint8_t[in_file_size]);
                    in_file.Read((char *)&in_file_data[0], in_file_size);

                    Sys::MemBuf mem = {&in_file_data[0], in_file_size};
                    std::istream in_file_stream(&mem);

                    action.anim_ref =
                        ctx_.LoadAnimSequence(js_action_anim.val.c_str(), in_file_stream);
                }

                if (js_action.Has("caption")) {
                    const JsString &js_caption = js_action.at("caption").as_str();
                    action.caption = js_caption.val;
                }
            }
        }
    }

    if (js_seq.Has("ending")) {
        const JsObject &js_ending = js_seq.at("ending").as_obj();
        end_time_ = js_ending.at("time_point").as_num().val;

        if (js_ending.Has("choices")) {
            const JsArray &js_choices = js_ending.at("choices").as_arr();
            for (const JsElement &js_choice_el : js_choices.elements) {
                const JsObject &js_choice = js_choice_el.as_obj();
                const JsString &js_choice_key = js_choice.at("key").as_str();
                const JsString &js_choice_text = js_choice.at("text").as_str();
                const JsString &js_choice_seq = js_choice.at("sequence").as_str();

                SeqChoice &choice = choices_[choices_count_++];
                choice.key = js_choice_key.val;
                choice.text = js_choice_text.val;
                choice.seq_name = js_choice_seq.val;
            }
        }
    }

    Reset();

    return true;
}

void ScriptedSequence::Save(JsObject &js_seq) {
    using namespace ScriptedSequenceInternal;

    { // write name
        js_seq.Push("name", JsString{ name_ });
    }

    { // write tracks
        JsArray js_tracks;
        for (const Track &track : tracks_) {
            JsObject js_track;
            js_track.Push("name", JsString{track.name});
            js_track.Push("type", JsString{TrackTypeNames[(int)track.type]});

            { // write actions
                JsArray js_actions;

                for (int i = track.action_start;
                     i < track.action_start + track.action_count; i++) {
                    const SeqAction &action = actions_[i];

                    JsObject js_action;
                    js_action.Push("type", JsString{ActionTypeNames[(int)action.type]});
                    js_action.Push("time_beg", JsNumber{action.time_beg});
                    js_action.Push("time_end", JsNumber{action.time_end});
                    if (action.anim_ref) {
                        js_action.Push("anim", JsString{action.anim_ref->name().c_str()});
                    }
                    if (!action.caption.empty()) {
                        js_action.Push("caption", JsString{action.caption});
                    }

                    js_actions.Push(std::move(js_action));
                }

                js_track.Push("actions", std::move(js_actions));
            }

            js_tracks.Push(std::move(js_track));
        }
        js_seq.Push("tracks", std::move(js_tracks));
    }

    { // write ending
        JsObject js_ending;
        js_ending.Push("time_point", JsNumber{end_time_});

        JsArray js_choices;
        for (int i = 0; i < choices_count_; i++) {
            const SeqChoice &choice = choices_[i];

            JsObject js_choice;
            js_choice.Push("key", JsString{choice.key});
            js_choice.Push("text", JsString{choice.text});
            js_choice.Push("seq_name", JsString{choice.seq_name});

            js_choices.Push(std::move(js_choice));
        }
        js_ending.Push("choices", std::move(js_choices));

        js_seq.Push("ending", std::move(js_ending));
    }
}

void ScriptedSequence::Reset() {
    const SceneData &scene = scene_manager_.scene_data();

    const auto *transforms = (Transform *)scene.comp_store[CompTransform]->Get(0);
    const auto *drawables = (Drawable *)scene.comp_store[CompDrawable]->Get(0);

    assert(scene.comp_store[CompTransform]->IsSequential());
    assert(scene.comp_store[CompDrawable]->IsSequential());

    for (Track &track : tracks_) {
        track.active_count = 0;
        track.time_beg = std::numeric_limits<double>::max();
        track.time_end = 0.0;
        track.target_actor = FindActor(track.target.c_str());

        for (int i = track.action_start; i < track.action_start + track.action_count;
             i++) {
            SeqAction &action = actions_[i];

            action.is_active = false;
            if (track.target_actor && action.anim_ref) {
                auto *target_drawable =
                    (Drawable *)&drawables[track.target_actor->components[CompDrawable]];
                Ren::Mesh *target_mesh = target_drawable->mesh.get();
                Ren::Skeleton *target_skel = target_mesh->skel();
                action.anim_id = target_skel->AddAnimSequence(action.anim_ref);
            }

            track.time_beg = std::min(track.time_beg, action.time_beg);
            track.time_end = std::max(track.time_end, action.time_end);
        }
    }
}

void ScriptedSequence::Update(const double cur_time_s) {
    for (Track &track : tracks_) {
        if (!track.active_count &&
            (cur_time_s < track.time_beg || cur_time_s > track.time_end))
            continue;

        for (int i = track.action_start; i < track.action_start + track.action_count;
             i++) {
            SeqAction &action = actions_[i];

            if (cur_time_s >= action.time_beg) {
                if (action.is_active) {
                    if (cur_time_s > action.time_end) {
                        // end action
                        action.is_active = false;
                        --track.active_count;
                    } else {
                        UpdateAction(track.target_actor, action, cur_time_s);
                    }
                } else {
                    if (cur_time_s <= action.time_end) {
                        // start action
                        action.is_active = true;
                        ++track.active_count;

                        UpdateAction(track.target_actor, action, cur_time_s);
                    }
                }
            }
        }
    }
}

SceneObject *ScriptedSequence::FindActor(const char *name) const {
    uint32_t obj_id = scene_manager_.FindObject(name);
    if (obj_id == 0xffffffff) {
        return nullptr;
    }
    return scene_manager_.GetObject(obj_id);
}

void ScriptedSequence::UpdateAction(SceneObject *target_actor, SeqAction &action,
                                    double time_cur_s) {
    const SceneData &scene = scene_manager_.scene_data();

    const auto *transforms = (Transform *)scene.comp_store[CompTransform]->Get(0);
    const auto *drawables = (Drawable *)scene.comp_store[CompDrawable]->Get(0);
    const auto *anim_states = (AnimState *)scene.comp_store[CompAnimState]->Get(0);

    assert(scene.comp_store[CompTransform]->IsSequential());
    assert(scene.comp_store[CompDrawable]->IsSequential());
    assert(scene.comp_store[CompAnimState]->IsSequential());

    if (action.type == eActionType::Play) {
        { // update skeleton
            auto *dr = (Drawable *)&drawables[target_actor->components[CompDrawable]];
            auto *as = (AnimState *)&anim_states[target_actor->components[CompAnimState]];
            Ren::Mesh *target_mesh = dr->mesh.get();
            Ren::Skeleton *target_skel = target_mesh->skel();

            const float t = float(time_cur_s - action.time_beg);
            target_skel->UpdateAnim(action.anim_id, t);
            target_skel->ApplyAnim(action.anim_id);
            target_skel->UpdateBones(as->matr_palette);

            target_actor->change_mask |= CompDrawableBit;
        }

        if (!action.caption.empty()) {
            const double vis0 =
                std::min(std::max((action.time_end - time_cur_s) / 0.5, 0.0), 1.0);
            const double vis1 =
                std::min(std::max((time_cur_s - action.time_beg) / 0.5, 0.0), 1.0);
            const uint8_t caption_color[] = {255, 255, 255,
                                             uint8_t(std::min(vis0, vis1) * 255)};
            push_caption_signal.FireN(action.caption.c_str(), caption_color);
        }
    } else if (action.type == eActionType::Look) {
    }
}
