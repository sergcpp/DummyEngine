#include "DialogController.h"

#include <Eng/Utils/ScriptedDialog.h>
#include <Eng/Utils/ScriptedSequence.h>

DialogController::DialogController() {}

void DialogController::SetDialog(ScriptedDialog *dialog) {
    dialog_ = dialog;
    cur_seq_ = dialog_->GetSequence(0);
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

            cur_seq_->Update(play_time_s_);
        }
    } else if (state_ == eState::Sequence) {
        if (cur_seq_) {
            const double end_time_s = cur_seq_->duration();
            play_time_s_ = cur_time_s - play_started_time_s_;

            if (play_time_s_ >= end_time_s) {
                if (cur_seq_->GetChoicesCount()) {
                    state_ = eState::ChoicePlaying;
                } else {
                    state_ = eState::Paused;
                }
                play_time_s_ = end_time_s;
            }

            cur_seq_->Update(play_time_s_);
        }
    } else if (state_ == eState::ChoicePlaying || state_ == eState::ChoicePaused) {
        const int choices_count = cur_seq_->GetChoicesCount();
        for (int i = 0; i < choices_count; i++) {
            SeqChoice *ch = cur_seq_->GetChoice(i);
            push_choice_signal.FireN(ch->key.c_str(), ch->text.c_str());
        }

        const double end_time_s = cur_seq_->duration();
        play_time_s_ = cur_time_s - play_started_time_s_;

        if (play_time_s_ < end_time_s) {
            // go back to playing
            state_ = eState::Sequence;
        } else {
            play_time_s_ = end_time_s;

            if (next_seq_id_ != -1) {
                state_ =
                    (state_ == eState::ChoicePlaying) ? eState::Sequence : eState::Paused;
                SetCurSequence(next_seq_id_);
                switch_sequence_signal.FireN(next_seq_id_);
                SetPlayTime(cur_time_s, 0.0);
                next_seq_id_ = -1;
            }
        }
    }
}

double DialogController::GetPlayTime() { return play_time_s_; }

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
    }
}

void DialogController::OnPushCaption(const char *text, const uint8_t color[4]) {
    push_caption_signal.FireN(text, color);
}