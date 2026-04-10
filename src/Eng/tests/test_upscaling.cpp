#include "test_common.h"

#include "test_scene.h"

extern std::string_view g_device_name;
extern int g_validation_level;
extern bool g_nohwrt, g_nosubgroup;

void test_upscaling(Sys::ThreadPool &threads) {
    LogErr log;
    TestContext ren_ctx(512, 512, g_device_name, g_validation_level, g_nohwrt, g_nosubgroup, &log);

    run_image_test(ren_ctx, threads, "upscaling_dyn",
                   std::vector<double>{23.70, 23.65, 23.40, 23.35, 22.90, 22.90, 22.70, 22.55, 22.35, 22.15, //
                                       21.95, 21.80, 21.70, 21.65, 21.45, 21.55, 21.45, 21.60, 21.60, 21.70,
                                       21.60, 21.75, 21.70, 21.75, 21.65, 21.75, 21.80, 22.00, 22.15, 22.25,
                                       22.30, 22.45, 22.60},
                   MedDiffGI, 2.0f);
    run_image_test(ren_ctx, threads, "upscaling_dyn",
                   std::vector<double>{22.60, 22.60, 22.50, 22.45, 22.35, 22.30, 22.15, 22.20, 22.10, 22.00, //
                                       21.85, 21.80, 21.75, 21.70, 21.60, 21.75, 21.70, 21.85, 21.80, 21.90,
                                       22.00, 22.05, 22.05, 22.00, 21.85, 21.95, 22.00, 22.15, 22.30, 22.35,
                                       22.50, 22.60, 22.65},
                   Full, 1.5f);
    run_image_test(ren_ctx, threads, "upscaling_dyn",
                   std::vector<double>{23.35, 23.35, 23.10, 23.05, 22.90, 22.75, 22.60, 22.55, 22.50, 22.45, //
                                       22.30, 22.25, 22.20, 22.15, 22.00, 22.15, 22.05, 22.15, 22.05, 22.10,
                                       22.15, 22.25, 22.25, 22.20, 22.00, 22.10, 22.15, 22.25, 22.40, 22.50,
                                       22.60, 22.75, 22.80},
                   Full_Ultra, 1.5f);
}
