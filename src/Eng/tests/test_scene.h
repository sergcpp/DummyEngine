#pragma once

#include <string_view>
#include <variant>
#include <vector>

#include <Ren/utils/Span.h>

class TestContext;

namespace Sys {
class ThreadPool;
}

enum eImgTest {
    NoGI,
    NoDiffGI,
    NoDiffGI_RTShadow,
    MedDiffGI,
    MedDiffGI_Dynamic,
    MedDiffGI_RadCacheDbg,
    Full,
    Full_Dynamic,
    Full_Ultra,
    Full_Ultra_Dynamic
};

void run_image_test(TestContext &ren_ctx, Sys::ThreadPool &threads, std::string_view test_name,
                    const std::variant<Ren::Span<const double>, Ren::Span<const int>> &thresholds, eImgTest img_test,
                    float res_scale = 1.0f);

inline void run_image_test(TestContext &ren_ctx, Sys::ThreadPool &threads, std::string_view test_name,
                           const double min_psnr, const eImgTest img_test, const float res_scale = 1.0f) {
    run_image_test(ren_ctx, threads, test_name, Ren::Span<const double>{&min_psnr, 1}, img_test, res_scale);
}

inline void run_image_test(TestContext &ren_ctx, Sys::ThreadPool &threads, std::string_view test_name,
                           const std::vector<double> &min_psnr, const eImgTest img_test, const float res_scale = 1.0f) {
    run_image_test(ren_ctx, threads, test_name, Ren::Span<const double>{min_psnr}, img_test, res_scale);
}

inline void run_image_test(TestContext &ren_ctx, Sys::ThreadPool &threads, std::string_view test_name,
                           const std::vector<int> &fireflies_max, const eImgTest img_test,
                           const float res_scale = 1.0f) {
    run_image_test(ren_ctx, threads, test_name, Ren::Span<const int>{fireflies_max}, img_test, res_scale);
}