#include "ScriptedSequence.h"

#include <limits>

#include <Snd/Utils.h>
#include <Sys/AssetFile.h>
#include <Sys/Json.h>
#include <Sys/MemBuf.h>

#include "../scene/SceneManager.h"

namespace ScriptedSequenceInternal {
const char *TrackTypeNames[] = {"actor", "camera"};

#if defined(__ANDROID__)
const char *MODELS_PATH = "./assets/models/";
const char *SOUNDS_PATH = "./assets/sounds/";
#else
const char *MODELS_PATH = "./assets_pc/models/";
const char *SOUNDS_PATH = "./assets_pc/sounds/";
#endif
} // namespace ScriptedSequenceInternal

const char *Eng::ScriptedSequence::ActionTypeNames[] = {"play", "look"};

Eng::ScriptedSequence::ScriptedSequence(Ren::Context &ren_ctx, Snd::Context &snd_ctx, Eng::SceneManager &scene_manager)
    : ren_ctx_(ren_ctx), snd_ctx_(snd_ctx), scene_manager_(scene_manager) {
    Reset();
}

void Eng::ScriptedSequence::Clear() {
    name_.clear();
    tracks_.clear();
    actions_.clear();
    choices_count_ = 0;
}

bool Eng::ScriptedSequence::Load(const std::string_view lookup_name, const Sys::JsObject &js_seq) {
    using namespace ScriptedSequenceInternal;

    Clear();

    if (!lookup_name.empty()) {
        lookup_name_ = lookup_name;
    }

    if (const size_t name_ndx = js_seq.IndexOf("name"); name_ndx < js_seq.Size()) {
        const Sys::JsString &js_name = js_seq[name_ndx].second.as_str();
        name_ = js_name.val;
    }

    if (const size_t tracks_ndx = js_seq.IndexOf("tracks"); tracks_ndx < js_seq.Size()) {
        const Sys::JsArray &js_tracks = js_seq[tracks_ndx].second.as_arr();
        tracks_.reserve(js_tracks.Size());
        for (const Sys::JsElement &js_track_el : js_tracks.elements) {
            const Sys::JsObject &js_track = js_track_el.as_obj();

            tracks_.emplace_back();
            Track &track = tracks_.back();

            track.name = js_track.at("name").as_str().val;
            track.type = eTrackType::Invalid;
            track.target = js_track.at("target").as_str().val;

            const Sys::JsString &js_track_type = js_track.at("type").as_str();
            for (int i = 0; i < int(eTrackType::Invalid); i++) {
                if (js_track_type.val == TrackTypeNames[i]) {
                    track.type = eTrackType(i);
                    break;
                }
            }

            if (track.type == eTrackType::Invalid) {
                ren_ctx_.log()->Error("Unknown track type %s.", js_track_type.val.c_str());
                return false;
            }

            const Sys::JsArray &js_track_actions = js_track.at("actions").as_arr();

            track.action_start = int(actions_.size());
            track.action_count = int(js_track_actions.Size());
            actions_.reserve(actions_.size() + track.action_count);

            for (const Sys::JsElement &js_act_el : js_track_actions.elements) {
                const Sys::JsObject &js_action = js_act_el.as_obj();

                actions_.emplace_back();
                SeqAction &action = actions_.back();
                action.type = eActionType::Invalid;

                const Sys::JsString &js_action_type = js_action.at("type").as_str();
                for (int i = 0; i < int(eActionType::Invalid); i++) {
                    if (js_action_type.val == ActionTypeNames[i]) {
                        action.type = eActionType(i);
                        break;
                    }
                }

                if (action.type == eActionType::Invalid) {
                    ren_ctx_.log()->Error("Unknown action type %s.", js_action_type.val.c_str());
                    return false;
                }

                action.time_beg = js_action.at("time_beg").as_num().val;

                if (const size_t time_end_ndx = js_action.IndexOf("time_end"); time_end_ndx < js_action.Size()) {
                    action.time_end = js_action[time_end_ndx].second.as_num().val;
                } else {
                    // TODO: derive from audio length etc.
                    ren_ctx_.log()->Error("Action end_time is not set");
                    return false;
                }

                if (const size_t pos_beg_ndx = js_action.IndexOf("pos_beg"); pos_beg_ndx < js_action.Size()) {
                    const Sys::JsArray &js_pos_beg = js_action[pos_beg_ndx].second.as_arr();

                    action.pos_beg[0] = float(js_pos_beg[0].as_num().val);
                    action.pos_beg[1] = float(js_pos_beg[1].as_num().val);
                    action.pos_beg[2] = float(js_pos_beg[2].as_num().val);

                    if (const size_t pos_end_ndx = js_action.IndexOf("pos_end"); pos_end_ndx < js_action.Size()) {
                        const Sys::JsArray &js_pos_end = js_action[pos_end_ndx].second.as_arr();

                        action.pos_end[0] = float(js_pos_end[0].as_num().val);
                        action.pos_end[1] = float(js_pos_end[1].as_num().val);
                        action.pos_end[2] = float(js_pos_end[2].as_num().val);
                    } else {
                        memcpy(action.pos_end, action.pos_beg, 3 * sizeof(float));
                    }
                } else {
                    action.pos_beg[0] = action.pos_beg[1] = action.pos_beg[2] = 0.0f;
                    memcpy(action.pos_end, action.pos_beg, 3 * sizeof(float));
                }

                if (const size_t rot_beg_ndx = js_action.IndexOf("rot_beg"); rot_beg_ndx < js_action.Size()) {
                    const Sys::JsArray &js_rot_beg = js_action[rot_beg_ndx].second.as_arr();

                    action.rot_beg[0] = float(js_rot_beg[0].as_num().val);
                    action.rot_beg[1] = float(js_rot_beg[1].as_num().val);
                    action.rot_beg[2] = float(js_rot_beg[2].as_num().val);

                    if (const size_t rot_end_ndx = js_action.IndexOf("rot_end"); rot_end_ndx < js_action.Size()) {
                        const Sys::JsArray &js_rot_end = js_action[rot_end_ndx].second.as_arr();

                        action.rot_end[0] = float(js_rot_end[0].as_num().val);
                        action.rot_end[1] = float(js_rot_end[1].as_num().val);
                        action.rot_end[2] = float(js_rot_end[2].as_num().val);
                    } else {
                        memcpy(action.rot_end, action.rot_beg, 3 * sizeof(float));
                    }
                } else {
                    action.rot_beg[0] = action.rot_beg[1] = action.rot_beg[2] = 0.0f;
                    memcpy(action.rot_end, action.rot_beg, 3 * sizeof(float));
                }

                if (const size_t anim_ndx = js_action.IndexOf("anim"); anim_ndx < js_action.Size()) {
                    const Sys::JsString &js_action_anim = js_action[anim_ndx].second.as_str();
                    const std::string anim_path = std::string(MODELS_PATH) + js_action_anim.val;

                    Sys::AssetFile in_file(anim_path);
                    if (!in_file) {
                        ren_ctx_.log()->Error("Failed to load %s", anim_path.c_str());
                        return false;
                    }
                    const size_t in_file_size = in_file.size();

                    std::unique_ptr<uint8_t[]> in_file_data(new uint8_t[in_file_size]);
                    in_file.Read((char *)&in_file_data[0], in_file_size);

                    Sys::MemBuf mem = {&in_file_data[0], in_file_size};
                    std::istream in_file_stream(&mem);

                    action.anim_ref = ren_ctx_.LoadAnimSequence(js_action_anim.val, in_file_stream);
                }

                if (const size_t caption_ndx = js_action.IndexOf("caption"); caption_ndx < js_action.Size()) {
                    const Sys::JsString &js_caption = js_action[caption_ndx].second.as_str();
                    action.caption = js_caption.val;
                }

                if (const size_t sound_ndx = js_action.IndexOf("sound"); sound_ndx < js_action.Size()) {
                    const Sys::JsString &js_action_sound = js_action[sound_ndx].second.as_str();
                    const auto &name = js_action_sound.val;
                    // check if sound was alpready loaded
                    Snd::eBufLoadStatus status;
                    action.sound_ref = snd_ctx_.LoadBuffer(name, {}, {}, &status);
                    if (status == Snd::eBufLoadStatus::CreatedDefault) {
                        const std::string sound_path = std::string(SOUNDS_PATH) + name;

                        // TODO: CHANGE THIS!!!

                        Sys::AssetFile in_file(sound_path);
                        const size_t in_file_size = in_file.size();

                        std::unique_ptr<uint8_t[]> in_file_data(new uint8_t[in_file_size]);
                        in_file.Read((char *)&in_file_data[0], in_file_size);

                        Sys::MemBuf mem = {&in_file_data[0], in_file_size};
                        std::istream in_file_stream(&mem);

                        int channels, samples_per_sec, bits_per_sample;
                        std::unique_ptr<uint8_t[]> samples;
                        const int size =
                            Snd::LoadWAV(in_file_stream, channels, samples_per_sec, bits_per_sample, samples);
                        assert(size);

                        Snd::BufParams params;
                        int samples_count;

                        if (channels == 1 && bits_per_sample == 8) {
                            params.format = Snd::eBufFormat::Mono8;
                            samples_count = size;
                        } else if (channels == 1 && bits_per_sample == 16) {
                            params.format = Snd::eBufFormat::Mono16;
                            samples_count = size / 2;
                        } else {
                            ren_ctx_.log()->Error("Unsupported sound format in file %s (%i "
                                                  "channels, %i bits per sample)",
                                                  sound_path.c_str(), channels, bits_per_sample);
                            return false;
                        }
                        params.samples_per_sec = samples_per_sec;

                        action.sound_ref = snd_ctx_.LoadBuffer(name, {&samples[0], size}, params, &status);
                        assert(status == Snd::eBufLoadStatus::Found || status == Snd::eBufLoadStatus::CreatedFromData);
                        action.sound_wave_tex = RenderSoundWave(name, &samples[0], samples_count, params);
                    }
                }

                if (const size_t sound_offset_ndx = js_action.IndexOf("sound_offset"); sound_offset_ndx < js_action.Size()) {
                    const Sys::JsNumber &js_action_sound_off = js_action[sound_offset_ndx].second.as_num();
                    action.sound_offset = js_action_sound_off.val;
                } else {
                    action.sound_offset = 0.0;
                }

                if (const size_t dof_ndx = js_action.IndexOf("dof"); dof_ndx < js_action.Size()) {
                    const Sys::JsLiteral js_action_dof = js_action[dof_ndx].second.as_lit();
                    action.dof = (js_action_dof.val == Sys::JsLiteralType::True);
                } else {
                    action.dof = false;
                }

                if (js_action.Has("fade_beg")) {
                    action.fade_beg = float(js_action.at("fade_beg").as_num().val);
                    action.fade_end = float(js_action.at("fade_end").as_num().val);
                } else {
                    action.fade_beg = action.fade_end = 0.0f;
                }
            }
        }
    }

    if (const size_t ending_ndx = js_seq.IndexOf("ending"); ending_ndx < js_seq.Size()) {
        const Sys::JsObject &js_ending = js_seq[ending_ndx].second.as_obj();
        end_time_ = js_ending.at("time_point").as_num().val;

        if (const size_t choices_ndx = js_ending.IndexOf("choices"); choices_ndx < js_ending.Size()) {
            const Sys::JsArray &js_choices = js_ending[choices_ndx].second.as_arr();
            for (const Sys::JsElement &js_choice_el : js_choices.elements) {
                const Sys::JsObject &js_choice = js_choice_el.as_obj();

                SeqChoice &choice = choices_[choices_count_++];

                choice.key = js_choice.at("key").as_str().val;

                if (const size_t text_ndx = js_choice.IndexOf("text"); text_ndx < js_choice.Size()) {
                    choice.text = js_choice[text_ndx].second.as_str().val;
                }

                choice.seq_name = js_choice.at("sequence").as_str().val;

                if (const size_t puzzle_ndx = js_choice.IndexOf("puzzle"); puzzle_ndx < js_choice.Size()) {
                    const Sys::JsString &js_choice_puz = js_choice[puzzle_ndx].second.as_str();
                    choice.puzzle_name = js_choice_puz.val;
                }
            }
        }

        if (const size_t choice_align_ndx = js_ending.IndexOf("choice_align"); choice_align_ndx < js_ending.Size()) {
            const Sys::JsString &js_choice_align = js_ending[choice_align_ndx].second.as_str();
            if (js_choice_align.val == "left") {
                choice_align_ = eChoiceAlign::Left;
            } else if (js_choice_align.val == "right") {
                choice_align_ = eChoiceAlign::Right;
            } else {
                choice_align_ = eChoiceAlign::Center;
            }
        } else {
            choice_align_ = eChoiceAlign::Center;
        }
    }

    Reset();

    return true;
}

