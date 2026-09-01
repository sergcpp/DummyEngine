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
                   std::vector<double>{38.95, 38.60, 37.80, 37.05, 36.10, 35.05, 34.10, 33.00, 31.60, 30.30, //
                                       28.75, 27.15, 25.65, 24.05, 22.45, 20.80, 19.15, 20.10, 21.20, 22.15,
                                       22.50, 23.20, 23.55, 24.95, 24.80, 25.20, 26.55, 26.45, 26.80, 28.05,
                                       27.55, 27.30, 27.45},
                   MedDiffGI, 2.0f);
    run_image_test(ren_ctx, threads, "upscaling_dyn_exposure",
                   std::vector<double>{38.80, 38.45, 37.65, 36.85, 35.95, 34.95, 34.05, 33.00, 31.60, 30.40, //
                                       28.90, 27.45, 26.00, 24.45, 22.85, 21.15, 19.55, 20.65, 21.80, 22.50,
                                       23.05, 23.45, 23.85, 24.35, 24.70, 25.35, 26.20, 26.80, 27.30, 27.50,
                                       26.95, 27.05, 27.30},
                   Full, 1.5f);
    run_image_test(ren_ctx, threads, "upscaling_dyn_exposure",
                   std::vector<double>{39.05, 38.75, 37.90, 37.10, 36.20, 35.20, 34.35, 33.25, 31.85, 30.65, //
                                       29.15, 27.65, 26.15, 24.60, 23.05, 21.35, 19.65, 20.50, 21.75, 22.45,
                                       23.05, 23.50, 23.95, 24.45, 24.75, 25.45, 25.80, 26.80, 27.30, 27.55,
                                       26.95, 27.10, 27.30},
                   Full_Ultra, 1.5f);
}
