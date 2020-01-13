#include "FlowControl.h"

#include <Sys/Log.h>

#pragma warning(disable : 4996)

FlowControl::FlowControl(unsigned int bad_delta, unsigned int good_delta) : bad_delta_(bad_delta), good_delta_(good_delta) {
    Reset();
}

void FlowControl::Reset() {
    mode_ = Bad;
    penalty_time_ = 4.0f;
    good_conditions_time_ = 0.0f;
    penalty_reduction_acc_ = 0.0f;
}

void FlowControl::Update(float dt_s, float rtt, Ren::ILog *log) {
    const float RTT_threshold = 250.0f;
    if (mode_ == Good) {
        if (rtt > RTT_threshold) {
            log->Info("[FlowControl]: dropping to bad mode");
            mode_ = Bad;
            if (good_conditions_time_ < 10.0f && penalty_time_ < 60.0f) {
                penalty_time_ *= 2.0f;
                if (penalty_time_ > 60.0f) {
                    penalty_time_ = 60.0f;
                }
                log->Info("[FlowControl]: penalty time increased to %f", penalty_time_);
            }
            good_conditions_time_ = 0.0f;
            penalty_reduction_acc_ = 0.0f;
        } else {
            good_conditions_time_ += dt_s;
            penalty_reduction_acc_ += dt_s;

            if (penalty_reduction_acc_ > 10.0f && penalty_time_ > 1.0f) {
                penalty_time_ /= 2.0f;
                if (penalty_time_ < 1.0f) {
                    penalty_time_ = 1.0f;
                }
                log->Info("[FlowControl]: penalty time reduced to %f", penalty_time_);
                penalty_reduction_acc_ = 0;
            }
        }
    } else if (mode_ == Bad) {
        if (rtt < RTT_threshold) {
            good_conditions_time_ += dt_s;
        } else {
            good_conditions_time_ = 0.0f;
        }

        if (good_conditions_time_ > penalty_time_) {
            log->Info("[FlowControl]: upgrading to good mode");
            good_conditions_time_ = 0.0f;
            penalty_reduction_acc_ = 0.0f;
            mode_ = Good;
        }
    }
}