void Eng::ScriptedSequence::Save(Sys::JsObject &js_seq) {
    using namespace ScriptedSequenceInternal;

    { // write name
        js_seq.Insert("name", Sys::JsString{name_});
    }

    { // write tracks
        Sys::JsArray js_tracks;
        for (const Track &track : tracks_) {
            Sys::JsObject js_track;
            js_track.Insert("name", Sys::JsString{track.name});
            js_track.Insert("type", Sys::JsString{TrackTypeNames[int(track.type)]});
            js_track.Insert("target", Sys::JsString{track.target});

            { // write actions
                Sys::JsArray js_actions;

                for (int i = track.action_start; i < track.action_start + track.action_count; i++) {
                    const SeqAction &action = actions_[i];

                    Sys::JsObject js_action;
                    js_action.Insert("type", Sys::JsString{ActionTypeNames[int(action.type)]});
                    js_action.Insert("time_beg", Sys::JsNumber{action.time_beg});
                    js_action.Insert("time_end", Sys::JsNumber{action.time_end});
                    { // write start pos
                        Sys::JsArray js_pos_beg;
                        js_pos_beg.Push(Sys::JsNumber{action.pos_beg[0]});
                        js_pos_beg.Push(Sys::JsNumber{action.pos_beg[1]});
                        js_pos_beg.Push(Sys::JsNumber{action.pos_beg[2]});
                        js_action.Insert("pos_beg", std::move(js_pos_beg));
                    }
                    if (action.pos_end[0] != action.pos_beg[0] || action.pos_end[1] != action.pos_beg[1] ||
                        action.pos_end[2] != action.pos_beg[2]) {
                        Sys::JsArray js_pos_end;
                        js_pos_end.Push(Sys::JsNumber{action.pos_end[0]});
                        js_pos_end.Push(Sys::JsNumber{action.pos_end[1]});
                        js_pos_end.Push(Sys::JsNumber{action.pos_end[2]});
                        js_action.Insert("pos_end", std::move(js_pos_end));
                    }
                    { // write start rot
                        Sys::JsArray js_rot_beg;
                        js_rot_beg.Push(Sys::JsNumber{action.rot_beg[0]});
                        js_rot_beg.Push(Sys::JsNumber{action.rot_beg[1]});
                        js_rot_beg.Push(Sys::JsNumber{action.rot_beg[2]});
                        js_action.Insert("rot_beg", std::move(js_rot_beg));
                    }
                    if (action.rot_end[0] != action.rot_beg[0] || action.rot_end[1] != action.rot_beg[1] ||
                        action.rot_end[2] != action.rot_beg[2]) {
                        Sys::JsArray js_rot_end;
                        js_rot_end.Push(Sys::JsNumber{action.rot_end[0]});
                        js_rot_end.Push(Sys::JsNumber{action.rot_end[1]});
                        js_rot_end.Push(Sys::JsNumber{action.rot_end[2]});
                        js_action.Insert("rot_end", std::move(js_rot_end));
                    }
                    if (action.anim_ref) {
                        js_action.Insert("anim", Sys::JsString{action.anim_ref->name()});
                    }
                    if (!action.caption.empty()) {
                        js_action.Insert("caption", Sys::JsString{action.caption});
                    }

                    if (action.sound_ref) {
                        js_action.Insert("sound", Sys::JsString{action.sound_ref->name()});
                    }

                    if (std::abs(action.sound_offset) > 0.001) {
                        js_action.Insert("sound_offset", Sys::JsNumber{action.sound_offset});
                    }

                    if (action.dof) {
                        js_action.Insert("dof", Sys::JsLiteral{Sys::JsLiteralType::True});
                    }

                    if (action.fade_beg != 0.0f || action.fade_end != 0.0f) {
                        js_action.Insert("fade_beg", Sys::JsNumber{(double)action.fade_beg});
                        js_action.Insert("fade_end", Sys::JsNumber{(double)action.fade_end});
                    }

                    js_actions.Push(std::move(js_action));
                }

                js_track.Insert("actions", std::move(js_actions));
            }

            js_tracks.Push(std::move(js_track));
        }
        js_seq.Insert("tracks", std::move(js_tracks));
    }

    { // write ending
        Sys::JsObject js_ending;
        js_ending.Insert("time_point", Sys::JsNumber{end_time_});

        Sys::JsArray js_choices;
        for (int i = 0; i < choices_count_; i++) {
            const SeqChoice &choice = choices_[i];

            Sys::JsObject js_choice;
            js_choice.Insert("key", Sys::JsString{choice.key});
            if (!choice.text.empty()) {
                js_choice.Insert("text", Sys::JsString{choice.text});
            }
            js_choice.Insert("sequence", Sys::JsString{choice.seq_name});
            if (!choice.puzzle_name.empty()) {
                js_choice.Insert("puzzle", Sys::JsString{choice.puzzle_name});
            }
            js_choices.Push(std::move(js_choice));
        }
        js_ending.Insert("choices", std::move(js_choices));

        if (choice_align_ != eChoiceAlign::Center) {
            js_ending.Insert("choice_align", Sys::JsString{choice_align_ == eChoiceAlign::Left ? "left" : "right"});
        }

        js_seq.Insert("ending", std::move(js_ending));
    }
}

