#include "test_common.h"

#include "test_scene.h"

extern std::string_view g_device_name;
extern int g_validation_level;
extern bool g_nohwrt, g_nosubgroup;

void test_volumetrics(Sys::ThreadPool &threads) {
    LogErr log;
    TestContext ren_ctx(512, 512, g_device_name, g_validation_level, g_nohwrt, g_nosubgroup, &log);

    // standard lights
    run_image_test(ren_ctx, threads, "vol_global0", 24.75, Full);
    run_image_test(ren_ctx, threads, "vol_global0", 24.60, Full_Ultra);
    run_image_test(ren_ctx, threads, "vol_global1", 25.15, Full);
    run_image_test(ren_ctx, threads, "vol_global1", 25.20, Full_Ultra);
    run_image_test(ren_ctx, threads, "vol_global2", 30.10, Full);
    run_image_test(ren_ctx, threads, "vol_global2", 30.15, Full_Ultra);
    run_image_test(ren_ctx, threads, "vol_global3", 31.95, Full);
    run_image_test(ren_ctx, threads, "vol_global3", 32.05, Full_Ultra);
    run_image_test(ren_ctx, threads, "vol_global4", 27.40, Full);
    run_image_test(ren_ctx, threads, "vol_global4", 27.55, Full_Ultra);

    // mesh lights
    run_image_test(ren_ctx, threads, "vol_global_mesh_lights", 27.70, Full);
    run_image_test(ren_ctx, threads, "vol_global_mesh_lights", 27.70, Full_Ultra);

    // sun
    run_image_test(ren_ctx, threads, "vol_global_sun", 30.95, Full);
    run_image_test(ren_ctx, threads, "vol_global_sun", 31.10, Full_Ultra);

    // absorption
    run_image_test(ren_ctx, threads, "vol_global_absorption", 22.30, Full);
    run_image_test(ren_ctx, threads, "vol_global_absorption", 22.30, Full_Ultra);

    // emission
    run_image_test(ren_ctx, threads, "vol_global_emission", 31.60, Full);
    run_image_test(ren_ctx, threads, "vol_global_emission", 31.55, Full_Ultra);

    // meshes
    run_image_test(ren_ctx, threads, "vol_local", 24.10, Full);
    run_image_test(ren_ctx, threads, "vol_local", 24.20, Full_Ultra);
    run_image_test(ren_ctx, threads, "vol_local_absorption", 24.40, Full);
    run_image_test(ren_ctx, threads, "vol_local_absorption", 24.60, Full_Ultra);
    run_image_test(ren_ctx, threads, "vol_local_emission", 27.80, Full);
    run_image_test(ren_ctx, threads, "vol_local_emission", 27.85, Full_Ultra);
}