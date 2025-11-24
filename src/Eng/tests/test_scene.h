#pragma once

#include <string_view>

#include <Ren/Span.h>

class TestContext;

namespace Sys {
class ThreadPool;
}

enum eImgTest {
    NoShadow,
    NoGI,
    NoGI_RTShadow,
    NoDiffGI,
    NoDiffGI_RTShadow,
    MedDiffGI,
    MedDiffGI_MotionBlur,
    Full,
    Full_MotionBlur,
    Full_Ultra,
    Full_Ultra_MotionBlur
};

void run_image_test(TestContext &ren_ctx, Sys::ThreadPool &threads, std::string_view test_name,
                    Ren::Span<const double> min_psnr, eImgTest img_test, float res_scale = 1.0f);

inline void run_image_test(TestContext &ren_ctx, Sys::ThreadPool &threads, std::string_view test_name,
                           const double min_psnr, const eImgTest img_test, const float res_scale = 1.0f) {
    run_image_test(ren_ctx, threads, test_name, {&min_psnr, 1}, img_test, res_scale);
}