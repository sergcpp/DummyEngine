#include "test_common.h"

#include "test_scene.h"

extern std::string_view g_device_name;
extern int g_validation_level;
extern bool g_nohwrt, g_nosubgroup;

void test_upscaling(Sys::ThreadPool &threads) {
    LogErr log;
    TestContext ren_ctx(512, 512, g_device_name, g_validation_level, g_nohwrt, g_nosubgroup, &log);

    run_image_test(ren_ctx, threads, "upscaling_dyn",
                   std::vector<double>{23.55, 23.50, 23.30, 23.25, 22.85, 22.85, 22.65, 22.30, 22.30, 22.10, //
                                       21.90, 21.80, 21.70, 21.65, 21.40, 21.55, 21.40, 21.55, 21.55, 21.70,
                                       21.60, 21.75, 21.70, 21.75, 21.65, 21.75, 21.80, 22.00, 22.15, 22.25,
                                       22.30, 22.45, 22.60},
                   MedDiffGI, 2.0f);
    run_image_test(ren_ctx, threads, "upscaling_dyn",
                   std::vector<double>{22.55, 22.55, 22.40, 22.40, 22.30, 22.25, 22.15, 22.10, 22.05, 22.00, //
                                       21.85, 21.80, 21.75, 21.70, 21.55, 21.70, 21.65, 21.80, 21.80, 21.85,
                                       21.95, 22.05, 22.00, 22.00, 21.85, 21.95, 22.00, 22.15, 22.30, 22.35,
                                       22.50, 22.60, 22.65},
                   Full, 1.5f);
    run_image_test(ren_ctx, threads, "upscaling_dyn",
                   std::vector<double>{23.35, 23.35, 23.10, 23.00, 22.85, 22.70, 22.55, 22.50, 22.40, 22.35, //
                                       22.25, 22.20, 22.15, 22.10, 21.95, 22.10, 22.00, 22.15, 22.05, 22.10,
                                       22.15, 22.25, 22.25, 22.20, 22.00, 22.10, 22.15, 22.25, 22.40, 22.50,
                                       22.60, 22.75, 22.80},
                   Full_Ultra, 1.5f);
    ///
    run_image_test(ren_ctx, threads, "upscaling_dyn_exposure",
                   std::vector<double>{38.95, 39.15, 38.35, 37.55, 36.60, 35.45, 34.40, 33.35, 31.95, 30.55, //
                                       28.95, 27.35, 25.80, 24.20, 22.45, 20.80, 19.15, 20.10, 21.20, 21.70,
                                       22.50, 23.50, 23.55, 24.00, 25.15, 25.20, 26.00, 26.45, 26.80, 27.00,
                                       27.00, 27.30, 26.95},
                   MedDiffGI, 2.0f);
    run_image_test(ren_ctx, threads, "upscaling_dyn_exposure",
                   std::vector<double>{38.85, 39.00, 38.25, 37.50, 36.55, 35.45, 34.40, 33.40, 32.00, 30.75, //
                                       29.25, 27.80, 26.35, 24.85, 23.20, 21.55, 19.70, 20.85, 21.55, 22.70,
                                       23.20, 23.65, 23.90, 24.40, 24.70, 25.40, 25.75, 26.80, 26.80, 26.85,
                                       26.95, 27.05, 26.70},
                   Full, 1.5f);
    run_image_test(ren_ctx, threads, "upscaling_dyn_exposure",
                   std::vector<double>{39.05, 39.20, 38.50, 37.75, 36.85, 35.70, 34.70, 33.70, 32.30, 31.05, //
                                       29.55, 28.05, 26.60, 25.05, 23.40, 21.75, 19.85, 20.95, 21.90, 22.70,
                                       23.20, 23.70, 23.95, 24.45, 24.75, 25.45, 25.80, 26.80, 26.80, 26.85,
                                       26.95, 27.10, 26.70},
                   Full_Ultra, 1.5f);
}
