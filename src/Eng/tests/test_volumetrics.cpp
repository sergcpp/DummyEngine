#include "test_common.h"

#include "test_scene.h"

extern std::string_view g_device_name;
extern int g_validation_level;
extern bool g_nohwrt, g_nosubgroup;

void test_volumetrics(Sys::ThreadPool &threads) {
    LogErr log;
    TestContext ren_ctx(512, 512, g_device_name, g_validation_level, g_nohwrt, g_nosubgroup, &log);

    // standard lights
    run_image_test(ren_ctx, threads, "vol_global0", 25.15, Full);
    run_image_test(ren_ctx, threads, "vol_global0", 24.95, Full_Ultra);
    run_image_test(ren_ctx, threads, "vol_global1", 25.20, Full);
    run_image_test(ren_ctx, threads, "vol_global1", 25.20, Full_Ultra);
    run_image_test(ren_ctx, threads, "vol_global2", 30.35, Full);
    run_image_test(ren_ctx, threads, "vol_global2", 30.45, Full_Ultra);
    run_image_test(ren_ctx, threads, "vol_global3", 33.30, Full);
    run_image_test(ren_ctx, threads, "vol_global3", 33.45, Full_Ultra);
    run_image_test(ren_ctx, threads, "vol_global4", 28.30, Full);
    run_image_test(ren_ctx, threads, "vol_global4", 28.50, Full_Ultra);

    // mesh lights
    run_image_test(ren_ctx, threads, "vol_global_mesh_lights", 29.60, Full);
    run_image_test(ren_ctx, threads, "vol_global_mesh_lights", 29.60, Full_Ultra);

    // sun
    run_image_test(ren_ctx, threads, "vol_global_sun", 31.90, Full);
    run_image_test(ren_ctx, threads, "vol_global_sun", 31.90, Full_Ultra);

    // absorption
    run_image_test(ren_ctx, threads, "vol_global_absorption", 25.60, Full);
    run_image_test(ren_ctx, threads, "vol_global_absorption", 25.80, Full_Ultra);

    // emission
    run_image_test(ren_ctx, threads, "vol_global_emission", 26.05, Full);
    run_image_test(ren_ctx, threads, "vol_global_emission", 26.35, Full_Ultra);

    // meshes
    run_image_test(ren_ctx, threads, "vol_local", 25.65, Full);
    run_image_test(ren_ctx, threads, "vol_local", 25.95, Full_Ultra);
    run_image_test(ren_ctx, threads, "vol_local_absorption", 25.95, Full);
    run_image_test(ren_ctx, threads, "vol_local_absorption", 26.50, Full_Ultra);
    run_image_test(ren_ctx, threads, "vol_local_emission", 25.00, Full);
    run_image_test(ren_ctx, threads, "vol_local_emission", 25.45, Full_Ultra);
}