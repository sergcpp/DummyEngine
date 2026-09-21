#include "test_common.h"

#include "test_scene.h"

extern std::string_view g_device_name;
extern int g_validation_level;
extern bool g_nohwrt, g_nosubgroup;

void test_upscaling(Sys::ThreadPool &threads) {
    LogErr log;
    TestContext ren_ctx(512, 512, g_device_name, g_validation_level, g_nohwrt, g_nosubgroup, &log);

    run_image_test(ren_ctx, threads, "upscaling_dyn",
                   std::vector<double>{24.10, 24.00, 23.85, 23.60, 23.50, 23.15, 23.05, 22.75, 22.60, 22.50, //
                                       21.75, 21.75, 21.65, 21.55, 21.40, 21.40, 21.40, 21.45, 21.50, 21.50,
                                       21.55, 21.55, 21.60, 21.50, 21.60, 21.65, 21.75, 21.90, 21.90, 22.15,
                                       22.15, 22.10, 22.40},
                   MedDiffGI, 2.0f);
    run_image_test(ren_ctx, threads, "upscaling_dyn",
                   std::vector<double>{22.85, 22.80, 22.75, 22.60, 22.55, 22.40, 22.40, 22.30, 22.30, 21.80, //
                                       21.60, 21.60, 21.55, 21.55, 21.45, 21.45, 21.55, 21.55, 21.60, 21.65,
                                       21.80, 21.80, 21.80, 21.75, 21.65, 21.80, 21.80, 21.90, 22.00, 22.05,
                                       22.20, 22.20, 22.25},
                   Full, 1.5f);
    run_image_test(ren_ctx, threads, "upscaling_dyn",
                   std::vector<double>{23.35, 23.30, 23.20, 23.00, 22.90, 22.70, 22.65, 22.55, 22.50, 22.55, //
                                       21.90, 21.90, 21.90, 21.85, 21.75, 21.75, 21.80, 21.80, 21.75, 21.80,
                                       21.95, 21.95, 21.95, 21.85, 21.75, 21.90, 21.85, 21.95, 22.05, 22.10,
                                       22.30, 22.25, 22.75},
                   Full_Ultra, 1.5f);
    ///
    run_image_test(ren_ctx, threads, "upscaling_dyn_exposure",
                   std::vector<double>{40.25, 39.45, 38.65, 37.65, 36.10, 35.05, 34.10, 33.00, 31.60, 30.30, //
                                       28.65, 27.05, 25.55, 23.95, 22.35, 20.70, 19.10, 20.10, 21.20, 22.10,
                                       22.50, 23.20, 23.55, 24.95, 24.80, 25.75, 26.55, 27.20, 27.85, 29.55,
                                       30.60, 31.75, 32.95},
                   MedDiffGI, 2.0f);
    run_image_test(ren_ctx, threads, "upscaling_dyn_exposure",
                   std::vector<double>{40.70, 39.85, 39.00, 38.00, 36.80, 35.60, 34.05, 33.00, 31.60, 30.40, //
                                       28.90, 27.40, 25.90, 24.40, 22.75, 21.05, 19.45, 20.55, 21.75, 22.50,
                                       23.05, 23.45, 23.85, 24.75, 25.30, 25.95, 26.65, 27.55, 28.20, 29.40,
                                       30.45, 32.10, 32.85},
                   Full, 1.5f);
    run_image_test(ren_ctx, threads, "upscaling_dyn_exposure",
                   std::vector<double>{41.15, 40.35, 39.35, 38.35, 37.15, 35.95, 34.35, 33.25, 31.85, 30.65, //
                                       29.15, 27.60, 26.05, 24.50, 22.90, 21.20, 19.50, 20.50, 21.70, 22.45,
                                       23.05, 23.50, 23.95, 24.45, 25.35, 26.00, 26.70, 27.60, 28.25, 29.45,
                                       30.50, 32.15, 32.90},
                   Full_Ultra, 1.5f);
}
