#include "test_scene.h"

void test_volumetrics(Sys::ThreadPool &threads, const bool full) {
    { // standart lights
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
    }
    { // mesh lights
        run_image_test(threads, "vol_global_mesh_lights", 29.35, Full);
        run_image_test(threads, "vol_global_mesh_lights", 29.35, Full_Ultra);
    }
    { // sun
        run_image_test(threads, "vol_global_sun", 31.50, Full);
        run_image_test(threads, "vol_global_sun", 31.85, Full_Ultra);
    }
    { // absorption
        run_image_test(threads, "vol_global_absorption", 25.60, Full);
        run_image_test(threads, "vol_global_absorption", 25.80, Full_Ultra);
    }
    { // emission
        run_image_test(threads, "vol_global_emission", 26.65, Full);
        run_image_test(threads, "vol_global_emission", 26.75, Full_Ultra);
    }
}