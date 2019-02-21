#pragma once

struct FrameInfo {
    unsigned int cur_time = 0, delta_time = 0, prev_time = 0, time_acc = 0;
    float time_fract = 0.0f;
};

