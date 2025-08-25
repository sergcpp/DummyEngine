#include "test_common.h"

#include <chrono>
#include <future>
#include <istream>

#include "test_scene.h"

void test_shading(Sys::ThreadPool &threads, const bool full) {
    // complex materials
    run_image_test(threads, "visibility_flags", 25.40, Full);
    run_image_test(threads, "visibility_flags", 25.30, Full_Ultra);
    run_image_test(threads, "visibility_flags_sun", 25.15, Full);
    run_image_test(threads, "visibility_flags_sun", 26.50, Full_Ultra);
    run_image_test(threads, "two_sided_mat", 39.45, NoShadow);
    run_image_test(threads, "two_sided_mat", 29.90, NoGI);
    run_image_test(threads, "two_sided_mat", 29.55, NoDiffGI);
    run_image_test(threads, "two_sided_mat", 27.95, MedDiffGI);
    run_image_test(threads, "two_sided_mat", 27.30, Full);
    run_image_test(threads, "complex_mat0", 38.40, NoShadow);
    run_image_test(threads, "complex_mat0", 35.20, NoGI);
    run_image_test(threads, "complex_mat0", 29.15, NoDiffGI);
    run_image_test(threads, "complex_mat0", 27.20, MedDiffGI);
    run_image_test(threads, "complex_mat0", 26.05, Full);
    run_image_test(threads, "complex_mat0", 26.30, Full_Ultra);
    run_image_test(threads, "complex_mat1", 35.75, NoShadow);
    run_image_test(threads, "complex_mat1", 34.70, NoGI);
    run_image_test(threads, "complex_mat1", 31.10, NoDiffGI);
    run_image_test(threads, "complex_mat1", 29.74, MedDiffGI);
    run_image_test(threads, "complex_mat1", 28.40, Full);
    run_image_test(threads, "complex_mat1", 28.85, Full_Ultra);
    run_image_test(threads, "complex_mat2", 33.90, NoShadow);
    run_image_test(threads, "complex_mat2", 33.35, NoGI);
    run_image_test(threads, "complex_mat2", 27.25, NoDiffGI);
    run_image_test(threads, "complex_mat2", 25.05, MedDiffGI);
    run_image_test(threads, "complex_mat2", 24.75, Full);
    run_image_test(threads, "complex_mat2", 27.70, Full_Ultra);
    run_image_test(threads, "complex_mat2_dyn",
                   std::vector<double>{24.85, 24.80, 24.60, 24.35, 24.30, 24.15, 24.10, 24.05, 23.85, 23.65, //
                                       23.50, 23.45, 23.30, 23.15, 22.95, 22.95, 22.95, 23.15, 23.05, 23.15,
                                       23.15, 23.40, 23.40, 23.55, 23.50, 23.60, 23.85, 24.20, 24.50, 24.60,
                                       24.50, 24.45, 24.70},
                   MedDiffGI);
    run_image_test(threads, "complex_mat2_dyn",
                   std::vector<double>{23.40, 23.45, 23.40, 23.40, 23.35, 23.40, 23.35, 23.35, 23.25, 23.15, //
                                       23.10, 23.10, 23.00, 22.90, 22.70, 22.75, 22.80, 22.95, 22.85, 22.95,
                                       22.95, 23.20, 23.20, 23.35, 23.25, 23.35, 23.60, 23.90, 24.15, 24.25,
                                       24.15, 24.10, 24.35},
                   Full);
    run_image_test(threads, "complex_mat2_dyn",
                   std::vector<double>{24.65, 24.60, 24.50, 24.45, 24.25, 24.25, 24.10, 24.10, 23.95, 23.85, //
                                       23.80, 23.75, 23.70, 23.60, 23.40, 23.40, 23.40, 23.45, 23.25, 23.30,
                                       23.30, 23.55, 23.50, 23.65, 23.50, 23.60, 23.80, 24.15, 24.40, 24.50,
                                       24.40, 24.40, 24.65},
                   Full_Ultra);
    run_image_test(threads, "complex_mat2_far_away", 25.05, MedDiffGI);
    run_image_test(threads, "complex_mat2_far_away", 24.75, Full);
    run_image_test(threads, "complex_mat2_far_away", 27.85, Full_Ultra);
    run_image_test(threads, "complex_mat2_spot_light", 34.55, NoShadow);
    run_image_test(threads, "complex_mat2_spot_light", 35.85, NoGI);
    run_image_test(threads, "complex_mat2_spot_light", 30.95, NoDiffGI);
    run_image_test(threads, "complex_mat2_spot_light", 26.70, MedDiffGI);
    run_image_test(threads, "complex_mat2_spot_light", 26.45, Full);
    run_image_test(threads, "complex_mat2_spot_light", 27.80, Full_Ultra);
    run_image_test(threads, "complex_mat2_sun_light", 33.40, NoShadow);
    run_image_test(threads, "complex_mat2_sun_light", 28.90, NoGI);
    run_image_test(threads, "complex_mat2_sun_light", 33.40, NoGI_RTShadow);
    run_image_test(threads, "complex_mat2_sun_light", 21.10, Full);
    run_image_test(threads, "complex_mat2_sun_light", 22.80, Full_Ultra);
    run_image_test(threads, "complex_mat2_sun_light_dyn",
                   std::vector<double>{32.00, 32.45, 32.45, 32.30, 32.05, 31.80, 31.40, 31.00, 30.05, 29.35, //
                                       28.55, 27.55, 26.45, 25.25, 23.70, 21.95, 20.20, 21.25, 22.50, 23.50,
                                       24.00, 24.85, 25.20, 26.00, 26.25, 26.60, 27.25, 27.95, 28.50, 28.85,
                                       28.80, 29.55, 29.95},
                   Full);
    run_image_test(threads, "complex_mat2_sun_light_dyn",
                   std::vector<double>{32.70, 33.25, 33.35, 33.20, 32.90, 32.60, 32.15, 31.65, 30.75, 30.00, //
                                       29.05, 28.05, 26.90, 25.65, 24.25, 22.60, 20.80, 21.70, 22.80, 23.75,
                                       24.25, 25.15, 25.50, 26.25, 26.50, 26.85, 27.50, 28.20, 28.70, 29.05,
                                       29.00, 29.80, 30.15},
                   Full_Ultra);
    run_image_test(threads, "complex_mat2_moon_light", 22.65, MedDiffGI);
    run_image_test(threads, "complex_mat2_moon_light", 22.75, Full);
    run_image_test(threads, "complex_mat2_moon_light", 23.40, Full_Ultra);
    run_image_test(threads, "complex_mat2_hdri_light", 20.80, MedDiffGI);
    run_image_test(threads, "complex_mat2_hdri_light", 22.40, Full);
    run_image_test(threads, "complex_mat2_hdri_light", 23.85, Full_Ultra);
    run_image_test(threads, "complex_mat2_portal_hdri", 24.40, NoDiffGI);
    run_image_test(threads, "complex_mat2_portal_hdri", 24.45, MedDiffGI);
    run_image_test(threads, "complex_mat2_portal_hdri", 23.85, Full);
    run_image_test(threads, "complex_mat2_portal_hdri", 24.80, Full_Ultra);
    run_image_test(threads, "complex_mat2_portal_sky", 23.55, MedDiffGI);
    run_image_test(threads, "complex_mat2_portal_sky", 24.55, Full);
    run_image_test(threads, "complex_mat2_portal_sky", 25.75, Full_Ultra);
    run_image_test(threads, "complex_mat2_mesh_lights", 20.45, MedDiffGI);
    run_image_test(threads, "complex_mat2_mesh_lights", 20.70, Full);
    run_image_test(threads, "complex_mat2_mesh_lights", 21.55, Full_Ultra);
    run_image_test(threads, "complex_mat3", 24.25, NoShadow);
    run_image_test(threads, "complex_mat3", 20.85, NoGI);
    run_image_test(threads, "complex_mat3", 22.90, NoDiffGI);
    run_image_test(threads, "complex_mat3", 22.55, MedDiffGI);
    run_image_test(threads, "complex_mat3", 22.65, Full);
    run_image_test(threads, "complex_mat3", 23.15, Full_Ultra);
    run_image_test(threads, "complex_mat3_dyn",
                   std::vector<double>{22.55, 22.55, 22.60, 22.60, 22.60, 22.65, 22.65, 22.65, 22.65, 22.65, //
                                       22.70, 22.70, 22.70, 22.70, 22.70, 22.75, 22.75, 22.75, 22.75, 22.75,
                                       22.75, 22.80, 22.80, 22.80, 22.80, 22.80, 22.80, 22.80, 22.80, 22.80,
                                       22.80, 22.85, 22.85},
                   MedDiffGI);
    run_image_test(threads, "complex_mat3_dyn",
                   std::vector<double>{22.50, 22.50, 22.55, 22.55, 22.60, 22.60, 22.60, 22.65, 22.65, 22.65, //
                                       22.70, 22.70, 22.70, 22.70, 22.70, 22.75, 22.75, 22.80, 22.80, 22.80,
                                       22.80, 22.85, 22.85, 22.85, 22.85, 22.85, 22.85, 22.90, 22.90, 22.90,
                                       22.90, 22.90, 22.90},
                   Full);
    run_image_test(threads, "complex_mat3_dyn",
                   std::vector<double>{22.90, 22.95, 22.95, 23.00, 23.00, 23.00, 23.05, 23.05, 23.10, 23.10, //
                                       23.10, 23.15, 23.15, 23.15, 23.15, 23.20, 23.20, 23.25, 23.25, 23.25,
                                       23.25, 23.30, 23.30, 23.30, 23.30, 23.35, 23.35, 23.35, 23.35, 23.35,
                                       23.35, 23.40, 23.40},
                   Full_Ultra);
    run_image_test(threads, "complex_mat3_sun_light", 22.25, NoShadow);
    run_image_test(threads, "complex_mat3_sun_light", 17.10, NoGI);
    run_image_test(threads, "complex_mat3_sun_light", 22.70, NoGI_RTShadow);
    run_image_test(threads, "complex_mat3_sun_light", 18.95, Full);
    run_image_test(threads, "complex_mat3_sun_light", 23.65, Full_Ultra);
    run_image_test(threads, "complex_mat3_mesh_lights", 16.90, MedDiffGI);
    run_image_test(threads, "complex_mat3_mesh_lights", 20.35, Full);
    run_image_test(threads, "complex_mat3_mesh_lights", 20.40, Full_Ultra);
    run_image_test(threads, "complex_mat4", 20.25, Full);
    run_image_test(threads, "complex_mat4", 20.25, Full_Ultra);
    run_image_test(threads, "complex_mat4_sun_light", 20.05, Full);
    run_image_test(threads, "complex_mat4_sun_light", 19.90, Full_Ultra);
    run_image_test(threads, "emit_mat0", 24.95, Full);
    run_image_test(threads, "emit_mat0", 25.85, Full_Ultra);
    run_image_test(threads, "emit_mat1", 23.30, Full);
    run_image_test(threads, "emit_mat1", 23.85, Full_Ultra);

    if (!full) {
        return;
    }

    puts(" ---------------");
    // diffuse material
    run_image_test(threads, "diff_mat0", 47.05, NoShadow);
    run_image_test(threads, "diff_mat0", 36.25, NoGI);
    run_image_test(threads, "diff_mat0", 31.50, MedDiffGI);
    run_image_test(threads, "diff_mat0", 28.50, Full);
    run_image_test(threads, "diff_mat1", 46.55, NoShadow);
    run_image_test(threads, "diff_mat1", 35.65, NoGI);
    run_image_test(threads, "diff_mat1", 29.65, MedDiffGI);
    run_image_test(threads, "diff_mat1", 26.65, Full);
    run_image_test(threads, "diff_mat2", 45.30, NoShadow);
    run_image_test(threads, "diff_mat2", 35.95, NoGI);
    run_image_test(threads, "diff_mat2", 31.30, MedDiffGI);
    run_image_test(threads, "diff_mat2", 28.30, Full);
    run_image_test(threads, "diff_mat3", 46.45, NoShadow);
    run_image_test(threads, "diff_mat3", 36.30, NoGI);
    run_image_test(threads, "diff_mat3", 26.25, MedDiffGI);
    run_image_test(threads, "diff_mat3", 22.90, Full);
    run_image_test(threads, "diff_mat4", 46.75, NoShadow);
    run_image_test(threads, "diff_mat4", 35.70, NoGI);
    run_image_test(threads, "diff_mat4", 25.40, MedDiffGI);
    run_image_test(threads, "diff_mat4", 21.25, Full);
    run_image_test(threads, "diff_mat5", 44.30, NoShadow);
    run_image_test(threads, "diff_mat5", 36.10, NoGI);
    run_image_test(threads, "diff_mat5", 26.25, MedDiffGI);
    run_image_test(threads, "diff_mat5", 22.70, Full);

    puts(" ---------------");
    // sheen material
    /*run_image_test(threads, "sheen_mat0", 46.55, NoShadow);
    run_image_test(threads, "sheen_mat0", 36.95, NoGI);
    run_image_test(threads, "sheen_mat0", 30.35, MedDiffGI);
    run_image_test(threads, "sheen_mat0", 28.85, Full);
    run_image_test(threads, "sheen_mat1", 43.85, NoShadow);
    run_image_test(threads, "sheen_mat1", 36.55, NoGI);
    run_image_test(threads, "sheen_mat1", 27.35, MedDiffGI);
    run_image_test(threads, "sheen_mat1", 26.60, Full);
    run_image_test(threads, "sheen_mat2", 45.30, NoShadow);
    run_image_test(threads, "sheen_mat2", 36.55, NoGI);
    run_image_test(threads, "sheen_mat2", 29.55, MedDiffGI);
    run_image_test(threads, "sheen_mat2", 27.65, Full);
    run_image_test(threads, "sheen_mat3", 43.90, NoShadow);
    run_image_test(threads, "sheen_mat3", 35.95, NoGI);
    run_image_test(threads, "sheen_mat3", 28.35, MedDiffGI);
    run_image_test(threads, "sheen_mat3", 26.40, Full);
    run_image_test(threads, "sheen_mat4", 45.20, NoShadow);
    run_image_test(threads, "sheen_mat4", 37.10, NoGI);
    run_image_test(threads, "sheen_mat4", 25.25, MedDiffGI);
    run_image_test(threads, "sheen_mat4", 21.00, Full);
    run_image_test(threads, "sheen_mat5", 42.30, NoShadow);
    run_image_test(threads, "sheen_mat5", 36.05, NoGI);
    run_image_test(threads, "sheen_mat5", 23.35, MedDiffGI);
    run_image_test(threads, "sheen_mat5", 20.15, Full);
    run_image_test(threads, "sheen_mat6", 44.40, NoShadow);
    run_image_test(threads, "sheen_mat6", 36.75, NoGI);
    run_image_test(threads, "sheen_mat6", 25.00, MedDiffGI);
    run_image_test(threads, "sheen_mat6", 19.90, Full);
    run_image_test(threads, "sheen_mat7", 42.70, NoShadow);
    run_image_test(threads, "sheen_mat7", 36.35, NoGI);
    run_image_test(threads, "sheen_mat7", 24.70, MedDiffGI);
    run_image_test(threads, "sheen_mat7", 19.35, Full);*/

    puts(" ---------------");
    // specular material
    run_image_test(threads, "spec_mat0", 40.10, NoShadow);
    run_image_test(threads, "spec_mat0", 35.80, NoGI);
    run_image_test(threads, "spec_mat0", 27.10, NoDiffGI);
    run_image_test(threads, "spec_mat0", 24.15, MedDiffGI);
    run_image_test(threads, "spec_mat0", 24.00, Full);
    run_image_test(threads, "spec_mat0", 27.50, Full_Ultra);
    run_image_test(threads, "spec_mat1", 19.60, NoShadow);
    run_image_test(threads, "spec_mat1", 19.40, NoGI);
    run_image_test(threads, "spec_mat1", 22.00, NoDiffGI);
    run_image_test(threads, "spec_mat1", 18.15, MedDiffGI);
    run_image_test(threads, "spec_mat1", 18.05, Full);
    run_image_test(threads, "spec_mat1", 18.80, Full_Ultra);
    run_image_test(threads, "spec_mat2", 44.75, NoShadow);
    run_image_test(threads, "spec_mat2", 35.30, NoGI);
    run_image_test(threads, "spec_mat2", 28.85, NoDiffGI);
    run_image_test(threads, "spec_mat2", 26.20, MedDiffGI);
    run_image_test(threads, "spec_mat2", 25.70, Full);
    run_image_test(threads, "spec_mat2", 25.45, Full_Ultra);
    run_image_test(threads, "spec_mat3", 36.40, NoShadow);
    run_image_test(threads, "spec_mat3", 34.00, NoGI);
    run_image_test(threads, "spec_mat3", 29.00, NoDiffGI);
    run_image_test(threads, "spec_mat3", 21.35, MedDiffGI);
    run_image_test(threads, "spec_mat3", 19.30, Full);
    run_image_test(threads, "spec_mat3", 21.80, Full_Ultra);
    run_image_test(threads, "spec_mat4", 21.00, NoShadow);
    run_image_test(threads, "spec_mat4", 21.10, NoGI);
    run_image_test(threads, "spec_mat4", 21.95, NoDiffGI);
    run_image_test(threads, "spec_mat4", 15.10, MedDiffGI);
    run_image_test(threads, "spec_mat4", 14.25, Full);
    run_image_test(threads, "spec_mat4", 14.35, Full_Ultra);
    run_image_test(threads, "spec_mat5", 32.15, NoShadow);
    run_image_test(threads, "spec_mat5", 30.30, NoGI);
    run_image_test(threads, "spec_mat5", 29.10, NoDiffGI);
    run_image_test(threads, "spec_mat5", 19.80, MedDiffGI);
    run_image_test(threads, "spec_mat5", 18.25, Full);
    run_image_test(threads, "spec_mat5", 18.35, Full_Ultra);

    puts(" ---------------");
    // metal material
    run_image_test(threads, "metal_mat0", 32.65, NoShadow);
    run_image_test(threads, "metal_mat0", 31.60, NoGI);
    run_image_test(threads, "metal_mat0", 29.45, NoDiffGI);
    run_image_test(threads, "metal_mat0", 26.50, MedDiffGI);
    run_image_test(threads, "metal_mat0", 26.15, Full);
    run_image_test(threads, "metal_mat0", 28.75, Full_Ultra);
    run_image_test(threads, "metal_mat1", 23.60, NoShadow);
    run_image_test(threads, "metal_mat1", 23.70, NoGI);
    run_image_test(threads, "metal_mat1", 27.05, NoDiffGI);
    run_image_test(threads, "metal_mat1", 24.85, MedDiffGI);
    run_image_test(threads, "metal_mat1", 24.40, Full);
    run_image_test(threads, "metal_mat1", 24.95, Full_Ultra);
    run_image_test(threads, "metal_mat2", 37.00, NoShadow);
    run_image_test(threads, "metal_mat2", 34.30, NoGI);
    run_image_test(threads, "metal_mat2", 33.55, NoDiffGI);
    run_image_test(threads, "metal_mat2", 31.15, MedDiffGI);
    run_image_test(threads, "metal_mat2", 29.55, Full);
    run_image_test(threads, "metal_mat2", 30.15, Full_Ultra);
    run_image_test(threads, "metal_mat3", 38.05, NoShadow);
    run_image_test(threads, "metal_mat3", 34.85, NoGI);
    run_image_test(threads, "metal_mat3", 30.05, NoDiffGI);
    run_image_test(threads, "metal_mat3", 22.85, MedDiffGI);
    run_image_test(threads, "metal_mat3", 20.05, Full);
    run_image_test(threads, "metal_mat3", 21.55, Full_Ultra);
    run_image_test(threads, "metal_mat4", 25.25, NoShadow);
    run_image_test(threads, "metal_mat4", 25.20, NoGI);
    run_image_test(threads, "metal_mat4", 26.45, NoDiffGI);
    run_image_test(threads, "metal_mat4", 19.70, MedDiffGI);
    run_image_test(threads, "metal_mat4", 17.95, Full);
    run_image_test(threads, "metal_mat4", 17.95, Full_Ultra);
    run_image_test(threads, "metal_mat5", 37.30, NoShadow);
    run_image_test(threads, "metal_mat5", 34.35, NoGI);
    run_image_test(threads, "metal_mat5", 33.00, NoDiffGI);
    run_image_test(threads, "metal_mat5", 24.50, MedDiffGI);
    run_image_test(threads, "metal_mat5", 20.70, Full);
    run_image_test(threads, "metal_mat5", 20.90, Full_Ultra);

    puts(" ---------------");
    // plastic material
    run_image_test(threads, "plastic_mat0", 43.60, NoShadow);
    run_image_test(threads, "plastic_mat0", 35.90, NoGI);
    run_image_test(threads, "plastic_mat0", 32.95, NoDiffGI);
    run_image_test(threads, "plastic_mat0", 28.80, MedDiffGI);
    run_image_test(threads, "plastic_mat0", 27.00, Full);
    run_image_test(threads, "plastic_mat0", 28.20, Full_Ultra);
    run_image_test(threads, "plastic_mat1", 36.20, NoShadow);
    run_image_test(threads, "plastic_mat1", 33.70, NoGI);
    run_image_test(threads, "plastic_mat1", 28.00, NoDiffGI);
    run_image_test(threads, "plastic_mat1", 24.30, MedDiffGI);
    run_image_test(threads, "plastic_mat1", 23.35, Full);
    run_image_test(threads, "plastic_mat1", 23.65, Full_Ultra);
    run_image_test(threads, "plastic_mat2", 41.75, NoShadow);
    run_image_test(threads, "plastic_mat2", 34.90, NoGI);
    run_image_test(threads, "plastic_mat2", 33.40, NoDiffGI);
    run_image_test(threads, "plastic_mat2", 29.25, MedDiffGI);
    run_image_test(threads, "plastic_mat2", 26.55, Full);
    run_image_test(threads, "plastic_mat2", 27.80, Full_Ultra);
    run_image_test(threads, "plastic_mat3", 38.45, NoShadow);
    run_image_test(threads, "plastic_mat3", 34.35, NoGI);
    run_image_test(threads, "plastic_mat3", 32.00, NoDiffGI);
    run_image_test(threads, "plastic_mat3", 24.60, MedDiffGI);
    run_image_test(threads, "plastic_mat3", 21.45, Full);
    run_image_test(threads, "plastic_mat3", 22.70, Full_Ultra);
    run_image_test(threads, "plastic_mat4", 35.70, NoShadow);
    run_image_test(threads, "plastic_mat4", 33.35, NoGI);
    run_image_test(threads, "plastic_mat4", 28.30, NoDiffGI);
    run_image_test(threads, "plastic_mat4", 21.90, MedDiffGI);
    run_image_test(threads, "plastic_mat4", 19.85, Full);
    run_image_test(threads, "plastic_mat4", 20.40, Full_Ultra);
    run_image_test(threads, "plastic_mat5", 41.85, NoShadow);
    run_image_test(threads, "plastic_mat5", 35.05, NoGI);
    run_image_test(threads, "plastic_mat5", 30.40, NoDiffGI);
    run_image_test(threads, "plastic_mat5", 25.40, MedDiffGI);
    run_image_test(threads, "plastic_mat5", 21.10, Full);
    run_image_test(threads, "plastic_mat5", 22.45, Full_Ultra);

    puts(" ---------------");
    // tint material
    run_image_test(threads, "tint_mat0", 43.20, NoShadow);
    run_image_test(threads, "tint_mat0", 35.85, NoGI);
    run_image_test(threads, "tint_mat0", 34.40, NoDiffGI);
    run_image_test(threads, "tint_mat0", 28.10, MedDiffGI);
    run_image_test(threads, "tint_mat0", 26.20, Full);
    run_image_test(threads, "tint_mat1", 32.50, NoShadow);
    run_image_test(threads, "tint_mat1", 31.35, NoGI);
    run_image_test(threads, "tint_mat1", 26.40, NoDiffGI);
    run_image_test(threads, "tint_mat1", 22.75, MedDiffGI);
    run_image_test(threads, "tint_mat1", 21.70, Full);
    run_image_test(threads, "tint_mat2", 40.45, NoShadow);
    run_image_test(threads, "tint_mat2", 34.35, NoGI);
    run_image_test(threads, "tint_mat2", 33.40, NoDiffGI);
    run_image_test(threads, "tint_mat2", 28.95, MedDiffGI);
    run_image_test(threads, "tint_mat2", 26.50, Full);
    run_image_test(threads, "tint_mat3", 43.65, NoShadow);
    run_image_test(threads, "tint_mat3", 35.95, NoGI);
    run_image_test(threads, "tint_mat3", 33.75, NoDiffGI);
    run_image_test(threads, "tint_mat3", 24.90, MedDiffGI);
    run_image_test(threads, "tint_mat3", 19.30, Full);
    run_image_test(threads, "tint_mat4", 30.95, NoShadow);
    run_image_test(threads, "tint_mat4", 30.30, NoGI);
    run_image_test(threads, "tint_mat4", 25.60, NoDiffGI);
    run_image_test(threads, "tint_mat4", 21.45, MedDiffGI);
    run_image_test(threads, "tint_mat4", 17.05, Full);
    run_image_test(threads, "tint_mat5", 41.45, NoShadow);
    run_image_test(threads, "tint_mat5", 34.70, NoGI);
    run_image_test(threads, "tint_mat5", 31.20, NoDiffGI);
    run_image_test(threads, "tint_mat5", 23.80, MedDiffGI);
    run_image_test(threads, "tint_mat5", 17.65, Full);

    puts(" ---------------");
    // clearcoat material
    run_image_test(threads, "coat_mat0", 41.05, NoShadow);
    run_image_test(threads, "coat_mat0", 35.95, NoGI);
    run_image_test(threads, "coat_mat1", 32.05, NoShadow);
    run_image_test(threads, "coat_mat1", 31.25, NoGI);
    run_image_test(threads, "coat_mat2", 28.90, NoShadow);
    run_image_test(threads, "coat_mat2", 28.50, NoGI);
    run_image_test(threads, "coat_mat3", 38.75, NoShadow);
    run_image_test(threads, "coat_mat3", 35.10, NoGI);
    run_image_test(threads, "coat_mat4", 30.85, NoShadow);
    run_image_test(threads, "coat_mat4", 30.55, NoGI);
    run_image_test(threads, "coat_mat5", 28.00, NoShadow);
    run_image_test(threads, "coat_mat5", 27.80, NoGI);

    puts(" ---------------");
    // alpha material
    run_image_test(threads, "alpha_mat0", 37.15, NoShadow);
    run_image_test(threads, "alpha_mat0", 29.75, NoGI);
    run_image_test(threads, "alpha_mat0", 25.55, NoDiffGI);
    run_image_test(threads, "alpha_mat0", 24.75, MedDiffGI);
    run_image_test(threads, "alpha_mat0", 24.80, Full);
    run_image_test(threads, "alpha_mat0", 24.00, Full_Ultra);
    run_image_test(threads, "alpha_mat1", 38.40, NoShadow);
    run_image_test(threads, "alpha_mat1", 30.95, NoGI);
    run_image_test(threads, "alpha_mat1", 28.00, NoDiffGI);
    run_image_test(threads, "alpha_mat1", 26.95, MedDiffGI);
    run_image_test(threads, "alpha_mat1", 27.00, Full);
    run_image_test(threads, "alpha_mat1", 26.45, Full_Ultra);
    run_image_test(threads, "alpha_mat2", 41.30, NoShadow);
    run_image_test(threads, "alpha_mat2", 35.25, NoGI);
    run_image_test(threads, "alpha_mat2", 33.70, NoDiffGI);
    run_image_test(threads, "alpha_mat2", 32.00, MedDiffGI);
    run_image_test(threads, "alpha_mat2", 30.90, Full);
    run_image_test(threads, "alpha_mat2", 31.30, Full_Ultra);
    run_image_test(threads, "alpha_mat3", 46.85, NoShadow);
    run_image_test(threads, "alpha_mat3", 37.15, NoGI);
    run_image_test(threads, "alpha_mat3", 37.55, NoDiffGI);
    run_image_test(threads, "alpha_mat3", 32.25, MedDiffGI);
    run_image_test(threads, "alpha_mat3", 29.00, Full);
    run_image_test(threads, "alpha_mat3", 30.00, Full_Ultra);
}
