#include "test_common.h"

#include "test_scene.h"

extern std::string_view g_device_name;
extern int g_validation_level;
extern bool g_nohwrt, g_nosubgroup;

void test_motion_blur(Sys::ThreadPool &threads) {
    LogErr log;
    TestContext ren_ctx(512, 512, g_device_name, g_validation_level, g_nohwrt, g_nosubgroup, &log);

    run_image_test(ren_ctx, threads, "motion_blur_dyn",
                   std::vector<double>{23.75, 24.55, 24.50, 24.45, 24.40, 24.20, 24.15, 24.00, 23.85, 23.75, //
                                       23.45, 23.30, 23.20, 22.60, 22.95, 22.50, 22.50, 22.95, 22.60, 22.70,
                                       23.40, 23.00, 23.60, 23.25, 23.25, 24.00, 23.70, 24.50, 24.75, 24.95,
                                       25.15, 25.20, 24.55},
                   MedDiffGI_Dynamic, 1.5f);
    run_image_test(ren_ctx, threads, "motion_blur_dyn",
                   std::vector<double>{22.65, 23.25, 23.30, 23.35, 23.35, 23.25, 23.30, 23.25, 23.20, 23.15, //
                                       22.95, 22.90, 22.85, 22.80, 22.25, 22.25, 22.75, 22.70, 22.80, 22.90,
                                       23.15, 23.20, 22.90, 23.40, 23.40, 23.65, 23.80, 24.10, 24.35, 24.50,
                                       24.65, 24.75, 24.15},
                   Full_Dynamic, 1.5f);
    run_image_test(ren_ctx, threads, "motion_blur_dyn",
                   std::vector<double>{23.60, 24.35, 24.40, 24.45, 24.45, 24.35, 24.40, 24.40, 24.40, 24.35, //
                                       24.25, 24.25, 24.30, 24.25, 24.15, 24.15, 24.00, 24.00, 23.90, 24.00,
                                       24.20, 24.30, 24.40, 24.40, 24.40, 24.65, 24.90, 25.20, 25.45, 25.55,
                                       25.65, 25.70, 25.10},
                   Full_Ultra_Dynamic);
}
