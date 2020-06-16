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

bool ScriptedSequence::Load(const char *lookup_name, const JsObject &js_seq) {
    using namespace ScriptedSequenceInternal;

    Clear();

    if (lookup_name) {
        lookup_name_ = lookup_name;
    }

    if (js_seq.Has("name")) {
        const JsString &js_name = js_seq.at("name").as_str();
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

                if (js_action.Has("pos_beg")) {
                    const JsArray &js_pos_beg = js_action.at("pos_beg").as_arr();

                    action.pos_beg[0] = (float)js_pos_beg[0].as_num();
                    action.pos_beg[1] = (float)js_pos_beg[1].as_num();
                    action.pos_beg[2] = (float)js_pos_beg[2].as_num();

                    if (js_action.Has("pos_end")) {
                        const JsArray &js_pos_end = js_action.at("pos_end").as_arr();

                        action.pos_end[0] = (float)js_pos_end[0].as_num();
                        action.pos_end[1] = (float)js_pos_end[1].as_num();
                        action.pos_end[2] = (float)js_pos_end[2].as_num();
                    } else {
                        memcpy(action.pos_end, action.pos_beg, 3 * sizeof(float));
                    }
                } else {
                    action.pos_beg[0] = action.pos_beg[1] = action.pos_beg[2] = 0.0f;
                    memcpy(action.pos_end, action.pos_beg, 3 * sizeof(float));
                }

                if (js_action.Has("rot_beg")) {
                    const JsArray &js_rot_beg = js_action.at("rot_beg").as_arr();

                    action.rot_beg[0] = (float)js_rot_beg[0].as_num();
                    action.rot_beg[1] = (float)js_rot_beg[1].as_num();
                    action.rot_beg[2] = (float)js_rot_beg[2].as_num();

                    if (js_action.Has("rot_end")) {
                        const JsArray &js_rot_end = js_action.at("rot_end").as_arr();

                        action.rot_end[0] = (float)js_rot_end[0].as_num();
                        action.rot_end[1] = (float)js_rot_end[1].as_num();
                        action.rot_end[2] = (float)js_rot_end[2].as_num();
                    } else {
                        memcpy(action.rot_end, action.rot_beg, 3 * sizeof(float));
                    }
                } else {
                    action.rot_beg[0] = action.rot_beg[1] = action.rot_beg[2] = 0.0f;
                    memcpy(action.rot_end, action.rot_beg, 3 * sizeof(float));
                }

                if (js_action.Has("anim")) {
                    const JsString &js_action_anim = js_action.at("anim").as_str();
                    const std::string anim_path =
                        std::string(MODELS_PATH) + js_action_anim.val;

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

                SeqChoice &choice = choices_[choices_count_++];

                choice.key = js_choice.at("key").as_str().val;

                if (js_choice.Has("text")) {
                    choice.text = js_choice.at("text").as_str().val;
                }

                choice.seq_name = js_choice.at("sequence").as_str().val;

                if (js_choice.Has("puzzle")) {
                    const JsString &js_choice_puz = js_choice.at("puzzle").as_str();
                    choice.puzzle_name = js_choice_puz.val;
                }
            }
        }
    }

    Reset();

    return true;
}

