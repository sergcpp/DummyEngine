#include "test_common.h"

#include "test_scene.h"

extern std::string_view g_device_name;
extern int g_validation_level;
extern bool g_nohwrt, g_nosubgroup;

void test_motion_blur(Sys::ThreadPool &threads) {
    LogErr log;
    TestContext ren_ctx(512, 512, g_device_name, g_validation_level, g_nohwrt, g_nosubgroup, &log);

    run_image_test(ren_ctx, threads, "motion_blur_dyn",
                   std::vector<double>{23.50, 24.25, 24.20, 24.15, 24.00, 23.95, 23.75, 23.70, 23.50, 23.35, //
                                       23.10, 23.00, 22.90, 22.80, 22.65, 22.80, 22.65, 22.75, 22.80, 22.90,
                                       23.10, 23.25, 23.35, 23.55, 23.50, 23.75, 23.95, 24.25, 24.50, 24.75,
                                       24.80, 25.10, 24.40},
                   MedDiffGI_MotionBlur, 1.5f);
    run_image_test(ren_ctx, threads, "motion_blur_dyn",
                   std::vector<double>{22.45, 23.05, 23.05, 23.10, 23.05, 23.10, 23.00, 23.05, 22.90, 22.90, //
                                       22.70, 22.65, 22.60, 22.55, 22.40, 22.60, 22.45, 22.55, 22.60, 22.75,
                                       22.90, 23.10, 23.15, 23.35, 23.25, 23.50, 23.65, 23.95, 24.15, 24.40,
                                       24.50, 24.75, 24.10},
                   Full_MotionBlur, 1.5f);
    run_image_test(ren_ctx, threads, "motion_blur_dyn",
                   std::vector<double>{23.65, 24.35, 24.35, 24.35, 24.35, 24.35, 24.30, 24.30, 24.25, 24.25, //
                                       24.10, 24.10, 24.00, 23.95, 23.80, 23.85, 23.70, 23.75, 23.70, 23.80,
                                       24.00, 24.20, 24.20, 24.35, 24.30, 24.55, 24.80, 25.10, 25.30, 25.50,
                                       25.65, 25.80, 25.20},
                   Full_Ultra_MotionBlur);
}
