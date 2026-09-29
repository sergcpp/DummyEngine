#include "test_common.h"

#include "test_scene.h"

extern std::string_view g_device_name;
extern int g_validation_level;
extern bool g_nohwrt, g_nosubgroup;

void test_upscaling(Sys::ThreadPool &threads) {
    LogErr log;
    TestContext ren_ctx(512, 512, g_device_name, g_validation_level, g_nohwrt, g_nosubgroup, &log);

    run_image_test(ren_ctx, threads, "upscaling_dyn",
                   std::vector<double>{23.05, 23.00, 22.95, 22.80, 22.70, 22.45, 22.40, 22.20, 22.10, 22.00, //
                                       21.75, 21.75, 21.65, 21.55, 21.40, 21.40, 21.40, 21.45, 21.50, 21.50,
                                       21.55, 21.55, 21.60, 21.50, 21.60, 21.65, 21.75, 21.90, 21.90, 22.15,
                                       22.15, 22.10, 22.40},
                   MedDiffGI, 2.0f);
    run_image_test(ren_ctx, threads, "upscaling_dyn",
                   std::vector<double>{21.95, 21.95, 21.95, 21.85, 21.85, 21.75, 21.80, 21.75, 21.75, 21.80, //
                                       21.60, 21.60, 21.55, 21.55, 21.45, 21.45, 21.55, 21.55, 21.60, 21.65,
                                       21.80, 21.80, 21.80, 21.75, 21.65, 21.80, 21.80, 21.90, 22.00, 22.05,
                                       22.20, 22.20, 22.25},
                   Full, 1.5f);
    run_image_test(ren_ctx, threads, "upscaling_dyn",
                   std::vector<double>{22.35, 22.35, 22.30, 22.20, 22.15, 22.00, 22.00, 21.95, 21.95, 22.05, //
                                       21.90, 21.90, 21.90, 21.85, 21.75, 21.75, 21.80, 21.80, 21.75, 21.80,
                                       21.95, 21.95, 21.95, 21.85, 21.75, 21.90, 21.85, 21.95, 22.05, 22.10,
                                       22.30, 22.25, 22.25},
                   Full_Ultra, 1.5f);
    ///
    run_image_test(ren_ctx, threads, "upscaling_dyn_exposure",
                   std::vector<double>{40.35, 39.55, 38.65, 37.70, 36.10, 35.05, 34.10, 33.00, 31.60, 30.30, //
                                       28.75, 27.15, 25.60, 24.00, 22.40, 20.75, 19.15, 20.10, 21.20, 22.15,
                                       22.50, 23.20, 23.55, 24.95, 24.80, 25.90, 26.55, 27.40, 28.10, 29.80,
                                       30.90, 32.10, 33.30},
                   MedDiffGI, 2.0f);
    run_image_test(ren_ctx, threads, "upscaling_dyn_exposure",
                   std::vector<double>{40.75, 39.90, 39.00, 38.00, 36.80, 35.65, 34.05, 33.00, 31.60, 30.40, //
                                       28.90, 27.45, 25.95, 24.45, 22.80, 21.10, 19.50, 20.60, 21.80, 22.50,
                                       23.05, 23.45, 23.85, 24.85, 25.35, 26.00, 26.80, 27.75, 28.35, 29.55,
                                       30.65, 32.40, 33.05},
                   Full, 1.5f);
    run_image_test(ren_ctx, threads, "upscaling_dyn_exposure",
                   std::vector<double>{41.20, 40.40, 39.35, 38.35, 37.15, 36.00, 34.35, 33.25, 31.85, 30.65, //
                                       29.15, 27.65, 26.15, 24.55, 22.95, 21.30, 19.55, 20.50, 21.75, 22.45,
                                       23.05, 23.50, 23.95, 24.45, 25.40, 26.05, 26.80, 27.75, 28.40, 29.60,
                                       30.70, 32.40, 33.10},
                   Full_Ultra, 1.5f);
}