void Eng::ScriptedSequence::Reset() {
    const SceneData &scene = scene_manager_.scene_data();

    // auto *transforms = (Transform *)scene.comp_store[CompTransform]->Get(0);
    auto *drawables = (Eng::Drawable *)scene.comp_store[CompDrawable]->SequentialData();
    auto *sounds = (Eng::SoundSource *)scene.comp_store[CompSoundSource]->SequentialData();

    for (Track &track : tracks_) {
        track.active_count = 0;
        track.time_beg = std::numeric_limits<double>::max();
        track.time_end = 0.0;
        track.target_actor = scene_manager_.FindObject(track.target.c_str());

        for (int i = track.action_start; i < track.action_start + track.action_count; i++) {
            SeqAction &action = actions_[i];

            action.is_active = false;
            if (track.target_actor != 0xffffffff) {
                SceneObject *actor = scene_manager_.GetObject(track.target_actor);
                Eng::Drawable &dr = drawables[actor->components[CompDrawable]];

                if (action.anim_ref) {
                    Ren::Mesh *target_mesh = dr.mesh.get();
                    Ren::Skeleton *target_skel = target_mesh->skel();
                    action.anim_id = target_skel->AddAnimSequence(action.anim_ref);
                }

                if (actor->comp_mask & CompSoundSourceBit) {
                    Eng::SoundSource &ss = sounds[actor->components[CompSoundSource]];
                    ss.snd_src.ResetBuffers();
                }
            } else if (track.type == eTrackType::Camera) {
                if (action.anim_ref) {
                    Ren::Mesh *target_mesh = scene_manager_.cam_rig();
                    Ren::Skeleton *target_skel = target_mesh->skel();
                    action.anim_id = target_skel->AddAnimSequence(action.anim_ref);
                }
            }

            track.time_beg = std::min(track.time_beg, action.time_beg);
            track.time_end = std::max(track.time_end, action.time_end);
        }
    }
}

