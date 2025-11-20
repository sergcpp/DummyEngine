#pragma once

#include <string>
#include <vector>

#include <Gui/Signal.h>
#include <Ren/Context.h>
#include <Snd/Context.h>

namespace Sys {
template <typename T, typename FallBackAllocator> class MultiPoolAllocator;
template <typename Alloc> struct JsObjectT;
using JsObject = JsObjectT<std::allocator<char>>;
using JsObjectP = JsObjectT<Sys::MultiPoolAllocator<char, std::allocator<char>>>;
} // namespace Sys

namespace Gui {
class Renderer;
}

namespace Eng {
class SceneManager;
struct SceneObject;

enum class eTrackType { Actor, Camera, Invalid };
enum class eActionType { Play, Look, Invalid };

struct SeqAction {
    eActionType type;
    double time_beg, time_end;
    float pos_beg[3], pos_end[3];
    float rot_beg[3], rot_end[3];
    float fade_beg, fade_end;
    double sound_offset;
    std::string caption;

    // temp data
    bool is_active;
    Ren::AnimSeqRef anim_ref;
    int anim_id;
    Snd::BufferRef sound_ref;
    bool dof;

    static constexpr float SoundWaveStepS = 0.02f;
    Ren::ImageRegionRef sound_wave_tex;
};

enum class eChoiceAlign { Center, Left, Right };

struct SeqChoice {
    std::string key;
    std::string text;
    std::string seq_name;
    std::string puzzle_name;

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
        uint32_t target_actor;
    };

    Ren::Context &ren_ctx_;
    Snd::Context &snd_ctx_;
    SceneManager &scene_manager_;
    std::string lookup_name_, name_;
    std::vector<Track> tracks_;
    std::vector<SeqAction> actions_;

    SeqChoice choices_[8];
    int choices_count_ = 0;

    eChoiceAlign choice_align_ = eChoiceAlign::Center;
    double end_time_, last_t_ = 0.0;

    void UpdateAction(uint32_t target_actor, SeqAction &action, double time_cur_s, bool playing);

    Ren::ImageRegionRef RenderSoundWave(std::string_view name, const void *samples_data, int samples_count,
                                          const Snd::BufParams &params);

  public:
    ScriptedSequence(Ren::Context &ren_ctx, Snd::Context &snd_ctx, SceneManager &scene_manager);

    std::string_view lookup_name() const { return lookup_name_; }
    std::string_view name() const { return name_; }

    double duration() const { return end_time_; }
    eChoiceAlign choice_align() const { return choice_align_; }

    std::string_view GetTrackName(int track) const {
        if (track >= int(tracks_.size())) {
            return {};
        }
        return tracks_[track].name;
    }

    std::string_view GetTrackTarget(int track) const {
        if (track >= int(tracks_.size())) {
            return {};
        }
        return tracks_[track].target;
    }

    int GetActionsCount(int track) const {
        if (track >= int(tracks_.size())) {
            return 0;
        }
        return tracks_[track].action_count;
    }

    SeqAction *GetAction(int track, int action) {
        if (track >= int(tracks_.size()) || action >= tracks_[track].action_count) {
            return nullptr;
        }
        return &actions_[tracks_[track].action_start + action];
    }

    int GetChoicesCount() const { return choices_count_; }

    const SeqChoice *GetChoice(int i) const {
        if (i < choices_count_) {
            return &choices_[i];
        }
        return nullptr;
    }

    SeqChoice *GetChoice(int i) {
        if (i < choices_count_) {
            return &choices_[i];
        }
        return nullptr;
    }

    const SeqChoice *GetChoice(std::string_view key) const {
        for (int i = 0; i < choices_count_; i++) {
            if (choices_[i].key == key) {
                return &choices_[i];
            }
        }
        return nullptr;
    }

    SeqChoice *GetChoice(std::string_view key) {
        for (int i = 0; i < choices_count_; i++) {
            if (choices_[i].key == key) {
                return &choices_[i];
            }
        }
        return nullptr;
    }

    void Clear();
    bool Load(std::string_view lookup_name, const Sys::JsObject &js_seq);
    void Save(Sys::JsObject &js_seq);

    void Reset();
    void Update(double cur_time_s, bool playing);

    Gui::Signal<void(std::string_view text, const uint8_t color[4])> push_caption_signal;

    static const char *ActionTypeNames[];
};
} // namespace Eng