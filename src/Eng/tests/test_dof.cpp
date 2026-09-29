#include "test_common.h"

#include "test_scene.h"

extern std::string_view g_device_name;
extern int g_validation_level;
extern bool g_nohwrt, g_nosubgroup;

void test_dof(Sys::ThreadPool &threads) {
    LogErr log;
    TestContext ren_ctx(512, 512, g_device_name, g_validation_level, g_nohwrt, g_nosubgroup, &log);

    run_image_test(ren_ctx, threads, "dof", 24.40, MedDiffGI);
    run_image_test(ren_ctx, threads, "dof", 24.40, Full);
    run_image_test(ren_ctx, threads, "dof", 26.35, Full_Ultra);

    run_image_test(ren_ctx, threads, "dof_dyn",
                   std::vector<double>{21.60, 22.10, 22.60, 23.00, 23.30, 23.45, 23.40, 23.30, 23.20, 23.20, //
                                       23.10, 23.00, 22.95, 22.95, 22.95, 23.00, 23.00, 23.20, 23.40, 23.45,
                                       23.50, 23.50, 23.55, 23.60, 23.65, 23.75, 23.75, 23.70, 23.70, 23.70,
                                       23.65, 23.65, 23.60},
                   MedDiffGI_Dynamic, 1.5f);
    run_image_test(ren_ctx, threads, "dof_dyn",
                   std::vector<double>{21.15, 21.65, 22.10, 22.50, 22.75, 22.90, 22.90, 22.80, 22.75, 22.75, //
                                       22.70, 22.60, 22.55, 22.60, 22.60, 22.65, 22.70, 22.85, 23.00, 23.05,
                                       23.10, 23.15, 23.15, 23.20, 23.25, 23.35, 23.35, 23.30, 23.30, 23.25,
                                       23.25, 23.20, 23.20},
                   Full_Dynamic, 1.5f);
    run_image_test(ren_ctx, threads, "dof_dyn",
                   std::vector<double>{21.70, 22.30, 22.85, 23.35, 23.70, 23.90, 24.00, 24.00, 24.25, 24.35, //
                                       24.40, 24.40, 24.35, 24.35, 24.40, 24.50, 24.60, 24.85, 24.95, 25.00,
                                       24.95, 24.85, 24.65, 24.50, 24.45, 24.40, 24.35, 24.30, 24.25, 24.20,
                                       24.15, 24.15, 24.10},
                   Full_Ultra_Dynamic);
}