void Eng::ScriptedSequence::Update(const double cur_time_s, bool playing) {
    for (Track &track : tracks_) {
        if (!track.active_count && (cur_time_s < track.time_beg || cur_time_s > track.time_end)) {
            continue;
        }

        for (int i = track.action_start; i < track.action_start + track.action_count; i++) {
            SeqAction &action = actions_[i];

            if (cur_time_s >= action.time_beg) {
                if (action.is_active) {
                    if (cur_time_s > action.time_end) {
                        // end action
                        action.is_active = false;
                        --track.active_count;
                    } else {
                        UpdateAction(track.target_actor, action, cur_time_s, playing);
                    }
                } else {
                    if (cur_time_s <= action.time_end) {
                        // start action
                        action.is_active = true;
                        ++track.active_count;

                        UpdateAction(track.target_actor, action, cur_time_s, playing);
                    }
                }
            }
        }
    }
}

void Eng::ScriptedSequence::UpdateAction(const uint32_t target_actor, SeqAction &action, double time_cur_s,
                                         bool playing) {
    const SceneData &scene = scene_manager_.scene_data();

    auto *transforms = (Eng::Transform *)scene.comp_store[CompTransform]->SequentialData();
    auto *drawables = (Eng::Drawable *)scene.comp_store[CompDrawable]->SequentialData();
    auto *anim_states = (Eng::AnimState *)scene.comp_store[CompAnimState]->SequentialData();
    auto *sounds = (Eng::SoundSource *)scene.comp_store[CompSoundSource]->SequentialData();

    const float t = float(time_cur_s - action.time_beg);
    const float t_norm = t / float(action.time_end - action.time_beg);

    const bool play_sound = playing || std::abs(t - last_t_) > 0.05;
    last_t_ = t;

    if (action.type == eActionType::Play) {
        SceneObject *actor_obj = scene_manager_.GetObject(target_actor);

        uint32_t invalidate_mask = 0;

        { // update position
            Eng::Transform &tr = transforms[actor_obj->components[CompTransform]];

            const Ren::Vec3f new_rot = Mix(Ren::MakeVec3(action.rot_beg), Ren::MakeVec3(action.rot_end), t_norm);

            tr.world_from_object_prev = tr.world_from_object;
            tr.world_from_object = Ren::Mat4f{1.0f};
            tr.world_from_object =
                Rotate(tr.world_from_object, new_rot[2] * Ren::Pi<float>() / 180.0f, Ren::Vec3f{0.0f, 0.0f, 1.0f});
            tr.world_from_object =
                Rotate(tr.world_from_object, new_rot[0] * Ren::Pi<float>() / 180.0f, Ren::Vec3f{1.0f, 0.0f, 0.0f});
            tr.world_from_object =
                Rotate(tr.world_from_object, new_rot[1] * Ren::Pi<float>() / 180.0f, Ren::Vec3f{0.0f, 1.0f, 0.0f});

            const Ren::Vec3f new_pos = Mix(Ren::MakeVec3(action.pos_beg), Ren::MakeVec3(action.pos_end), t_norm);
            memcpy(&tr.world_from_object[3][0], ValuePtr(new_pos), 3 * sizeof(float));

            if (memcmp(ValuePtr(tr.world_from_object_prev), ValuePtr(tr.world_from_object), sizeof(Ren::Mat4f)) != 0) {
                invalidate_mask |= CompTransformBit;
            }
        }

        { // update skeleton
            Eng::Drawable &dr = drawables[actor_obj->components[CompDrawable]];
            Eng::AnimState &as = anim_states[actor_obj->components[CompAnimState]];

            // keep previous palette for velocity calculation
            std::swap(as.matr_palette_curr, as.matr_palette_prev);
            std::swap(as.shape_palette_curr, as.shape_palette_prev);
            std::swap(as.shape_palette_count_curr, as.shape_palette_count_prev);

            Ren::Mesh *target_mesh = dr.mesh.get();
            Ren::Skeleton *target_skel = target_mesh->skel();

            target_skel->UpdateAnim(action.anim_id, t);
            target_skel->ApplyAnim(action.anim_id);
            target_skel->UpdateBones(&as.matr_palette_curr[0]);
            as.shape_palette_count_curr = target_skel->UpdateShapes(&as.shape_palette_curr[0]);

            invalidate_mask |= CompDrawableBit;
        }

        if ((actor_obj->comp_mask & CompSoundSourceBit)) {
            Eng::SoundSource &ss = sounds[actor_obj->components[CompSoundSource]];
            if (action.sound_ref) {
                const Eng::Transform &tr = transforms[actor_obj->components[CompTransform]];

                if (std::abs(t - action.sound_offset - ss.snd_src.GetOffset()) > 0.05f) {
                    ss.snd_src.SetOffset(t - float(action.sound_offset));
                }

                const Ren::Vec4f pos = tr.world_from_object * Ren::Vec4f{0.0f, 1.0f, 0.0f, 1.0f};
                ss.snd_src.set_position(ValuePtr(pos));

                if (play_sound) {
                    if (t >= action.sound_offset && t < (action.sound_offset + action.sound_ref->GetDurationS())) {

                        if (ss.snd_src.GetState() != Snd::eSrcState::Playing ||
                            ss.snd_src.GetBuffer(0).index() != action.sound_ref.index()) {
                            ss.snd_src.SetOffset(t - float(action.sound_offset));
                            ss.snd_src.Stop();
                            ss.snd_src.SetBuffer(action.sound_ref);
                            ss.snd_src.Play();
                        }
                    } else {
                        if (ss.snd_src.GetBuffer(0).index() != action.sound_ref.index()) {
                            ss.snd_src.ResetBuffers();
                        }
                    }
                } else {
                    ss.snd_src.Stop();
                }
            } else {
                ss.snd_src.Stop();
            }
        }

        scene_manager_.InvalidateObjects({&target_actor, 1}, invalidate_mask);

        if (!action.caption.empty()) {
            const double vis0 = std::min(std::max((action.time_end - time_cur_s) / 0.5, 0.0), 1.0);
            const double vis1 = std::min(std::max((time_cur_s - action.time_beg) / 0.5, 0.0), 1.0);
            const uint8_t caption_color[] = {255, 255, 255, uint8_t(std::min(vis0, vis1) * 255)};
            push_caption_signal.FireN(action.caption.c_str(), caption_color);
        }
    } else if (action.type == eActionType::Look) {
        Ren::Camera &cam = scene_manager_.main_cam();
        Ren::Mesh *cam_rig = scene_manager_.cam_rig();
        Ren::Skeleton *cam_skel = cam_rig->skel();

        if (action.anim_ref) {
            cam_skel->UpdateAnim(action.anim_id, t);
            cam_skel->ApplyAnim(action.anim_id);
        }

        Ren::Mat4f matrices[4];
        cam_skel->UpdateBones(matrices);

        Ren::Mat4f cam_mat, target_mat;
        cam_skel->bone_matrix("tip", cam_mat);
        cam_skel->bone_matrix("target", target_mat);

        const auto pos = Ren::Vec3d{cam_mat[3]};
        const Ren::Vec3d trg = pos - Ren::Vec3d{cam_mat[2]};

        cam.focus_depth = 3.0f;
        cam.focus_distance = float(Distance(pos, Ren::Vec3d{target_mat[3]}));
        cam.focus_far_mul = cam.focus_near_mul = action.dof ? 1.0f : 0.0f;
        cam.fade = Ren::Mix(action.fade_beg, action.fade_end, t_norm);
        cam.max_exposure = 32.0f;

        scene_manager_.SetupView(pos, trg, Ren::Vec3f{0.0f, 1.0f, 0.0f}, cam.angle(), Ren::Vec2f{0.0f}, cam.gamma,
                                 cam.min_exposure, cam.max_exposure);

        Snd::Source &amb_sound = scene_manager_.ambient_sound();
        if (action.sound_ref) {
            amb_sound.set_position(ValuePtr(cam.world_position()));

            if (play_sound) {
                if (t >= action.sound_offset && t < (action.sound_offset + action.sound_ref->GetDurationS())) {

                    if (amb_sound.GetState() != Snd::eSrcState::Playing ||
                        amb_sound.GetBuffer(0).index() != action.sound_ref.index()) {
                        amb_sound.SetOffset(t - float(action.sound_offset));
                        amb_sound.Stop();
                        amb_sound.SetBuffer(action.sound_ref);
                        amb_sound.Play();
                    }
                } else {
                    if (amb_sound.GetBuffer(0).index() != action.sound_ref.index()) {
                        amb_sound.ResetBuffers();
                    }
                }
            } else {
                amb_sound.Stop();
            }
        } else {
            amb_sound.Stop();
        }
    }
}

