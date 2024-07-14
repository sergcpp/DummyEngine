#pragma once

#include <cstdint>

#include <string_view>

#include <Sys/Signal_.h>

namespace Eng {
class ScriptedDialog;
class ScriptedSequence;
} // namespace Eng

class DialogController {
  public:
    enum class eState { Paused, Sequence, ChoicePlaying, ChoicePaused, ChoicePuzzle, ChoiceTransition };

    DialogController();

    eState state() const { return state_; }

    void SetDialog(Eng::ScriptedDialog *dialog);

    void Play(double cur_time_s);
    void Pause();
    void Update(double cur_time_s);

    double GetPlayTime() const { return play_time_s_; }
    void SetPlayTime(double cur_time_s, double play_time_s);

    Eng::ScriptedSequence *GetCurSequence() { return cur_seq_; }
    void SetCurSequence(int id);

    void MakeChoice(std::string_view key);
    void ContinueChoice();

    Sys::SignalN<void(std::string_view text, const uint8_t color[4])> push_caption_signal;
    Sys::SignalN<void(std::string_view key, std::string_view text, int off)> push_choice_signal;

    Sys::SignalN<void(int id)> switch_sequence_signal;
    Sys::SignalN<void(std::string_view puzzle)> start_puzzle_signal;

  private:
    Eng::ScriptedDialog *dialog_ = nullptr;
    Eng::ScriptedSequence *cur_seq_ = nullptr;
    int next_seq_id_ = -1;

    eState state_ = eState::Paused;

    double play_started_time_s_ = 0.0, play_time_s_ = 0.0;

    void OnPushCaption(std::string_view text, const uint8_t color[4]);
};