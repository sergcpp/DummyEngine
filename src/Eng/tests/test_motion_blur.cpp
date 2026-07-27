#include "test_common.h"

#include "test_scene.h"

extern std::string_view g_device_name;
extern int g_validation_level;
extern bool g_nohwrt, g_nosubgroup;

void test_motion_blur(Sys::ThreadPool &threads) {
    LogErr log;
    TestContext ren_ctx(512, 512, g_device_name, g_validation_level, g_nohwrt, g_nosubgroup, &log);

    run_image_test(ren_ctx, threads, "motion_blur_dyn",
                   std::vector<double>{22.85, 23.50, 23.50, 23.50, 23.45, 23.35, 23.35, 23.20, 23.15, 23.10, //
                                       22.85, 22.80, 22.70, 22.60, 22.45, 22.50, 22.50, 22.50, 22.60, 22.70,
                                       22.85, 23.00, 23.10, 23.25, 23.25, 23.50, 23.70, 23.95, 24.20, 24.35,
                                       24.45, 24.55, 24.05},
                   MedDiffGI_MotionBlur, 1.5f);
    run_image_test(ren_ctx, threads, "motion_blur_dyn",
                   std::vector<double>{21.80, 22.30, 22.35, 22.45, 22.50, 22.45, 22.55, 22.50, 22.50, 22.55, //
                                       22.35, 22.35, 22.35, 22.30, 22.25, 22.25, 22.25, 22.25, 22.30, 22.40,
                                       22.65, 22.70, 22.90, 22.90, 22.95, 23.15, 23.25, 23.50, 23.70, 23.85,
                                       24.00, 24.05, 23.60},
                   Full_MotionBlur, 1.5f);
    run_image_test(ren_ctx, threads, "motion_blur_dyn",
                   std::vector<double>{22.60, 23.20, 23.25, 23.35, 23.40, 23.40, 23.45, 23.50, 23.55, 23.55, //
                                       23.50, 23.55, 23.60, 23.55, 23.45, 23.45, 23.35, 23.35, 23.30, 23.35,
                                       23.60, 23.65, 23.75, 23.75, 23.75, 23.95, 24.15, 24.40, 24.60, 24.70,
                                       24.80, 24.80, 24.35},
                   Full_Ultra_MotionBlur);
}
