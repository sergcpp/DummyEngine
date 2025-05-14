#include "test_scene.h"

void test_volumetrics(Sys::ThreadPool &threads, const bool full) {
    // standart lights
    run_image_test(threads, "vol_global0", 24.95, Full);
    run_image_test(threads, "vol_global0", 24.80, Full_Ultra);
    run_image_test(threads, "vol_global1", 25.20, Full);
    run_image_test(threads, "vol_global1", 25.25, Full_Ultra);
    run_image_test(threads, "vol_global2", 30.50, Full);
    run_image_test(threads, "vol_global2", 30.60, Full_Ultra);
    run_image_test(threads, "vol_global3", 32.80, Full);
    run_image_test(threads, "vol_global3", 32.90, Full_Ultra);
    run_image_test(threads, "vol_global4", 27.65, Full);
    run_image_test(threads, "vol_global4", 27.80, Full_Ultra);

    // mesh lights
    run_image_test(threads, "vol_global_mesh_lights", 29.35, Full);
    run_image_test(threads, "vol_global_mesh_lights", 29.35, Full_Ultra);

    // sun
    run_image_test(threads, "vol_global_sun", 31.50, Full);
    run_image_test(threads, "vol_global_sun", 30.50, Full_Ultra);

    // absorption
    run_image_test(threads, "vol_global_absorption", 25.60, Full);
    run_image_test(threads, "vol_global_absorption", 25.80, Full_Ultra);

    // emission
    run_image_test(threads, "vol_global_emission", 26.60, Full);
    run_image_test(threads, "vol_global_emission", 26.70, Full_Ultra);

    // meshes
    run_image_test(threads, "vol_local", 26.10, Full);
    run_image_test(threads, "vol_local", 26.45, Full_Ultra);
    run_image_test(threads, "vol_local_absorption", 25.95, Full);
    run_image_test(threads, "vol_local_absorption", 26.50, Full_Ultra);
    run_image_test(threads, "vol_local_emission", 24.65, Full);
    run_image_test(threads, "vol_local_emission", 25.00, Full_Ultra);
}