void ScriptedSequence::Save(JsObject &js_seq) {
    using namespace ScriptedSequenceInternal;

    { // write name
        js_seq.Push("name", JsString{name_});
    }

    { // write tracks
        JsArray js_tracks;
        for (const Track &track : tracks_) {
            JsObject js_track;
            js_track.Push("name", JsString{track.name});
            js_track.Push("type", JsString{TrackTypeNames[(int)track.type]});
            js_track.Push("target", JsString{track.target});

            { // write actions
                JsArray js_actions;

                for (int i = track.action_start;
                     i < track.action_start + track.action_count; i++) {
                    const SeqAction &action = actions_[i];

                    JsObject js_action;
                    js_action.Push("type", JsString{ActionTypeNames[(int)action.type]});
                    js_action.Push("time_beg", JsNumber{action.time_beg});
                    js_action.Push("time_end", JsNumber{action.time_end});
                    { // write start pos
                        JsArray js_pos_beg;
                        js_pos_beg.Push(JsNumber{(double)action.pos_beg[0]});
                        js_pos_beg.Push(JsNumber{(double)action.pos_beg[1]});
                        js_pos_beg.Push(JsNumber{(double)action.pos_beg[2]});
                        js_action.Push("pos_beg", std::move(js_pos_beg));
                    }
                    if (action.pos_end[0] != action.pos_beg[0] ||
                        action.pos_end[1] != action.pos_beg[1] ||
                        action.pos_end[2] != action.pos_beg[2]) {
                        JsArray js_pos_end;
                        js_pos_end.Push(JsNumber{(double)action.pos_end[0]});
                        js_pos_end.Push(JsNumber{(double)action.pos_end[1]});
                        js_pos_end.Push(JsNumber{(double)action.pos_end[2]});
                        js_action.Push("pos_end", std::move(js_pos_end));
                    }
                    { // write start rot
                        JsArray js_rot_beg;
                        js_rot_beg.Push(JsNumber{(double)action.rot_beg[0]});
                        js_rot_beg.Push(JsNumber{(double)action.rot_beg[1]});
                        js_rot_beg.Push(JsNumber{(double)action.rot_beg[2]});
                        js_action.Push("rot_beg", std::move(js_rot_beg));
                    }
                    if (action.rot_end[0] != action.rot_beg[0] ||
                        action.rot_end[1] != action.rot_beg[1] ||
                        action.rot_end[2] != action.rot_beg[2]) {
                        JsArray js_rot_end;
                        js_rot_end.Push(JsNumber{(double)action.rot_end[0]});
                        js_rot_end.Push(JsNumber{(double)action.rot_end[1]});
                        js_rot_end.Push(JsNumber{(double)action.rot_end[2]});
                        js_action.Push("rot_end", std::move(js_rot_end));
                    }
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
            if (!choice.text.empty()) {
                js_choice.Push("text", JsString{choice.text});
            }
            js_choice.Push("sequence", JsString{choice.seq_name});
            if (!choice.puzzle_name.empty()) {
                js_choice.Push("puzzle", JsString{choice.puzzle_name});
            }

            js_choices.Push(std::move(js_choice));
        }
        js_ending.Push("choices", std::move(js_choices));

        js_seq.Push("ending", std::move(js_ending));
    }
}

void ScriptedSequence::Reset() {
    const SceneData &scene = scene_manager_.scene_data();

    // auto *transforms = (Transform *)scene.comp_store[CompTransform]->Get(0);
    auto *drawables = (Drawable *)scene.comp_store[CompDrawable]->Get(0);

    assert(scene.comp_store[CompTransform]->IsSequential());
    assert(scene.comp_store[CompDrawable]->IsSequential());

    for (Track &track : tracks_) {
        track.active_count = 0;
        track.time_beg = std::numeric_limits<double>::max();
        track.time_end = 0.0;
        track.target_actor = scene_manager_.FindObject(track.target.c_str());

        for (int i = track.action_start; i < track.action_start + track.action_count;
             i++) {
            SeqAction &action = actions_[i];

            action.is_active = false;
            if (track.target_actor != 0xffffffff && action.anim_ref) {
                SceneObject *actor = scene_manager_.GetObject(track.target_actor);
                Drawable &target_drawable = drawables[actor->components[CompDrawable]];
                Ren::Mesh *target_mesh = target_drawable.mesh.get();
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

void ScriptedSequence::UpdateAction(const uint32_t target_actor, SeqAction &action,
                                    double time_cur_s) {
    const SceneData &scene = scene_manager_.scene_data();

    auto *transforms = (Transform *)scene.comp_store[CompTransform]->Get(0);
    auto *drawables = (Drawable *)scene.comp_store[CompDrawable]->Get(0);
    auto *anim_states = (AnimState *)scene.comp_store[CompAnimState]->Get(0);

    assert(scene.comp_store[CompTransform]->IsSequential());
    assert(scene.comp_store[CompDrawable]->IsSequential());
    assert(scene.comp_store[CompAnimState]->IsSequential());

    if (action.type == eActionType::Play) {
        SceneObject *actor_obj = scene_manager_.GetObject(target_actor);

        const float t = float(time_cur_s - action.time_beg);
        const float t_norm = t / float(action.time_end - action.time_beg);

        uint32_t invalidate_mask = 0;

        { // update position
            Transform &tr = transforms[actor_obj->components[CompTransform]];

            const Ren::Vec3f new_rot =
                Mix(Ren::MakeVec3(action.rot_beg), Ren::MakeVec3(action.rot_end), t_norm);

            tr.prev_mat = tr.mat;
            tr.mat = Ren::Mat4f{1.0f};
            tr.mat = Ren::Rotate(tr.mat, new_rot[2] * Ren::Pi<float>() / 180.0f,
                                 Ren::Vec3f{0.0f, 0.0f, 1.0f});
            tr.mat = Ren::Rotate(tr.mat, new_rot[0] * Ren::Pi<float>() / 180.0f,
                                 Ren::Vec3f{1.0f, 0.0f, 0.0f});
            tr.mat = Ren::Rotate(tr.mat, new_rot[1] * Ren::Pi<float>() / 180.0f,
                                 Ren::Vec3f{0.0f, 1.0f, 0.0f});

            const Ren::Vec3f new_pos =
                Mix(Ren::MakeVec3(action.pos_beg), Ren::MakeVec3(action.pos_end), t_norm);
            memcpy(&tr.mat[3][0], ValuePtr(new_pos), 3 * sizeof(float));

            if (memcmp(ValuePtr(tr.prev_mat), ValuePtr(tr.mat), sizeof(Ren::Mat4f)) !=
                0) {
                invalidate_mask |= CompTransformBit;
            }
        }

        { // update skeleton
            Drawable &dr = drawables[actor_obj->components[CompDrawable]];
            AnimState &as = anim_states[actor_obj->components[CompAnimState]];

            // keep previous palette for velocity calculation
            std::swap(as.matr_palette_curr, as.matr_palette_prev);
            std::swap(as.shape_palette_curr, as.shape_palette_prev);
            std::swap(as.shape_palette_count_curr, as.shape_palette_count_prev);

            Ren::Mesh *target_mesh = dr.mesh.get();
            Ren::Skeleton *target_skel = target_mesh->skel();

            target_skel->UpdateAnim(action.anim_id, t);
            target_skel->ApplyAnim(action.anim_id);
            target_skel->UpdateBones(&as.matr_palette_curr[0]);
            as.shape_palette_count_curr =
                target_skel->UpdateShapes(&as.shape_palette_curr[0]);

            invalidate_mask |= CompDrawableBit;
        }

        scene_manager_.InvalidateObjects(&target_actor, 1, invalidate_mask);

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
