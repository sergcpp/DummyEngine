#include "test_common.h"

#include "test_scene.h"

extern std::string_view g_device_name;
extern int g_validation_level;
extern bool g_nohwrt, g_nosubgroup;

void test_shading(Sys::ThreadPool &threads, const bool full) {
    LogErr log;
    TestContext ren_ctx(512, 512, g_device_name, g_validation_level, g_nohwrt, g_nosubgroup, &log);

    // complex materials
    run_image_test(ren_ctx, threads, "visibility_flags", 25.40, Full);
    run_image_test(ren_ctx, threads, "visibility_flags", 25.45, Full_Ultra);
    run_image_test(ren_ctx, threads, "visibility_flags_sun", 24.70, Full);
    run_image_test(ren_ctx, threads, "visibility_flags_sun", 26.60, Full_Ultra);
    run_image_test(ren_ctx, threads, "two_sided_mat", 26.80, MedDiffGI);
    run_image_test(ren_ctx, threads, "complex_mat0", 26.70, MedDiffGI);
    run_image_test(ren_ctx, threads, "complex_mat0", 25.40, Full);
    run_image_test(ren_ctx, threads, "complex_mat0", 25.20, Full_Ultra);
    run_image_test(ren_ctx, threads, "complex_mat1", 29.35, MedDiffGI);
    run_image_test(ren_ctx, threads, "complex_mat1", 27.75, Full);
    run_image_test(ren_ctx, threads, "complex_mat1", 27.70, Full_Ultra);
    run_image_test(ren_ctx, threads, "complex_mat2", 24.70, MedDiffGI);
    run_image_test(ren_ctx, threads, "complex_mat2", 24.35, Full);
    run_image_test(ren_ctx, threads, "complex_mat2", 26.25, Full_Ultra);
    run_image_test(ren_ctx, threads, "complex_mat2_area_spread", 17.10, NoDiffGI);
    run_image_test(ren_ctx, threads, "complex_mat2_area_spread", 18.05, MedDiffGI);
    run_image_test(ren_ctx, threads, "complex_mat2_area_spread", 19.20, Full);
    run_image_test(ren_ctx, threads, "complex_mat2_area_spread", 19.15, Full_Ultra);
    run_image_test(ren_ctx, threads, "complex_mat2_emissive", 18.10, Full);
    run_image_test(ren_ctx, threads, "complex_mat2_emissive", 17.05, Full_Ultra);
    run_image_test(ren_ctx, threads, "complex_mat2_dyn",
                   std::vector<double>{23.85, 23.85, 23.80, 23.70, 23.70, 23.60, 23.55, 23.50, 23.40, 23.30, //
                                       23.15, 23.05, 23.00, 22.90, 22.80, 22.85, 22.95, 23.00, 23.00, 23.05,
                                       23.15, 23.30, 23.30, 23.35, 23.30, 23.55, 23.65, 23.80, 24.05, 24.05,
                                       24.15, 24.15, 24.30},
                   MedDiffGI);
    run_image_test(ren_ctx, threads, "complex_mat2_dyn",
                   std::vector<double>{22.55, 22.60, 22.60, 22.65, 22.70, 22.70, 22.75, 22.75, 22.75, 22.75, //
                                       22.65, 22.60, 22.65, 22.55, 22.55, 22.55, 22.55, 22.70, 22.70, 22.75,
                                       22.95, 23.05, 23.05, 23.00, 22.95, 23.20, 23.25, 23.40, 23.60, 23.65,
                                       23.70, 23.65, 23.80},
                   Full);
    run_image_test(ren_ctx, threads, "complex_mat2_dyn",
                   std::vector<double>{23.05, 23.15, 23.15, 23.15, 23.15, 23.10, 23.15, 23.15, 23.15, 23.15, //
                                       23.10, 23.15, 23.15, 23.10, 23.05, 23.05, 23.10, 23.10, 23.00, 23.05,
                                       23.25, 23.30, 23.30, 23.25, 23.15, 23.40, 23.45, 23.55, 23.75, 23.80,
                                       23.80, 23.80, 23.95},
                   Full_Ultra);
    run_image_test(ren_ctx, threads, "complex_mat2_far_away", 24.25, MedDiffGI);
    run_image_test(ren_ctx, threads, "complex_mat2_far_away", 23.85, Full);
    run_image_test(ren_ctx, threads, "complex_mat2_far_away", 25.55, Full_Ultra);
    run_image_test(ren_ctx, threads, "complex_mat2_spot_light", 26.95, MedDiffGI);
    run_image_test(ren_ctx, threads, "complex_mat2_spot_light", 26.45, Full);
    run_image_test(ren_ctx, threads, "complex_mat2_spot_light", 27.45, Full_Ultra);
    run_image_test(ren_ctx, threads, "complex_mat2_sun_light", 21.80, Full);
    run_image_test(ren_ctx, threads, "complex_mat2_sun_light", 22.45, Full_Ultra);
    run_image_test(ren_ctx, threads, "complex_mat2_sun_light_dyn",
                   std::vector<double>{33.35, 33.65, 33.70, 33.65, 33.50, 33.15, 32.65, 32.05, 31.25, 30.30, //
                                       29.45, 28.20, 26.40, 25.55, 23.60, 22.00, 20.35, 22.10, 23.60, 24.60,
                                       25.20, 25.95, 25.60, 26.95, 26.55, 28.20, 28.20, 27.80, 29.20, 29.35,
                                       29.35, 30.10, 30.50},
                   Full);
    run_image_test(ren_ctx, threads, "complex_mat2_sun_light_dyn",
                   std::vector<double>{33.45, 33.80, 33.55, 33.45, 33.65, 33.45, 32.90, 31.90, 31.45, 30.40, //
                                       29.35, 28.15, 26.95, 25.70, 24.05, 22.60, 20.80, 22.05, 23.75, 24.25,
                                       25.10, 26.10, 26.00, 27.10, 26.70, 27.95, 28.35, 27.90, 29.40, 29.50,
                                       29.55, 30.25, 30.70},
                   Full_Ultra);
    run_image_test(ren_ctx, threads, "complex_mat2_moon_light", 24.10, MedDiffGI);
    run_image_test(ren_ctx, threads, "complex_mat2_moon_light", 23.25, Full);
    run_image_test(ren_ctx, threads, "complex_mat2_moon_light", 24.05, Full_Ultra);
    run_image_test(ren_ctx, threads, "complex_mat2_hdri_light", 21.35, MedDiffGI);
    run_image_test(ren_ctx, threads, "complex_mat2_hdri_light", 22.90, Full);
    run_image_test(ren_ctx, threads, "complex_mat2_hdri_light", 24.30, Full_Ultra);
    run_image_test(ren_ctx, threads, "complex_mat2_portal_hdri", 24.50, MedDiffGI);
    run_image_test(ren_ctx, threads, "complex_mat2_portal_hdri", 23.85, Full);
    run_image_test(ren_ctx, threads, "complex_mat2_portal_hdri", 24.55, Full_Ultra);
    run_image_test(ren_ctx, threads, "complex_mat2_portal_sky", 22.15, MedDiffGI);
    run_image_test(ren_ctx, threads, "complex_mat2_portal_sky", 23.55, Full);
    run_image_test(ren_ctx, threads, "complex_mat2_portal_sky", 23.90, Full_Ultra);
    run_image_test(ren_ctx, threads, "complex_mat2_mesh_lights", 20.45, MedDiffGI);
    run_image_test(ren_ctx, threads, "complex_mat2_mesh_lights", 20.55, Full);
    run_image_test(ren_ctx, threads, "complex_mat2_mesh_lights", 21.20, Full_Ultra);
    run_image_test(ren_ctx, threads, "complex_mat3", 22.15, MedDiffGI);
    run_image_test(ren_ctx, threads, "complex_mat3", 22.10, Full);
    run_image_test(ren_ctx, threads, "complex_mat3", 22.40, Full_Ultra);
    run_image_test(ren_ctx, threads, "complex_mat3_dyn",
                   std::vector<double>{22.25, 22.25, 22.25, 22.25, 22.30, 22.30, 22.30, 22.30, 22.30, 22.30, //
                                       22.30, 22.30, 22.35, 22.35, 22.30, 22.30, 22.30, 22.30, 22.30, 22.30,
                                       22.30, 22.35, 22.35, 22.35, 22.35, 22.35, 22.35, 22.35, 22.35, 22.35,
                                       22.35, 22.35, 22.35},
                   MedDiffGI);
    run_image_test(ren_ctx, threads, "complex_mat3_dyn",
                   std::vector<double>{22.05, 22.05, 22.05, 22.10, 22.10, 22.10, 22.15, 22.15, 22.15, 22.15, //
                                       22.20, 22.20, 22.20, 22.20, 22.20, 22.20, 22.25, 22.25, 22.25, 22.25,
                                       22.25, 22.25, 22.25, 22.25, 22.25, 22.30, 22.30, 22.30, 22.30, 22.30,
                                       22.30, 22.30, 22.30},
                   Full);
    run_image_test(ren_ctx, threads, "complex_mat3_dyn",
                   std::vector<double>{22.30, 22.30, 22.35, 22.35, 22.35, 22.40, 22.40, 22.40, 22.40, 22.45, //
                                       22.45, 22.45, 22.50, 22.50, 22.50, 22.50, 22.50, 22.50, 22.50, 22.50,
                                       22.50, 22.50, 22.55, 22.55, 22.55, 22.55, 22.55, 22.55, 22.60, 22.60,
                                       22.60, 22.60, 22.60},
                   Full_Ultra);
    run_image_test(ren_ctx, threads, "complex_mat3_sun_light", 19.15, Full);
    run_image_test(ren_ctx, threads, "complex_mat3_sun_light", 24.15, Full_Ultra);
    run_image_test(ren_ctx, threads, "complex_mat3_mesh_lights", 17.75, MedDiffGI);
    run_image_test(ren_ctx, threads, "complex_mat3_mesh_lights", 19.95, Full);
    run_image_test(ren_ctx, threads, "complex_mat3_mesh_lights", 19.90, Full_Ultra);
    run_image_test(ren_ctx, threads, "complex_mat4", 20.10, Full);
    run_image_test(ren_ctx, threads, "complex_mat4", 20.05, Full_Ultra);
    run_image_test(ren_ctx, threads, "complex_mat4_sun_light", 20.05, Full);
    run_image_test(ren_ctx, threads, "complex_mat4_sun_light", 19.90, Full_Ultra);
    run_image_test(ren_ctx, threads, "emit_mat0", 24.60, Full);
    run_image_test(ren_ctx, threads, "emit_mat0", 23.60, Full_Ultra);
    run_image_test(ren_ctx, threads, "emit_mat1", 23.00, Full);
    run_image_test(ren_ctx, threads, "emit_mat1", 21.55, Full_Ultra);

    if (!full) {
        return;
    }

    puts(" ---------------");
    run_image_test(ren_ctx, threads, "complex_mat0", 35.20, NoGI);
    run_image_test(ren_ctx, threads, "complex_mat0", 29.10, NoDiffGI);
    run_image_test(ren_ctx, threads, "complex_mat1", 34.70, NoGI);
    run_image_test(ren_ctx, threads, "complex_mat1", 31.10, NoDiffGI);
    run_image_test(ren_ctx, threads, "complex_mat2", 33.35, NoGI);
    run_image_test(ren_ctx, threads, "complex_mat2", 27.05, NoDiffGI);
    run_image_test(ren_ctx, threads, "complex_mat2_spot_light", 35.85, NoGI);
    run_image_test(ren_ctx, threads, "complex_mat2_spot_light", 31.45, NoDiffGI);
    run_image_test(ren_ctx, threads, "complex_mat2_sun_light", 28.90, NoGI);
    run_image_test(ren_ctx, threads, "complex_mat2_portal_hdri", 24.40, NoDiffGI);
    run_image_test(ren_ctx, threads, "complex_mat3", 20.85, NoGI);
    run_image_test(ren_ctx, threads, "complex_mat3", 22.80, NoDiffGI);
    run_image_test(ren_ctx, threads, "complex_mat3_sun_light", 17.10, NoGI);

    puts(" ---------------");
    // diffuse material
    run_image_test(ren_ctx, threads, "diff_mat0", 36.00, NoGI);
    run_image_test(ren_ctx, threads, "diff_mat0", 30.05, MedDiffGI);
    run_image_test(ren_ctx, threads, "diff_mat0", 27.20, Full);
    run_image_test(ren_ctx, threads, "diff_mat1", 35.40, NoGI);
    run_image_test(ren_ctx, threads, "diff_mat1", 28.15, MedDiffGI);
    run_image_test(ren_ctx, threads, "diff_mat1", 25.55, Full);
    run_image_test(ren_ctx, threads, "diff_mat2", 35.70, NoGI);
    run_image_test(ren_ctx, threads, "diff_mat2", 29.80, MedDiffGI);
    run_image_test(ren_ctx, threads, "diff_mat2", 27.05, Full);
    run_image_test(ren_ctx, threads, "diff_mat3", 36.20, NoGI);
    run_image_test(ren_ctx, threads, "diff_mat3", 26.25, MedDiffGI);
    run_image_test(ren_ctx, threads, "diff_mat3", 21.50, Full);
    run_image_test(ren_ctx, threads, "diff_mat4", 35.60, NoGI);
    run_image_test(ren_ctx, threads, "diff_mat4", 25.35, MedDiffGI);
    run_image_test(ren_ctx, threads, "diff_mat4", 20.05, Full);
    run_image_test(ren_ctx, threads, "diff_mat5", 36.00, NoGI);
    run_image_test(ren_ctx, threads, "diff_mat5", 26.20, MedDiffGI);
    run_image_test(ren_ctx, threads, "diff_mat5", 21.35, Full);

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
    run_image_test(ren_ctx, threads, "spec_mat0", 35.65, NoGI);
    run_image_test(ren_ctx, threads, "spec_mat0", 27.10, NoDiffGI);
    run_image_test(ren_ctx, threads, "spec_mat0", 24.05, MedDiffGI);
    run_image_test(ren_ctx, threads, "spec_mat0", 23.80, Full);
    run_image_test(ren_ctx, threads, "spec_mat0", 26.05, Full_Ultra);
    run_image_test(ren_ctx, threads, "spec_mat1", 19.40, NoGI);
    run_image_test(ren_ctx, threads, "spec_mat1", 21.90, NoDiffGI);
    run_image_test(ren_ctx, threads, "spec_mat1", 18.00, MedDiffGI);
    run_image_test(ren_ctx, threads, "spec_mat1", 17.85, Full);
    run_image_test(ren_ctx, threads, "spec_mat1", 18.60, Full_Ultra);
    run_image_test(ren_ctx, threads, "spec_mat2", 35.05, NoGI);
    run_image_test(ren_ctx, threads, "spec_mat2", 28.55, NoDiffGI);
    run_image_test(ren_ctx, threads, "spec_mat2", 25.50, MedDiffGI);
    run_image_test(ren_ctx, threads, "spec_mat2", 24.90, Full);
    run_image_test(ren_ctx, threads, "spec_mat2", 24.75, Full_Ultra);
    run_image_test(ren_ctx, threads, "spec_mat3", 34.00, NoGI);
    run_image_test(ren_ctx, threads, "spec_mat3", 29.00, NoDiffGI);
    run_image_test(ren_ctx, threads, "spec_mat3", 21.05, MedDiffGI);
    run_image_test(ren_ctx, threads, "spec_mat3", 18.50, Full);
    run_image_test(ren_ctx, threads, "spec_mat3", 19.85, Full_Ultra);
    run_image_test(ren_ctx, threads, "spec_mat4", 21.10, NoGI);
    run_image_test(ren_ctx, threads, "spec_mat4", 21.95, NoDiffGI);
    run_image_test(ren_ctx, threads, "spec_mat4", 14.70, MedDiffGI);
    run_image_test(ren_ctx, threads, "spec_mat4", 13.75, Full);
    run_image_test(ren_ctx, threads, "spec_mat4", 13.85, Full_Ultra);
    run_image_test(ren_ctx, threads, "spec_mat5", 30.30, NoGI);
    run_image_test(ren_ctx, threads, "spec_mat5", 28.85, NoDiffGI);
    run_image_test(ren_ctx, threads, "spec_mat5", 19.25, MedDiffGI);
    run_image_test(ren_ctx, threads, "spec_mat5", 17.35, Full);
    run_image_test(ren_ctx, threads, "spec_mat5", 17.35, Full_Ultra);

    puts(" ---------------");
    // metal material
    run_image_test(ren_ctx, threads, "metal_mat0", 31.55, NoGI);
    run_image_test(ren_ctx, threads, "metal_mat0", 29.45, NoDiffGI);
    run_image_test(ren_ctx, threads, "metal_mat0", 26.40, MedDiffGI);
    run_image_test(ren_ctx, threads, "metal_mat0", 25.75, Full);
    run_image_test(ren_ctx, threads, "metal_mat0", 27.55, Full_Ultra);
    run_image_test(ren_ctx, threads, "metal_mat1", 23.60, NoGI);
    run_image_test(ren_ctx, threads, "metal_mat1", 26.90, NoDiffGI);
    run_image_test(ren_ctx, threads, "metal_mat1", 24.30, MedDiffGI);
    run_image_test(ren_ctx, threads, "metal_mat1", 23.75, Full);
    run_image_test(ren_ctx, threads, "metal_mat1", 24.05, Full_Ultra);
    run_image_test(ren_ctx, threads, "metal_mat2", 34.30, NoGI);
    run_image_test(ren_ctx, threads, "metal_mat2", 33.25, NoDiffGI);
    run_image_test(ren_ctx, threads, "metal_mat2", 30.25, MedDiffGI);
    run_image_test(ren_ctx, threads, "metal_mat2", 28.25, Full);
    run_image_test(ren_ctx, threads, "metal_mat2", 28.20, Full_Ultra);
    run_image_test(ren_ctx, threads, "metal_mat3", 34.80, NoGI);
    run_image_test(ren_ctx, threads, "metal_mat3", 30.05, NoDiffGI);
    run_image_test(ren_ctx, threads, "metal_mat3", 22.80, MedDiffGI);
    run_image_test(ren_ctx, threads, "metal_mat3", 19.35, Full);
    run_image_test(ren_ctx, threads, "metal_mat3", 20.15, Full_Ultra);
    run_image_test(ren_ctx, threads, "metal_mat4", 25.20, NoGI);
    run_image_test(ren_ctx, threads, "metal_mat4", 26.40, NoDiffGI);
    run_image_test(ren_ctx, threads, "metal_mat4", 19.30, MedDiffGI);
    run_image_test(ren_ctx, threads, "metal_mat4", 17.15, Full);
    run_image_test(ren_ctx, threads, "metal_mat4", 17.15, Full_Ultra);
    run_image_test(ren_ctx, threads, "metal_mat5", 34.15, NoGI);
    run_image_test(ren_ctx, threads, "metal_mat5", 32.65, NoDiffGI);
    run_image_test(ren_ctx, threads, "metal_mat5", 24.00, MedDiffGI);
    run_image_test(ren_ctx, threads, "metal_mat5", 19.60, Full);
    run_image_test(ren_ctx, threads, "metal_mat5", 19.70, Full_Ultra);

    puts(" ---------------");
    // plastic material
    run_image_test(ren_ctx, threads, "plastic_mat0", 35.65, NoGI);
    run_image_test(ren_ctx, threads, "plastic_mat0", 32.60, NoDiffGI);
    run_image_test(ren_ctx, threads, "plastic_mat0", 27.85, MedDiffGI);
    run_image_test(ren_ctx, threads, "plastic_mat0", 26.00, Full);
    run_image_test(ren_ctx, threads, "plastic_mat0", 26.20, Full_Ultra);
    run_image_test(ren_ctx, threads, "plastic_mat1", 33.45, NoGI);
    run_image_test(ren_ctx, threads, "plastic_mat1", 27.90, NoDiffGI);
    run_image_test(ren_ctx, threads, "plastic_mat1", 23.75, MedDiffGI);
    run_image_test(ren_ctx, threads, "plastic_mat1", 22.70, Full);
    run_image_test(ren_ctx, threads, "plastic_mat1", 22.70, Full_Ultra);
    run_image_test(ren_ctx, threads, "plastic_mat2", 34.80, NoGI);
    run_image_test(ren_ctx, threads, "plastic_mat2", 33.20, NoDiffGI);
    run_image_test(ren_ctx, threads, "plastic_mat2", 27.85, MedDiffGI);
    run_image_test(ren_ctx, threads, "plastic_mat2", 25.45, Full);
    run_image_test(ren_ctx, threads, "plastic_mat2", 25.45, Full_Ultra);
    run_image_test(ren_ctx, threads, "plastic_mat3", 34.35, NoGI);
    run_image_test(ren_ctx, threads, "plastic_mat3", 31.75, NoDiffGI);
    run_image_test(ren_ctx, threads, "plastic_mat3", 24.55, MedDiffGI);
    run_image_test(ren_ctx, threads, "plastic_mat3", 20.30, Full);
    run_image_test(ren_ctx, threads, "plastic_mat3", 20.90, Full_Ultra);
    run_image_test(ren_ctx, threads, "plastic_mat4", 33.35, NoGI);
    run_image_test(ren_ctx, threads, "plastic_mat4", 28.30, NoDiffGI);
    run_image_test(ren_ctx, threads, "plastic_mat4", 21.70, MedDiffGI);
    run_image_test(ren_ctx, threads, "plastic_mat4", 18.95, Full);
    run_image_test(ren_ctx, threads, "plastic_mat4", 19.25, Full_Ultra);
    run_image_test(ren_ctx, threads, "plastic_mat5", 35.05, NoGI);
    run_image_test(ren_ctx, threads, "plastic_mat5", 30.40, NoDiffGI);
    run_image_test(ren_ctx, threads, "plastic_mat5", 25.20, MedDiffGI);
    run_image_test(ren_ctx, threads, "plastic_mat5", 19.95, Full);
    run_image_test(ren_ctx, threads, "plastic_mat5", 20.55, Full_Ultra);

    puts(" ---------------");
    // tint material
    run_image_test(ren_ctx, threads, "tint_mat0", 35.65, NoGI);
    run_image_test(ren_ctx, threads, "tint_mat0", 34.40, NoDiffGI);
    run_image_test(ren_ctx, threads, "tint_mat0", 27.40, MedDiffGI);
    run_image_test(ren_ctx, threads, "tint_mat0", 25.55, Full);
    run_image_test(ren_ctx, threads, "tint_mat1", 31.15, NoGI);
    run_image_test(ren_ctx, threads, "tint_mat1", 26.40, NoDiffGI);
    run_image_test(ren_ctx, threads, "tint_mat1", 22.60, MedDiffGI);
    run_image_test(ren_ctx, threads, "tint_mat1", 21.55, Full);
    run_image_test(ren_ctx, threads, "tint_mat2", 34.25, NoGI);
    run_image_test(ren_ctx, threads, "tint_mat2", 33.85, NoDiffGI);
    run_image_test(ren_ctx, threads, "tint_mat2", 28.20, MedDiffGI);
    run_image_test(ren_ctx, threads, "tint_mat2", 25.90, Full);
    run_image_test(ren_ctx, threads, "tint_mat3", 35.85, NoGI);
    run_image_test(ren_ctx, threads, "tint_mat3", 34.35, NoDiffGI);
    run_image_test(ren_ctx, threads, "tint_mat3", 24.75, MedDiffGI);
    run_image_test(ren_ctx, threads, "tint_mat3", 18.60, Full);
    run_image_test(ren_ctx, threads, "tint_mat4", 30.10, NoGI);
    run_image_test(ren_ctx, threads, "tint_mat4", 26.15, NoDiffGI);
    run_image_test(ren_ctx, threads, "tint_mat4", 21.30, MedDiffGI);
    run_image_test(ren_ctx, threads, "tint_mat4", 16.80, Full);
    run_image_test(ren_ctx, threads, "tint_mat5", 34.40, NoGI);
    run_image_test(ren_ctx, threads, "tint_mat5", 32.40, NoDiffGI);
    run_image_test(ren_ctx, threads, "tint_mat5", 23.60, MedDiffGI);
    run_image_test(ren_ctx, threads, "tint_mat5", 17.25, Full);

    puts(" ---------------");
    // clearcoat material
    run_image_test(ren_ctx, threads, "coat_mat0", 35.75, NoGI);
    run_image_test(ren_ctx, threads, "coat_mat1", 31.00, NoGI);
    run_image_test(ren_ctx, threads, "coat_mat2", 28.50, NoGI);
    run_image_test(ren_ctx, threads, "coat_mat3", 35.10, NoGI);
    run_image_test(ren_ctx, threads, "coat_mat4", 30.55, NoGI);
    run_image_test(ren_ctx, threads, "coat_mat5", 27.80, NoGI);

    puts(" ---------------");
    // alpha material
    run_image_test(ren_ctx, threads, "alpha_mat0", 30.50, NoGI);
    run_image_test(ren_ctx, threads, "alpha_mat0", 26.25, NoDiffGI);
    run_image_test(ren_ctx, threads, "alpha_mat0", 24.75, MedDiffGI);
    run_image_test(ren_ctx, threads, "alpha_mat0", 24.80, Full);
    run_image_test(ren_ctx, threads, "alpha_mat0", 24.85, Full_Ultra);
    run_image_test(ren_ctx, threads, "alpha_mat1", 31.85, NoGI);
    run_image_test(ren_ctx, threads, "alpha_mat1", 28.60, NoDiffGI);
    run_image_test(ren_ctx, threads, "alpha_mat1", 27.30, MedDiffGI);
    run_image_test(ren_ctx, threads, "alpha_mat1", 26.85, Full);
    run_image_test(ren_ctx, threads, "alpha_mat1", 27.00, Full_Ultra);
    run_image_test(ren_ctx, threads, "alpha_mat2", 36.30, NoGI);
    run_image_test(ren_ctx, threads, "alpha_mat2", 34.65, NoDiffGI);
    run_image_test(ren_ctx, threads, "alpha_mat2", 31.65, MedDiffGI);
    run_image_test(ren_ctx, threads, "alpha_mat2", 29.40, Full);
    run_image_test(ren_ctx, threads, "alpha_mat2", 29.55, Full_Ultra);
    run_image_test(ren_ctx, threads, "alpha_mat3", 36.70, NoGI);
    run_image_test(ren_ctx, threads, "alpha_mat3", 37.15, NoDiffGI);
    run_image_test(ren_ctx, threads, "alpha_mat3", 30.10, MedDiffGI);
    run_image_test(ren_ctx, threads, "alpha_mat3", 27.10, Full);
    run_image_test(ren_ctx, threads, "alpha_mat3", 27.00, Full_Ultra);
}