Ren::TextureRegionRef Eng::ScriptedSequence::RenderSoundWave(std::string_view name, const void *samples_data,
                                                             int samples_count, const Snd::BufParams &params) {
    { // check if sound-wave picture was already loaded
        Ren::eTexLoadStatus status;
        Ren::TextureRegionRef ret = ren_ctx_.LoadTextureRegion(name, {}, {}, ren_ctx_.current_cmd_buf(), &status);
        if (status == Ren::eTexLoadStatus::Found) {
            return ret;
        }
    }

    const auto *samples_i8 = reinterpret_cast<const int8_t *>(samples_data);
    const auto *samples_i16 = reinterpret_cast<const int16_t *>(samples_data);

    const float duration_s = float(samples_count) / params.samples_per_sec;

    const int tex_w = int(std::ceil(duration_s / SeqAction::SoundWaveStepS));
    const int tex_h = 16;
    const int tex_data_size = tex_w * tex_h * 4;

    std::vector<uint8_t> tex_data(tex_data_size);
    memset(&tex_data[0], 0x00, tex_data_size);
    int tex_data_pos = 0;

    for (int i = 0; i < samples_count; i += int(params.samples_per_sec * SeqAction::SoundWaveStepS)) {
        int min_val = std::numeric_limits<int>::max(), max_val = std::numeric_limits<int>::lowest();

        for (int j = 0; j < std::min(int(params.samples_per_sec * SeqAction::SoundWaveStepS), samples_count - i); j++) {
            if (params.format == Snd::eBufFormat::Mono8) {
                min_val = std::min(int(samples_i8[i + j]), min_val);
                max_val = std::max(int(samples_i8[i + j]), max_val);
            } else if (params.format == Snd::eBufFormat::Mono16) {
                min_val = std::min(int(samples_i16[i + j]), min_val);
                max_val = std::max(int(samples_i16[i + j]), max_val);
            } else {
                return {};
            }
        }

        if (params.format == Snd::eBufFormat::Mono8) {
            min_val = (min_val - std::numeric_limits<int8_t>::lowest()) * tex_h / std::numeric_limits<uint8_t>::max();
            max_val = (max_val - std::numeric_limits<int8_t>::lowest()) * tex_h / std::numeric_limits<uint8_t>::max();
        } else if (params.format == Snd::eBufFormat::Mono16) {
            min_val = (min_val - std::numeric_limits<int16_t>::lowest()) * tex_h / std::numeric_limits<uint16_t>::max();
            max_val = (max_val - std::numeric_limits<int16_t>::lowest()) * tex_h / std::numeric_limits<uint16_t>::max();
        } else {
            return {};
        }

        for (int j = 0; j < tex_h; j++) {
            if (j >= min_val && j < max_val) {
                const int k = 4 * (j * tex_w + tex_data_pos);
                tex_data[k + 0] = tex_data[k + 1] = 0;
                tex_data[k + 2] = 50;
                tex_data[k + 3] = 255;
            }
        }
        tex_data_pos++;
    }

    Ren::TexParams p;
    p.w = tex_w;
    p.h = tex_h;
    p.format = Ren::eTexFormat::RGBA8;

    Ren::eTexLoadStatus status;
    Ren::TextureRegionRef ret = ren_ctx_.LoadTextureRegion(name, tex_data, p, ren_ctx_.current_cmd_buf(), &status);
    assert(status == Ren::eTexLoadStatus::CreatedFromData);

    return ret;
}