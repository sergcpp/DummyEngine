#include "test_scene.h"

void test_volumetrics(Sys::ThreadPool &threads, const bool full) {
    { // standart lights
        run_image_test(threads, "vol_global0", 24.95, Full);
        run_image_test(threads, "vol_global0", 24.80, Full_Ultra);
        run_image_test(threads, "vol_global1", 25.40, Full);
        run_image_test(threads, "vol_global1", 25.40, Full_Ultra);
        run_image_test(threads, "vol_global2", 30.75, Full);
        run_image_test(threads, "vol_global2", 30.85, Full_Ultra);
        run_image_test(threads, "vol_global3", 32.80, Full);
        run_image_test(threads, "vol_global3", 32.90, Full_Ultra);
        run_image_test(threads, "vol_global4", 27.65, Full);
        run_image_test(threads, "vol_global4", 27.80, Full_Ultra);
    }
    { // mesh lights
        run_image_test(threads, "vol_global_mesh_lights", 30.25, Full);
        run_image_test(threads, "vol_global_mesh_lights", 30.25, Full_Ultra);
    }
    { // sun
        run_image_test(threads, "vol_global_sun", 31.85, Full);
        run_image_test(threads, "vol_global_sun", 31.05, Full_Ultra);
    }
}