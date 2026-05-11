#include "test_common.h"

#include "test_scene.h"

extern std::string_view g_device_name;
extern int g_validation_level;
extern bool g_nohwrt, g_nosubgroup;

void test_motion_blur(Sys::ThreadPool &threads) {
    LogErr log;
    TestContext ren_ctx(512, 512, g_device_name, g_validation_level, g_nohwrt, g_nosubgroup, &log);

    run_image_test(ren_ctx, threads, "motion_blur_dyn",
                   std::vector<double>{22.90, 23.60, 23.55, 23.55, 23.45, 23.45, 23.35, 23.35, 23.15, 23.10, //
                                       22.90, 22.80, 22.70, 22.60, 22.45, 22.60, 22.50, 22.60, 22.60, 22.75,
                                       22.85, 23.05, 23.10, 23.30, 23.25, 23.50, 23.70, 23.95, 24.20, 24.40,
                                       24.45, 24.70, 24.05},
                   MedDiffGI_MotionBlur, 1.5f);
    run_image_test(ren_ctx, threads, "motion_blur_dyn",
                   std::vector<double>{22.35, 22.95, 23.00, 23.00, 22.95, 23.00, 22.95, 23.00, 22.85, 22.85, //
                                       22.70, 22.65, 22.55, 22.50, 22.40, 22.55, 22.40, 22.50, 22.55, 22.65,
                                       22.80, 23.00, 23.05, 23.25, 23.20, 23.40, 23.60, 23.90, 24.10, 24.30,
                                       24.40, 24.65, 24.00},
                   Full_MotionBlur, 1.5f);
    run_image_test(ren_ctx, threads, "motion_blur_dyn",
                   std::vector<double>{23.65, 24.35, 24.35, 24.35, 24.30, 24.35, 24.25, 24.30, 24.25, 24.20, //
                                       24.10, 24.05, 24.00, 23.95, 23.80, 23.85, 23.65, 23.70, 23.60, 23.65,
                                       23.85, 24.05, 24.10, 24.20, 24.20, 24.40, 24.70, 25.00, 25.20, 25.40,
                                       25.50, 25.70, 25.05},
                   Full_Ultra_MotionBlur);
}
