#pragma once

#include <string_view>

#include <Ren/Span.h>

namespace Sys {
class ThreadPool;
}

enum eImgTest { NoShadow, NoGI, NoGI_RTShadow, NoDiffGI, NoDiffGI_RTShadow, MedDiffGI, Full, Full_Ultra };

void run_image_test(Sys::ThreadPool &threads, std::string_view test_name, Ren::Span<const double> min_psnr,
                    eImgTest img_test, float res_scale = 1.0f);

inline void run_image_test(Sys::ThreadPool &threads, std::string_view test_name, const double min_psnr,
                           const eImgTest img_test, const float res_scale = 1.0f) {
    run_image_test(threads, test_name, {&min_psnr, 1}, img_test, res_scale);
}