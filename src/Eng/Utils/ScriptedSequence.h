#pragma once

#include <string>
#include <vector>

#include <Ren/Context.h>
#include <Sys/Signal_.h>

struct JsObject;
class SceneManager;
struct SceneObject;

namespace Gui {
class Renderer;
}

enum class eTrackType { Actor, Camera, Invalid };

enum class eActionType { Play, Look, Invalid };

struct SeqAction {
    eActionType type;
    double time_beg, time_end;
    std::string caption;

    // temp data
    bool is_active;
    Ren::AnimSeqRef anim_ref;
    int anim_id;
};

struct SeqChoice {
    std::string key;
    std::string text;
    std::string seq_name;

    // temp data
    int seq_id;
};

class ScriptedSequence {
    struct Track {
        std::string name, target;
        eTrackType type;
        int action_start, action_count;

        // temp data
        double time_beg, time_end;
        int active_count;
        SceneObject *target_actor;
    };

    Ren::Context &ctx_;
    SceneManager &scene_manager_;
    std::string name_;
    std::vector<Track> tracks_;
    std::vector<SeqAction> actions_;

    SeqChoice choices_[8];
    int choices_count_ = 0;

    double end_time_;

    SceneObject *FindActor(const char *name) const;
    void UpdateAction(SceneObject *target_actor, SeqAction &action, double time_cur_s);

  public:
    ScriptedSequence(Ren::Context &ctx_, SceneManager &scene_manager);

    const char *name() const { return name_.empty() ? nullptr : name_.c_str(); }

    double duration() const { return end_time_; }

    const char *GetTrackName(int track) const {
        if (track >= tracks_.size()) {
            return nullptr;
        }
        return tracks_[track].name.c_str();
    }

    const char *GetTrackTarget(int track) const {
        if (track >= tracks_.size()) {
            return nullptr;
        }
        return tracks_[track].target.c_str();
    }

    int GetActionsCount(int track) const {
        if (track >= tracks_.size()) {
            return 0;
        }
        return tracks_[track].action_count;
    }

    SeqAction *GetAction(int track, int action) {
        if (track >= tracks_.size() || action >= tracks_[track].action_count) {
            return nullptr;
        }
        return &actions_[tracks_[track].action_start + action];
    }

    int GetChoicesCount() const { return choices_count_; }

    SeqChoice *GetChoice(int i) {
        if (i < choices_count_) {
            return &choices_[i];
        }
        return nullptr;
    }

    SeqChoice *GetChoice(const char *key) {
        for (int i = 0; i < choices_count_; i++) {
            if (choices_[i].key == key) {
                return &choices_[i];
            }
        }
        return nullptr;
    }

    void Clear();
    bool Load(const JsObject &js_seq);
    void Save(JsObject &js_seq);

    void Reset();
    void Update(double cur_time_s);

    Sys::Signal<void(const char *text, const uint8_t color[4])> push_caption_signal;

    static const char *ActionTypeNames[];
};
