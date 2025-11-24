#include "test_common.h"

#include "test_scene.h"

extern std::string_view g_device_name;
extern int g_validation_level;
extern bool g_nohwrt, g_nosubgroup;

void test_upscaling(Sys::ThreadPool &threads) {
    LogErr log;
    TestContext ren_ctx(512, 512, g_device_name, g_validation_level, g_nohwrt, g_nosubgroup, &log);

    run_image_test(ren_ctx, threads, "upscaling_dyn",
                   std::vector<double>{23.50, 23.35, 23.15, 23.10, 22.90, 22.90, 22.75, 22.70, 22.50, 22.40, //
                                       22.20, 22.10, 22.05, 22.0,  21.80, 21.80, 21.80, 21.90, 21.90, 22.10,
                                       22.00, 22.10, 22.15, 22.20, 22.15, 22.10, 22.35, 22.15, 22.50, 22.85,
                                       22.95, 23.00, 23.15},
                   MedDiffGI, 2.0f);
    run_image_test(ren_ctx, threads, "upscaling_dyn",
                   std::vector<double>{22.75, 22.70, 22.65, 22.65, 22.55, 22.55, 22.45, 22.45, 22.35, 22.25, //
                                       22.05, 22.10, 22.05, 21.95, 21.85, 21.90, 21.95, 22.05, 21.95, 22.05,
                                       22.20, 22.25, 22.25, 22.25, 22.15, 22.20, 22.25, 22.50, 22.60, 22.70,
                                       22.90, 22.95, 23.00},
                   Full, 1.5f);
    run_image_test(ren_ctx, threads, "upscaling_dyn",
                   std::vector<double>{23.75, 23.65, 23.50, 23.45, 23.25, 23.20, 23.05, 22.95, 22.85, 22.75, //
                                       22.55, 22.60, 22.55, 22.50, 22.35, 22.45, 22.35, 22.45, 22.25, 22.35,
                                       22.40, 22.55, 22.50, 22.45, 22.30, 22.35, 22.35, 22.60, 22.70, 22.85,
                                       23.05, 23.10, 23.15},
                   Full_Ultra, 1.5f);
}
