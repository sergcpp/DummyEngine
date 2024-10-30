#pragma once

namespace Eng {
struct FrameInfo {
    uint64_t cur_time_us = 0, delta_time_us = 0, prev_time_us = 0, time_acc_us = 0;
    double time_fract = 0.0;
};
} // namespace Eng
