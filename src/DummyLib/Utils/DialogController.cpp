#include "DialogController.h"

#include <Eng/Utils/ScriptedDialog.h>
#include <Eng/Utils/ScriptedSequence.h>

DialogController::DialogController() = default;

void DialogController::SetDialog(ScriptedDialog *dialog) {
    dialog_ = dialog;
    SetCurSequence(0);
}

void DialogController::Play(const double cur_time_s) {
    if (dialog_) {
        state_ = eState::Sequence;
        play_started_time_s_ = cur_time_s - play_time_s_;
    }
}

void DialogController::Pause() { state_ = eState::Paused; }

void DialogController::Update(const double cur_time_s) {
    if (state_ == eState::Paused) {
        if (cur_seq_) {
            const double end_time_s = cur_seq_->duration();

            if (play_time_s_ >= end_time_s) {
                if (cur_seq_->GetChoicesCount()) {
                    state_ = eState::ChoicePaused;
                }
                play_time_s_ = end_time_s;
            }

            cur_seq_->Update(play_time_s_, false);
        }
    } else if (state_ == eState::Sequence) {
        if (cur_seq_) {
            const double end_time_s = cur_seq_->duration();
            play_time_s_ = cur_time_s - play_started_time_s_;

            if (play_time_s_ >= end_time_s) {
                const int choices_count = cur_seq_->GetChoicesCount();
                if (choices_count) {
                    if (choices_count == 1) {
                        state_ = eState::ChoicePuzzle;
                        MakeChoice("pass");
                    } else {
                        state_ = eState::ChoicePlaying;
                    }
                } else {
                    state_ = eState::Paused;
                }
                play_time_s_ = end_time_s;
            }

            cur_seq_->Update(play_time_s_, true);
        }
    } else if (state_ == eState::ChoicePlaying || state_ == eState::ChoicePaused) {
        const int choices_count = cur_seq_->GetChoicesCount();
        for (int i = 0; i < choices_count; i++) {
            const SeqChoice *ch = cur_seq_->GetChoice(i);
            push_choice_signal.FireN(ch->key.c_str(), ch->text.c_str());
        }

        const double end_time_s = cur_seq_->duration();
        play_time_s_ = cur_time_s - play_started_time_s_;

        const eState next_state =
            (state_ == eState::ChoicePlaying) ? eState::Sequence : eState::Paused;

        if (play_time_s_ < end_time_s) {
            // go back to playing
            state_ = next_state;
        } else {
            play_time_s_ = end_time_s;

            if (next_seq_id_ != -1) {
                //state_ = next_state;
                state_ = eState::ChoicePuzzle;
                /*SetCurSequence(next_seq_id_);
                switch_sequence_signal.FireN(next_seq_id_);
                SetPlayTime(cur_time_s, 0.0);
                next_seq_id_ = -1;*/
            }
        }
    } else if (state_ == eState::ChoicePuzzle) {

    } else if (state_ == eState::ChoiceTransition) {
        state_ = eState::Sequence;
        SetPlayTime(cur_time_s, 0.0);
    }
}

void DialogController::SetPlayTime(const double cur_time_s, const double play_time_s) {
    play_time_s_ = play_time_s;
    play_started_time_s_ = cur_time_s - play_time_s;
}

void DialogController::SetCurSequence(const int id) {
    assert(dialog_);
    if (cur_seq_) {
        cur_seq_->push_caption_signal.clear();
    }
    cur_seq_ = dialog_->GetSequence(id);
    cur_seq_->push_caption_signal
        .Connect<DialogController, &DialogController::OnPushCaption>(this);
}

void DialogController::MakeChoice(const char *key) {
    if (cur_seq_) {
        SeqChoice *ch = cur_seq_->GetChoice(key);
        next_seq_id_ = ch->seq_id;
        if (!ch->puzzle_name.empty()) {
            start_puzzle_signal.FireN(ch->puzzle_name.c_str());
        } else {
            ContinueChoice();
        }
    }
}

void DialogController::ContinueChoice() {
    if (next_seq_id_ != -1) {
        state_ = eState::ChoiceTransition;
        SetCurSequence(next_seq_id_);
        switch_sequence_signal.FireN(next_seq_id_);
        next_seq_id_ = -1;
    }
}

void DialogController::OnPushCaption(const char *text, const uint8_t color[4]) {
    push_caption_signal.FireN(text, color);
}