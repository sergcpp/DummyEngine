#include "test_scene.h"

void test_volumetrics(Sys::ThreadPool &threads, const bool full) {
    { // standart lights
        run_image_test(threads, "fog_global0", 24.95, Full);
        run_image_test(threads, "fog_global0", 24.80, Full_Ultra);
        run_image_test(threads, "fog_global1", 25.40, Full);
        run_image_test(threads, "fog_global1", 25.40, Full_Ultra);
        run_image_test(threads, "fog_global2", 30.75, Full);
        run_image_test(threads, "fog_global2", 30.85, Full_Ultra);
        run_image_test(threads, "fog_global3", 32.80, Full);
        run_image_test(threads, "fog_global3", 32.90, Full_Ultra);
        run_image_test(threads, "fog_global4", 27.65, Full);
        run_image_test(threads, "fog_global4", 27.80, Full_Ultra);
    }
    { // mesh lights
        run_image_test(threads, "fog_global_mesh_lights", 30.25, Full);
        run_image_test(threads, "fog_global_mesh_lights", 30.25, Full_Ultra);
    }
}