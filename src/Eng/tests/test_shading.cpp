#include "test_common.h"

#include "test_scene.h"

extern std::string_view g_device_name;
extern int g_validation_level;
extern bool g_nohwrt, g_nosubgroup;

void test_shading(Sys::ThreadPool &threads, const bool full) {
    LogErr log;
    TestContext ren_ctx(512, 512, g_device_name, g_validation_level, g_nohwrt, g_nosubgroup, &log);

    // complex materials
    run_image_test(ren_ctx, threads, "visibility_flags", 24.50, Full);
    run_image_test(ren_ctx, threads, "visibility_flags", 24.85, Full_Ultra);
    run_image_test(ren_ctx, threads, "visibility_flags_sun", 22.70, Full);
    run_image_test(ren_ctx, threads, "visibility_flags_sun", 24.30, Full_Ultra);
    run_image_test(ren_ctx, threads, "two_sided_mat", 28.10, MedDiffGI);
    run_image_test(ren_ctx, threads, "complex_mat0", 26.90, MedDiffGI);
    run_image_test(ren_ctx, threads, "complex_mat0", 27.00, Full);
    run_image_test(ren_ctx, threads, "complex_mat0", 27.00, Full_Ultra);
    run_image_test(ren_ctx, threads, "complex_mat1", 28.35, MedDiffGI);
    run_image_test(ren_ctx, threads, "complex_mat1", 29.55, Full);
    run_image_test(ren_ctx, threads, "complex_mat1", 29.75, Full_Ultra);
    run_image_test(ren_ctx, threads, "complex_mat2", 24.40, MedDiffGI);
    run_image_test(ren_ctx, threads, "complex_mat2", 24.35, Full);
    run_image_test(ren_ctx, threads, "complex_mat2", 26.95, Full_Ultra);
    run_image_test(ren_ctx, threads, "complex_mat2_area_spread", 16.40, NoDiffGI);
    run_image_test(ren_ctx, threads, "complex_mat2_area_spread", 17.75, MedDiffGI);
    run_image_test(ren_ctx, threads, "complex_mat2_area_spread", 18.70, Full);
    run_image_test(ren_ctx, threads, "complex_mat2_area_spread", 18.60, Full_Ultra);
    run_image_test(ren_ctx, threads, "complex_mat2_emissive", 18.10, Full);
    run_image_test(ren_ctx, threads, "complex_mat2_emissive", 17.00, Full_Ultra);
    run_image_test(ren_ctx, threads, "complex_mat2_dyn",
                   std::vector<double>{25.10, 25.05, 24.95, 24.80, 24.65, 24.50, 24.40, 24.30, 24.15, 23.95, //
                                       23.75, 23.60, 23.50, 22.90, 22.80, 22.85, 22.95, 23.00, 23.00, 23.05,
                                       23.70, 23.30, 23.80, 23.35, 23.30, 24.10, 24.20, 24.40, 24.65, 24.70,
                                       24.75, 24.75, 25.00},
                   MedDiffGI);
    run_image_test(ren_ctx, threads, "complex_mat2_dyn",
                   std::vector<double>{23.55, 23.60, 23.55, 23.55, 23.50, 23.50, 23.50, 23.50, 23.45, 23.40, //
                                       23.25, 23.20, 23.15, 23.10, 22.55, 23.05, 23.20, 23.25, 23.20, 23.25,
                                       23.45, 23.05, 23.05, 23.50, 23.45, 23.75, 23.85, 24.00, 24.20, 24.25,
                                       24.30, 24.30, 24.45},
                   Full);
    run_image_test(ren_ctx, threads, "complex_mat2_dyn",
                   std::vector<double>{24.25, 24.30, 24.25, 24.15, 24.10, 24.00, 24.00, 23.95, 23.95, 23.90, //
                                       23.80, 23.75, 23.80, 23.70, 23.60, 23.65, 23.70, 23.70, 23.55, 23.55,
                                       23.75, 23.80, 23.80, 23.80, 23.70, 23.95, 24.05, 24.20, 24.40, 24.40,
                                       24.45, 24.45, 24.65},
                   Full_Ultra);
    run_image_test(ren_ctx, threads, "complex_mat2_far_away", 24.25, MedDiffGI);
    run_image_test(ren_ctx, threads, "complex_mat2_far_away", 24.60, Full);
    run_image_test(ren_ctx, threads, "complex_mat2_far_away", 26.75, Full_Ultra);
    run_image_test(ren_ctx, threads, "complex_mat2_spot_light", 26.15, MedDiffGI);
    run_image_test(ren_ctx, threads, "complex_mat2_spot_light", 25.45, Full);
    run_image_test(ren_ctx, threads, "complex_mat2_spot_light", 26.05, Full_Ultra);
    run_image_test(ren_ctx, threads, "complex_mat2_sun_light", 22.45, Full);
    run_image_test(ren_ctx, threads, "complex_mat2_sun_light", 23.70, Full_Ultra);
    run_image_test(ren_ctx, threads, "complex_mat2_sun_light_dyn",
                   std::vector<double>{35.90, 36.10, 36.00, 35.65, 35.10, 34.40, 33.55, 32.75, 31.65, 30.55, //
                                       28.85, 27.60, 26.30, 25.05, 23.60, 21.95, 20.35, 21.80, 23.35, 24.35,
                                       25.20, 25.95, 25.60, 26.95, 26.55, 28.20, 28.20, 28.50, 29.65, 29.85,
                                       30.10, 30.70, 31.20},
                   Full);
    run_image_test(ren_ctx, threads, "complex_mat2_sun_light_dyn",
                   std::vector<double>{36.45, 37.30, 36.75, 36.40, 35.80, 35.15, 34.30, 33.40, 32.35, 31.20, //
                                       29.25, 28.00, 26.75, 25.50, 24.05, 22.55, 20.80, 22.05, 23.55, 24.25,
                                       25.10, 26.10, 26.00, 27.10, 26.70, 28.65, 28.35, 28.65, 29.85, 30.00,
                                       30.25, 30.85, 31.30},
                   Full_Ultra);
    run_image_test(ren_ctx, threads, "complex_mat2_moon_light", 28.75, MedDiffGI);
    run_image_test(ren_ctx, threads, "complex_mat2_moon_light", 29.15, Full);
    run_image_test(ren_ctx, threads, "complex_mat2_moon_light", 30.45, Full_Ultra);
    run_image_test(ren_ctx, threads, "complex_mat2_hdri_light", 21.25, MedDiffGI);
    run_image_test(ren_ctx, threads, "complex_mat2_hdri_light", 22.90, Full);
    run_image_test(ren_ctx, threads, "complex_mat2_hdri_light", 24.30, Full_Ultra);
    run_image_test(ren_ctx, threads, "complex_mat2_portal_hdri", 25.65, MedDiffGI);
    run_image_test(ren_ctx, threads, "complex_mat2_portal_hdri", 24.90, Full);
    run_image_test(ren_ctx, threads, "complex_mat2_portal_hdri", 25.75, Full_Ultra);
    run_image_test(ren_ctx, threads, "complex_mat2_portal_sky", 21.75, MedDiffGI);
    run_image_test(ren_ctx, threads, "complex_mat2_portal_sky", 23.65, Full);
    run_image_test(ren_ctx, threads, "complex_mat2_portal_sky", 23.95, Full_Ultra);
    run_image_test(ren_ctx, threads, "complex_mat2_mesh_lights", 20.20, MedDiffGI);
    run_image_test(ren_ctx, threads, "complex_mat2_mesh_lights", 21.00, Full);
    run_image_test(ren_ctx, threads, "complex_mat2_mesh_lights", 21.80, Full_Ultra);
    run_image_test(ren_ctx, threads, "complex_mat3", 22.15, MedDiffGI);
    run_image_test(ren_ctx, threads, "complex_mat3", 23.15, Full);
    run_image_test(ren_ctx, threads, "complex_mat3", 23.50, Full_Ultra);
    run_image_test(ren_ctx, threads, "complex_mat3_dyn",
                   std::vector<double>{22.25, 22.25, 22.25, 22.25, 22.30, 22.30, 22.30, 22.30, 22.30, 22.30, //
                                       22.30, 22.30, 22.35, 22.35, 22.30, 22.30, 22.30, 22.30, 22.30, 22.30,
                                       22.30, 22.35, 22.35, 22.35, 22.35, 22.35, 22.35, 22.35, 22.35, 22.35,
                                       22.35, 22.35, 22.35},
                   MedDiffGI);
    run_image_test(ren_ctx, threads, "complex_mat3_dyn",
                   std::vector<double>{22.55, 22.55, 22.60, 22.60, 22.60, 22.65, 22.65, 22.65, 22.65, 22.70, //
                                       22.70, 22.70, 22.75, 22.75, 22.75, 22.75, 22.75, 22.75, 22.75, 22.75,
                                       22.75, 22.75, 22.75, 22.80, 22.75, 22.80, 22.80, 22.80, 22.85, 22.80,
                                       22.80, 22.80, 22.85},
                   Full);
    run_image_test(ren_ctx, threads, "complex_mat3_dyn",
                   std::vector<double>{22.85, 22.90, 22.90, 22.90, 22.95, 22.95, 22.95, 23.00, 23.00, 23.00, //
                                       23.00, 23.00, 23.05, 23.05, 23.05, 23.05, 23.05, 23.05, 23.05, 23.05,
                                       23.05, 23.05, 23.10, 23.10, 23.10, 23.10, 23.10, 23.10, 23.15, 23.15,
                                       23.15, 23.15, 23.15},
                   Full_Ultra);
    run_image_test(ren_ctx, threads, "complex_mat3_sun_light", 20.40, Full);
    run_image_test(ren_ctx, threads, "complex_mat3_sun_light", 26.90, Full_Ultra);
    run_image_test(ren_ctx, threads, "complex_mat3_mesh_lights", 17.70, MedDiffGI);
    run_image_test(ren_ctx, threads, "complex_mat3_mesh_lights", 20.45, Full);
    run_image_test(ren_ctx, threads, "complex_mat3_mesh_lights", 20.35, Full_Ultra);
    run_image_test(ren_ctx, threads, "complex_mat4", 20.10, Full);
    run_image_test(ren_ctx, threads, "complex_mat4", 20.60, Full_Ultra);
    run_image_test(ren_ctx, threads, "complex_mat4_sun_light", 19.85, Full);
    run_image_test(ren_ctx, threads, "complex_mat4_sun_light", 19.55, Full_Ultra);
    run_image_test(ren_ctx, threads, "emit_mat0", 24.60, Full);
    run_image_test(ren_ctx, threads, "emit_mat0", 23.60, Full_Ultra);
    run_image_test(ren_ctx, threads, "emit_mat1", 23.00, Full);
    run_image_test(ren_ctx, threads, "emit_mat1", 21.55, Full_Ultra);

    if (!full) {
        return;
    }

    puts(" ---------------");
    run_image_test(ren_ctx, threads, "complex_mat0", 28.10, NoGI);
    run_image_test(ren_ctx, threads, "complex_mat0", 26.65, NoDiffGI);
    run_image_test(ren_ctx, threads, "complex_mat1", 28.05, NoGI);
    run_image_test(ren_ctx, threads, "complex_mat1", 27.00, NoDiffGI);
    run_image_test(ren_ctx, threads, "complex_mat2", 27.60, NoGI);
    run_image_test(ren_ctx, threads, "complex_mat2", 24.80, NoDiffGI);
    run_image_test(ren_ctx, threads, "complex_mat2_spot_light", 29.80, NoGI);
    run_image_test(ren_ctx, threads, "complex_mat2_spot_light", 28.20, NoDiffGI);
    run_image_test(ren_ctx, threads, "complex_mat2_sun_light", 28.20, NoGI);
    run_image_test(ren_ctx, threads, "complex_mat2_portal_hdri", 24.40, NoDiffGI);
    run_image_test(ren_ctx, threads, "complex_mat3", 20.55, NoGI);
    run_image_test(ren_ctx, threads, "complex_mat3", 22.20, NoDiffGI);
    run_image_test(ren_ctx, threads, "complex_mat3_sun_light", 17.10, NoGI);

    puts(" ---------------");
    // diffuse material
    run_image_test(ren_ctx, threads, "diff_mat0", 29.45, NoGI);
    run_image_test(ren_ctx, threads, "diff_mat0", 31.40, MedDiffGI);
    run_image_test(ren_ctx, threads, "diff_mat0", 29.80, Full);
    run_image_test(ren_ctx, threads, "diff_mat1", 29.05, NoGI);
    run_image_test(ren_ctx, threads, "diff_mat1", 29.80, MedDiffGI);
    run_image_test(ren_ctx, threads, "diff_mat1", 27.70, Full);
    run_image_test(ren_ctx, threads, "diff_mat2", 29.05, NoGI);
    run_image_test(ren_ctx, threads, "diff_mat2", 30.45, MedDiffGI);
    run_image_test(ren_ctx, threads, "diff_mat2", 29.40, Full);
    run_image_test(ren_ctx, threads, "diff_mat3", 29.80, NoGI);
    run_image_test(ren_ctx, threads, "diff_mat3", 26.55, MedDiffGI);
    run_image_test(ren_ctx, threads, "diff_mat3", 21.15, Full);
    run_image_test(ren_ctx, threads, "diff_mat4", 29.50, NoGI);
    run_image_test(ren_ctx, threads, "diff_mat4", 25.40, MedDiffGI);
    run_image_test(ren_ctx, threads, "diff_mat4", 19.35, Full);
    run_image_test(ren_ctx, threads, "diff_mat5", 29.50, NoGI);
    run_image_test(ren_ctx, threads, "diff_mat5", 26.20, MedDiffGI);
    run_image_test(ren_ctx, threads, "diff_mat5", 20.90, Full);

    puts(" ---------------");
    // sheen material
    /*run_image_test(ren_ctx, threads, "sheen_mat0", 46.55, NoShadow);
    run_image_test(ren_ctx, threads, "sheen_mat0", 36.95, NoGI);
    run_image_test(ren_ctx, threads, "sheen_mat0", 30.35, MedDiffGI);
    run_image_test(ren_ctx, threads, "sheen_mat0", 28.85, Full);
    run_image_test(ren_ctx, threads, "sheen_mat1", 43.85, NoShadow);
    run_image_test(ren_ctx, threads, "sheen_mat1", 36.55, NoGI);
    run_image_test(ren_ctx, threads, "sheen_mat1", 27.35, MedDiffGI);
    run_image_test(ren_ctx, threads, "sheen_mat1", 26.60, Full);
    run_image_test(ren_ctx, threads, "sheen_mat2", 45.30, NoShadow);
    run_image_test(ren_ctx, threads, "sheen_mat2", 36.55, NoGI);
    run_image_test(ren_ctx, threads, "sheen_mat2", 29.55, MedDiffGI);
    run_image_test(ren_ctx, threads, "sheen_mat2", 27.65, Full);
    run_image_test(ren_ctx, threads, "sheen_mat3", 43.90, NoShadow);
    run_image_test(ren_ctx, threads, "sheen_mat3", 35.95, NoGI);
    run_image_test(ren_ctx, threads, "sheen_mat3", 28.35, MedDiffGI);
    run_image_test(ren_ctx, threads, "sheen_mat3", 26.40, Full);
    run_image_test(ren_ctx, threads, "sheen_mat4", 45.20, NoShadow);
    run_image_test(ren_ctx, threads, "sheen_mat4", 37.10, NoGI);
    run_image_test(ren_ctx, threads, "sheen_mat4", 25.25, MedDiffGI);
    run_image_test(ren_ctx, threads, "sheen_mat4", 21.00, Full);
    run_image_test(ren_ctx, threads, "sheen_mat5", 42.30, NoShadow);
    run_image_test(ren_ctx, threads, "sheen_mat5", 36.05, NoGI);
    run_image_test(ren_ctx, threads, "sheen_mat5", 23.35, MedDiffGI);
    run_image_test(ren_ctx, threads, "sheen_mat5", 20.15, Full);
    run_image_test(ren_ctx, threads, "sheen_mat6", 44.40, NoShadow);
    run_image_test(ren_ctx, threads, "sheen_mat6", 36.75, NoGI);
    run_image_test(ren_ctx, threads, "sheen_mat6", 25.00, MedDiffGI);
    run_image_test(ren_ctx, threads, "sheen_mat6", 19.90, Full);
    run_image_test(ren_ctx, threads, "sheen_mat7", 42.70, NoShadow);
    run_image_test(ren_ctx, threads, "sheen_mat7", 36.35, NoGI);
    run_image_test(ren_ctx, threads, "sheen_mat7", 24.70, MedDiffGI);
    run_image_test(ren_ctx, threads, "sheen_mat7", 19.35, Full);*/

    puts(" ---------------");
    // specular material
    run_image_test(ren_ctx, threads, "spec_mat0", 28.05, NoGI);
    run_image_test(ren_ctx, threads, "spec_mat0", 24.90, NoDiffGI);
    run_image_test(ren_ctx, threads, "spec_mat0", 23.65, MedDiffGI);
    run_image_test(ren_ctx, threads, "spec_mat0", 23.75, Full);
    run_image_test(ren_ctx, threads, "spec_mat0", 26.05, Full_Ultra);
    run_image_test(ren_ctx, threads, "spec_mat1", 19.30, NoGI);
    run_image_test(ren_ctx, threads, "spec_mat1", 21.70, NoDiffGI);
    run_image_test(ren_ctx, threads, "spec_mat1", 18.00, MedDiffGI);
    run_image_test(ren_ctx, threads, "spec_mat1", 17.85, Full);
    run_image_test(ren_ctx, threads, "spec_mat1", 18.60, Full_Ultra);
    run_image_test(ren_ctx, threads, "spec_mat2", 28.10, NoGI);
    run_image_test(ren_ctx, threads, "spec_mat2", 26.20, NoDiffGI);
    run_image_test(ren_ctx, threads, "spec_mat2", 25.35, MedDiffGI);
    run_image_test(ren_ctx, threads, "spec_mat2", 24.90, Full);
    run_image_test(ren_ctx, threads, "spec_mat2", 24.75, Full_Ultra);
    run_image_test(ren_ctx, threads, "spec_mat3", 27.55, NoGI);
    run_image_test(ren_ctx, threads, "spec_mat3", 25.65, NoDiffGI);
    run_image_test(ren_ctx, threads, "spec_mat3", 21.05, MedDiffGI);
    run_image_test(ren_ctx, threads, "spec_mat3", 18.50, Full);
    run_image_test(ren_ctx, threads, "spec_mat3", 19.85, Full_Ultra);
    run_image_test(ren_ctx, threads, "spec_mat4", 20.65, NoGI);
    run_image_test(ren_ctx, threads, "spec_mat4", 21.55, NoDiffGI);
    run_image_test(ren_ctx, threads, "spec_mat4", 14.70, MedDiffGI);
    run_image_test(ren_ctx, threads, "spec_mat4", 13.75, Full);
    run_image_test(ren_ctx, threads, "spec_mat4", 13.85, Full_Ultra);
    run_image_test(ren_ctx, threads, "spec_mat5", 26.50, NoGI);
    run_image_test(ren_ctx, threads, "spec_mat5", 26.35, NoDiffGI);
    run_image_test(ren_ctx, threads, "spec_mat5", 19.25, MedDiffGI);
    run_image_test(ren_ctx, threads, "spec_mat5", 17.35, Full);
    run_image_test(ren_ctx, threads, "spec_mat5", 17.35, Full_Ultra);

    puts(" ---------------");
    // metal material
    run_image_test(ren_ctx, threads, "metal_mat0", 27.05, NoGI);
    run_image_test(ren_ctx, threads, "metal_mat0", 25.85, NoDiffGI);
    run_image_test(ren_ctx, threads, "metal_mat0", 25.50, MedDiffGI);
    run_image_test(ren_ctx, threads, "metal_mat0", 25.75, Full);
    run_image_test(ren_ctx, threads, "metal_mat0", 27.55, Full_Ultra);
    run_image_test(ren_ctx, threads, "metal_mat1", 22.60, NoGI);
    run_image_test(ren_ctx, threads, "metal_mat1", 25.10, NoDiffGI);
    run_image_test(ren_ctx, threads, "metal_mat1", 24.20, MedDiffGI);
    run_image_test(ren_ctx, threads, "metal_mat1", 23.75, Full);
    run_image_test(ren_ctx, threads, "metal_mat1", 24.05, Full_Ultra);
    run_image_test(ren_ctx, threads, "metal_mat2", 27.95, NoGI);
    run_image_test(ren_ctx, threads, "metal_mat2", 27.75, NoDiffGI);
    run_image_test(ren_ctx, threads, "metal_mat2", 29.15, MedDiffGI);
    run_image_test(ren_ctx, threads, "metal_mat2", 28.25, Full);
    run_image_test(ren_ctx, threads, "metal_mat2", 28.20, Full_Ultra);
    run_image_test(ren_ctx, threads, "metal_mat3", 27.75, NoGI);
    run_image_test(ren_ctx, threads, "metal_mat3", 25.95, NoDiffGI);
    run_image_test(ren_ctx, threads, "metal_mat3", 22.80, MedDiffGI);
    run_image_test(ren_ctx, threads, "metal_mat3", 19.35, Full);
    run_image_test(ren_ctx, threads, "metal_mat3", 20.15, Full_Ultra);
    run_image_test(ren_ctx, threads, "metal_mat4", 23.85, NoGI);
    run_image_test(ren_ctx, threads, "metal_mat4", 24.55, NoDiffGI);
    run_image_test(ren_ctx, threads, "metal_mat4", 19.30, MedDiffGI);
    run_image_test(ren_ctx, threads, "metal_mat4", 17.15, Full);
    run_image_test(ren_ctx, threads, "metal_mat4", 17.15, Full_Ultra);
    run_image_test(ren_ctx, threads, "metal_mat5", 27.65, NoGI);
    run_image_test(ren_ctx, threads, "metal_mat5", 27.35, NoDiffGI);
    run_image_test(ren_ctx, threads, "metal_mat5", 24.00, MedDiffGI);
    run_image_test(ren_ctx, threads, "metal_mat5", 19.60, Full);
    run_image_test(ren_ctx, threads, "metal_mat5", 19.70, Full_Ultra);

    puts(" ---------------");
    // plastic material
    run_image_test(ren_ctx, threads, "plastic_mat0", 27.80, NoGI);
    run_image_test(ren_ctx, threads, "plastic_mat0", 27.50, NoDiffGI);
    run_image_test(ren_ctx, threads, "plastic_mat0", 27.85, MedDiffGI);
    run_image_test(ren_ctx, threads, "plastic_mat0", 26.00, Full);
    run_image_test(ren_ctx, threads, "plastic_mat0", 26.20, Full_Ultra);
    run_image_test(ren_ctx, threads, "plastic_mat1", 26.95, NoGI);
    run_image_test(ren_ctx, threads, "plastic_mat1", 25.70, NoDiffGI);
    run_image_test(ren_ctx, threads, "plastic_mat1", 23.75, MedDiffGI);
    run_image_test(ren_ctx, threads, "plastic_mat1", 22.70, Full);
    run_image_test(ren_ctx, threads, "plastic_mat1", 22.70, Full_Ultra);
    run_image_test(ren_ctx, threads, "plastic_mat2", 25.20, NoGI);
    run_image_test(ren_ctx, threads, "plastic_mat2", 25.65, NoDiffGI);
    run_image_test(ren_ctx, threads, "plastic_mat2", 27.30, MedDiffGI);
    run_image_test(ren_ctx, threads, "plastic_mat2", 25.45, Full);
    run_image_test(ren_ctx, threads, "plastic_mat2", 25.45, Full_Ultra);
    run_image_test(ren_ctx, threads, "plastic_mat3", 27.40, NoGI);
    run_image_test(ren_ctx, threads, "plastic_mat3", 27.00, NoDiffGI);
    run_image_test(ren_ctx, threads, "plastic_mat3", 24.55, MedDiffGI);
    run_image_test(ren_ctx, threads, "plastic_mat3", 20.30, Full);
    run_image_test(ren_ctx, threads, "plastic_mat3", 20.90, Full_Ultra);
    run_image_test(ren_ctx, threads, "plastic_mat4", 26.90, NoGI);
    run_image_test(ren_ctx, threads, "plastic_mat4", 25.50, NoDiffGI);
    run_image_test(ren_ctx, threads, "plastic_mat4", 21.70, MedDiffGI);
    run_image_test(ren_ctx, threads, "plastic_mat4", 18.95, Full);
    run_image_test(ren_ctx, threads, "plastic_mat4", 19.25, Full_Ultra);
    run_image_test(ren_ctx, threads, "plastic_mat5", 24.25, NoGI);
    run_image_test(ren_ctx, threads, "plastic_mat5", 23.65, NoDiffGI);
    run_image_test(ren_ctx, threads, "plastic_mat5", 24.40, MedDiffGI);
    run_image_test(ren_ctx, threads, "plastic_mat5", 19.95, Full);
    run_image_test(ren_ctx, threads, "plastic_mat5", 20.55, Full_Ultra);

    puts(" ---------------");
    // tint material
    run_image_test(ren_ctx, threads, "tint_mat0", 27.80, NoGI);
    run_image_test(ren_ctx, threads, "tint_mat0", 27.90, NoDiffGI);
    run_image_test(ren_ctx, threads, "tint_mat0", 27.40, MedDiffGI);
    run_image_test(ren_ctx, threads, "tint_mat0", 25.55, Full);
    run_image_test(ren_ctx, threads, "tint_mat1", 26.65, NoGI);
    run_image_test(ren_ctx, threads, "tint_mat1", 25.80, NoDiffGI);
    run_image_test(ren_ctx, threads, "tint_mat1", 22.60, MedDiffGI);
    run_image_test(ren_ctx, threads, "tint_mat1", 21.55, Full);
    run_image_test(ren_ctx, threads, "tint_mat2", 25.10, NoGI);
    run_image_test(ren_ctx, threads, "tint_mat2", 25.65, NoDiffGI);
    run_image_test(ren_ctx, threads, "tint_mat2", 27.85, MedDiffGI);
    run_image_test(ren_ctx, threads, "tint_mat2", 25.90, Full);
    run_image_test(ren_ctx, threads, "tint_mat3", 27.60, NoGI);
    run_image_test(ren_ctx, threads, "tint_mat3", 27.55, NoDiffGI);
    run_image_test(ren_ctx, threads, "tint_mat3", 24.75, MedDiffGI);
    run_image_test(ren_ctx, threads, "tint_mat3", 18.60, Full);
    run_image_test(ren_ctx, threads, "tint_mat4", 26.15, NoGI);
    run_image_test(ren_ctx, threads, "tint_mat4", 24.85, NoDiffGI);
    run_image_test(ren_ctx, threads, "tint_mat4", 21.30, MedDiffGI);
    run_image_test(ren_ctx, threads, "tint_mat4", 16.80, Full);
    run_image_test(ren_ctx, threads, "tint_mat5", 24.60, NoGI);
    run_image_test(ren_ctx, threads, "tint_mat5", 24.55, NoDiffGI);
    run_image_test(ren_ctx, threads, "tint_mat5", 23.60, MedDiffGI);
    run_image_test(ren_ctx, threads, "tint_mat5", 17.25, Full);

    puts(" ---------------");
    // clearcoat material
    run_image_test(ren_ctx, threads, "coat_mat0", 28.30, NoGI);
    run_image_test(ren_ctx, threads, "coat_mat1", 26.90, NoGI);
    run_image_test(ren_ctx, threads, "coat_mat2", 25.90, NoGI);
    run_image_test(ren_ctx, threads, "coat_mat3", 27.90, NoGI);
    run_image_test(ren_ctx, threads, "coat_mat4", 26.50, NoGI);
    run_image_test(ren_ctx, threads, "coat_mat5", 25.35, NoGI);

    puts(" ---------------");
    // alpha material
    run_image_test(ren_ctx, threads, "alpha_mat0", 25.90, NoGI);
    run_image_test(ren_ctx, threads, "alpha_mat0", 23.35, NoDiffGI);
    run_image_test(ren_ctx, threads, "alpha_mat0", 23.55, MedDiffGI);
    run_image_test(ren_ctx, threads, "alpha_mat0", 24.35, Full);
    run_image_test(ren_ctx, threads, "alpha_mat0", 24.65, Full_Ultra);
    run_image_test(ren_ctx, threads, "alpha_mat1", 26.25, NoGI);
    run_image_test(ren_ctx, threads, "alpha_mat1", 24.60, NoDiffGI);
    run_image_test(ren_ctx, threads, "alpha_mat1", 25.20, MedDiffGI);
    run_image_test(ren_ctx, threads, "alpha_mat1", 26.55, Full);
    run_image_test(ren_ctx, threads, "alpha_mat1", 26.90, Full_Ultra);
    run_image_test(ren_ctx, threads, "alpha_mat2", 26.80, NoGI);
    run_image_test(ren_ctx, threads, "alpha_mat2", 26.20, NoDiffGI);
    run_image_test(ren_ctx, threads, "alpha_mat2", 28.35, MedDiffGI);
    run_image_test(ren_ctx, threads, "alpha_mat2", 29.40, Full);
    run_image_test(ren_ctx, threads, "alpha_mat2", 29.55, Full_Ultra);
    run_image_test(ren_ctx, threads, "alpha_mat3", 26.55, NoGI);
    run_image_test(ren_ctx, threads, "alpha_mat3", 26.55, NoDiffGI);
    run_image_test(ren_ctx, threads, "alpha_mat3", 30.10, MedDiffGI);
    run_image_test(ren_ctx, threads, "alpha_mat3", 27.10, Full);
    run_image_test(ren_ctx, threads, "alpha_mat3", 27.00, Full_Ultra);
}
