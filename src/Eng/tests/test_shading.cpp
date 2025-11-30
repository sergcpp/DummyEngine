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
    run_image_test(ren_ctx, threads, "visibility_flags", 25.30, Full_Ultra);
    run_image_test(ren_ctx, threads, "visibility_flags_sun", 24.95, Full);
    run_image_test(ren_ctx, threads, "visibility_flags_sun", 26.30, Full_Ultra);
    run_image_test(ren_ctx, threads, "two_sided_mat", 39.45, NoShadow);
    run_image_test(ren_ctx, threads, "two_sided_mat", 29.90, NoGI);
    run_image_test(ren_ctx, threads, "two_sided_mat", 29.55, NoDiffGI);
    run_image_test(ren_ctx, threads, "two_sided_mat", 27.95, MedDiffGI);
    run_image_test(ren_ctx, threads, "complex_mat0", 38.40, NoShadow);
    run_image_test(ren_ctx, threads, "complex_mat0", 35.20, NoGI);
    run_image_test(ren_ctx, threads, "complex_mat0", 29.15, NoDiffGI);
    run_image_test(ren_ctx, threads, "complex_mat0", 27.20, MedDiffGI);
    run_image_test(ren_ctx, threads, "complex_mat0", 26.05, Full);
    run_image_test(ren_ctx, threads, "complex_mat0", 26.30, Full_Ultra);
    run_image_test(ren_ctx, threads, "complex_mat1", 35.75, NoShadow);
    run_image_test(ren_ctx, threads, "complex_mat1", 34.70, NoGI);
    run_image_test(ren_ctx, threads, "complex_mat1", 31.10, NoDiffGI);
    run_image_test(ren_ctx, threads, "complex_mat1", 29.74, MedDiffGI);
    run_image_test(ren_ctx, threads, "complex_mat1", 28.40, Full);
    run_image_test(ren_ctx, threads, "complex_mat1", 28.85, Full_Ultra);
    run_image_test(ren_ctx, threads, "complex_mat2", 33.90, NoShadow);
    run_image_test(ren_ctx, threads, "complex_mat2", 33.35, NoGI);
    run_image_test(ren_ctx, threads, "complex_mat2", 27.20, NoDiffGI);
    run_image_test(ren_ctx, threads, "complex_mat2", 25.05, MedDiffGI);
    run_image_test(ren_ctx, threads, "complex_mat2", 24.75, Full);
    run_image_test(ren_ctx, threads, "complex_mat2", 27.70, Full_Ultra);
    run_image_test(ren_ctx, threads, "complex_mat2_area_spread", 17.10, NoDiffGI);
    run_image_test(ren_ctx, threads, "complex_mat2_area_spread", 17.15, MedDiffGI);
    run_image_test(ren_ctx, threads, "complex_mat2_area_spread", 18.50, Full);
    run_image_test(ren_ctx, threads, "complex_mat2_area_spread", 18.30, Full_Ultra);
    run_image_test(ren_ctx, threads, "complex_mat2_emissive", 18.35, Full);
    run_image_test(ren_ctx, threads, "complex_mat2_emissive", 17.35, Full_Ultra);
    run_image_test(ren_ctx, threads, "complex_mat2_dyn",
                   std::vector<double>{24.70, 24.65, 24.50, 24.35, 24.30, 24.15, 24.05, 24.00, 23.85, 23.60, //
                                       23.50, 23.45, 23.30, 23.15, 22.95, 22.95, 22.95, 23.15, 23.05, 23.15,
                                       23.15, 23.40, 23.40, 23.55, 23.50, 23.60, 23.85, 24.20, 24.50, 24.55,
                                       24.50, 24.45, 24.70},
                   MedDiffGI);
    run_image_test(ren_ctx, threads, "complex_mat2_dyn",
                   std::vector<double>{23.30, 23.30, 23.30, 23.30, 23.30, 23.30, 23.30, 23.30, 23.25, 23.15, //
                                       23.05, 23.05, 23.00, 22.90, 22.70, 22.75, 22.80, 22.95, 22.85, 22.95,
                                       22.95, 23.20, 23.20, 23.35, 23.25, 23.35, 23.60, 23.90, 24.10, 24.20,
                                       24.15, 24.10, 24.35},
                   Full);
    run_image_test(ren_ctx, threads, "complex_mat2_dyn",
                   std::vector<double>{24.45, 24.45, 24.35, 24.30, 24.20, 24.15, 24.05, 24.05, 23.95, 23.85, //
                                       23.80, 23.75, 23.70, 23.60, 23.40, 23.40, 23.40, 23.45, 23.25, 23.30,
                                       23.30, 23.55, 23.50, 23.65, 23.50, 23.60, 23.80, 24.15, 24.35, 24.45,
                                       24.40, 24.40, 24.65},
                   Full_Ultra);
    run_image_test(ren_ctx, threads, "complex_mat2_far_away", 25.05, MedDiffGI);
    run_image_test(ren_ctx, threads, "complex_mat2_far_away", 24.75, Full);
    run_image_test(ren_ctx, threads, "complex_mat2_far_away", 27.85, Full_Ultra);
    run_image_test(ren_ctx, threads, "complex_mat2_spot_light", 34.55, NoShadow);
    run_image_test(ren_ctx, threads, "complex_mat2_spot_light", 35.85, NoGI);
    run_image_test(ren_ctx, threads, "complex_mat2_spot_light", 30.95, NoDiffGI);
    run_image_test(ren_ctx, threads, "complex_mat2_spot_light", 26.70, MedDiffGI);
    run_image_test(ren_ctx, threads, "complex_mat2_spot_light", 26.45, Full);
    run_image_test(ren_ctx, threads, "complex_mat2_spot_light", 27.80, Full_Ultra);
    run_image_test(ren_ctx, threads, "complex_mat2_sun_light", 33.35, NoShadow);
    run_image_test(ren_ctx, threads, "complex_mat2_sun_light", 28.90, NoGI);
    run_image_test(ren_ctx, threads, "complex_mat2_sun_light", 33.40, NoGI_RTShadow);
    run_image_test(ren_ctx, threads, "complex_mat2_sun_light", 21.10, Full);
    run_image_test(ren_ctx, threads, "complex_mat2_sun_light", 22.80, Full_Ultra);
    run_image_test(ren_ctx, threads, "complex_mat2_sun_light_dyn",
                   std::vector<double>{31.85, 32.15, 32.25, 32.15, 32.00, 31.55, 31.15, 30.70, 30.05, 29.35, //
                                       28.55, 27.55, 26.45, 25.20, 23.70, 21.95, 20.20, 21.25, 23.10, 24.15,
                                       24.75, 24.80, 25.15, 25.90, 26.85, 27.75, 27.25, 27.85, 28.30, 28.70,
                                       29.35, 30.45, 30.50},
                   Full);
    run_image_test(ren_ctx, threads, "complex_mat2_sun_light_dyn",
                   std::vector<double>{32.55, 32.90, 33.05, 32.90, 32.65, 32.25, 31.80, 31.30, 30.75, 30.00, //
                                       29.05, 28.05, 26.85, 25.60, 24.25, 22.60, 20.80, 21.70, 23.40, 24.25,
                                       25.10, 25.15, 25.50, 26.20, 27.15, 28.10, 27.50, 28.10, 28.55, 28.90,
                                       29.55, 30.75, 30.75},
                   Full_Ultra);
    run_image_test(ren_ctx, threads, "complex_mat2_moon_light", 22.65, MedDiffGI);
    run_image_test(ren_ctx, threads, "complex_mat2_moon_light", 22.75, Full);
    run_image_test(ren_ctx, threads, "complex_mat2_moon_light", 23.40, Full_Ultra);
    run_image_test(ren_ctx, threads, "complex_mat2_hdri_light", 20.80, MedDiffGI);
    run_image_test(ren_ctx, threads, "complex_mat2_hdri_light", 22.40, Full);
    run_image_test(ren_ctx, threads, "complex_mat2_hdri_light", 23.85, Full_Ultra);
    run_image_test(ren_ctx, threads, "complex_mat2_portal_hdri", 24.40, NoDiffGI);
    run_image_test(ren_ctx, threads, "complex_mat2_portal_hdri", 24.45, MedDiffGI);
    run_image_test(ren_ctx, threads, "complex_mat2_portal_hdri", 23.85, Full);
    run_image_test(ren_ctx, threads, "complex_mat2_portal_hdri", 24.80, Full_Ultra);
    run_image_test(ren_ctx, threads, "complex_mat2_portal_sky", 23.55, MedDiffGI);
    run_image_test(ren_ctx, threads, "complex_mat2_portal_sky", 24.55, Full);
    run_image_test(ren_ctx, threads, "complex_mat2_portal_sky", 25.75, Full_Ultra);
    run_image_test(ren_ctx, threads, "complex_mat2_mesh_lights", 20.45, MedDiffGI);
    run_image_test(ren_ctx, threads, "complex_mat2_mesh_lights", 20.70, Full);
    run_image_test(ren_ctx, threads, "complex_mat2_mesh_lights", 21.55, Full_Ultra);
    run_image_test(ren_ctx, threads, "complex_mat3", 24.25, NoShadow);
    run_image_test(ren_ctx, threads, "complex_mat3", 20.85, NoGI);
    run_image_test(ren_ctx, threads, "complex_mat3", 22.90, NoDiffGI);
    run_image_test(ren_ctx, threads, "complex_mat3", 22.55, MedDiffGI);
    run_image_test(ren_ctx, threads, "complex_mat3", 22.65, Full);
    run_image_test(ren_ctx, threads, "complex_mat3", 23.15, Full_Ultra);
    run_image_test(ren_ctx, threads, "complex_mat3_dyn",
                   std::vector<double>{22.45, 22.45, 22.45, 22.50, 22.50, 22.50, 22.45, 22.50, 22.50, 22.55, //
                                       22.55, 22.50, 22.50, 22.50, 22.55, 22.60, 22.60, 22.60, 22.60, 22.65,
                                       22.60, 22.60, 22.60, 22.60, 22.60, 22.65, 22.65, 22.60, 22.60, 22.60,
                                       22.65, 22.70, 22.70},
                   MedDiffGI);
    run_image_test(ren_ctx, threads, "complex_mat3_dyn",
                   std::vector<double>{22.40, 22.40, 22.45, 22.45, 22.45, 22.45, 22.45, 22.50, 22.50, 22.55, //
                                       22.55, 22.55, 22.55, 22.55, 22.55, 22.60, 22.65, 22.65, 22.65, 22.65,
                                       22.65, 22.65, 22.65, 22.65, 22.65, 22.70, 22.70, 22.70, 22.70, 22.70,
                                       22.70, 22.75, 22.80},
                   Full);
    run_image_test(ren_ctx, threads, "complex_mat3_dyn",
                   std::vector<double>{22.80, 22.80, 22.85, 22.85, 22.85, 22.90, 22.90, 22.90, 22.90, 22.90, //
                                       22.95, 22.95, 22.95, 22.95, 23.00, 23.05, 23.10, 23.10, 23.10, 23.10,
                                       23.10, 23.10, 23.10, 23.10, 23.15, 23.20, 23.15, 23.15, 23.15, 23.15,
                                       23.20, 23.25, 23.25},
                   Full_Ultra);
    run_image_test(ren_ctx, threads, "complex_mat3_sun_light", 22.25, NoShadow);
    run_image_test(ren_ctx, threads, "complex_mat3_sun_light", 17.10, NoGI);
    run_image_test(ren_ctx, threads, "complex_mat3_sun_light", 23.20, NoGI_RTShadow);
    run_image_test(ren_ctx, threads, "complex_mat3_sun_light", 19.45, Full);
    run_image_test(ren_ctx, threads, "complex_mat3_sun_light", 24.45, Full_Ultra);
    run_image_test(ren_ctx, threads, "complex_mat3_mesh_lights", 16.90, MedDiffGI);
    run_image_test(ren_ctx, threads, "complex_mat3_mesh_lights", 20.35, Full);
    run_image_test(ren_ctx, threads, "complex_mat3_mesh_lights", 20.40, Full_Ultra);
    run_image_test(ren_ctx, threads, "complex_mat4", 20.10, Full);
    run_image_test(ren_ctx, threads, "complex_mat4", 20.05, Full_Ultra);
    run_image_test(ren_ctx, threads, "complex_mat4_sun_light", 20.05, Full);
    run_image_test(ren_ctx, threads, "complex_mat4_sun_light", 19.90, Full_Ultra);
    run_image_test(ren_ctx, threads, "emit_mat0", 24.80, Full);
    run_image_test(ren_ctx, threads, "emit_mat0", 25.50, Full_Ultra);
    run_image_test(ren_ctx, threads, "emit_mat1", 23.20, Full);
    run_image_test(ren_ctx, threads, "emit_mat1", 23.45, Full_Ultra);

    if (!full) {
        return;
    }

    puts(" ---------------");
    // diffuse material
    run_image_test(ren_ctx, threads, "diff_mat0", 46.00, NoShadow);
    run_image_test(ren_ctx, threads, "diff_mat0", 36.00, NoGI);
    run_image_test(ren_ctx, threads, "diff_mat0", 31.30, MedDiffGI);
    run_image_test(ren_ctx, threads, "diff_mat0", 28.30, Full);
    run_image_test(ren_ctx, threads, "diff_mat1", 45.75, NoShadow);
    run_image_test(ren_ctx, threads, "diff_mat1", 35.40, NoGI);
    run_image_test(ren_ctx, threads, "diff_mat1", 29.30, MedDiffGI);
    run_image_test(ren_ctx, threads, "diff_mat1", 26.50, Full);
    run_image_test(ren_ctx, threads, "diff_mat2", 44.90, NoShadow);
    run_image_test(ren_ctx, threads, "diff_mat2", 35.70, NoGI);
    run_image_test(ren_ctx, threads, "diff_mat2", 31.00, MedDiffGI);
    run_image_test(ren_ctx, threads, "diff_mat2", 28.15, Full);
    run_image_test(ren_ctx, threads, "diff_mat3", 47.45, NoShadow);
    run_image_test(ren_ctx, threads, "diff_mat3", 36.20, NoGI);
    run_image_test(ren_ctx, threads, "diff_mat3", 26.25, MedDiffGI);
    run_image_test(ren_ctx, threads, "diff_mat3", 22.75, Full);
    run_image_test(ren_ctx, threads, "diff_mat4", 48.00, NoShadow);
    run_image_test(ren_ctx, threads, "diff_mat4", 35.60, NoGI);
    run_image_test(ren_ctx, threads, "diff_mat4", 25.40, MedDiffGI);
    run_image_test(ren_ctx, threads, "diff_mat4", 21.10, Full);
    run_image_test(ren_ctx, threads, "diff_mat5", 45.10, NoShadow);
    run_image_test(ren_ctx, threads, "diff_mat5", 36.00, NoGI);
    run_image_test(ren_ctx, threads, "diff_mat5", 26.25, MedDiffGI);
    run_image_test(ren_ctx, threads, "diff_mat5", 22.55, Full);

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
    run_image_test(ren_ctx, threads, "spec_mat0", 40.10, NoShadow);
    run_image_test(ren_ctx, threads, "spec_mat0", 35.65, NoGI);
    run_image_test(ren_ctx, threads, "spec_mat0", 27.10, NoDiffGI);
    run_image_test(ren_ctx, threads, "spec_mat0", 24.15, MedDiffGI);
    run_image_test(ren_ctx, threads, "spec_mat0", 24.00, Full);
    run_image_test(ren_ctx, threads, "spec_mat0", 27.50, Full_Ultra);
    run_image_test(ren_ctx, threads, "spec_mat1", 19.60, NoShadow);
    run_image_test(ren_ctx, threads, "spec_mat1", 19.40, NoGI);
    run_image_test(ren_ctx, threads, "spec_mat1", 22.00, NoDiffGI);
    run_image_test(ren_ctx, threads, "spec_mat1", 18.15, MedDiffGI);
    run_image_test(ren_ctx, threads, "spec_mat1", 18.05, Full);
    run_image_test(ren_ctx, threads, "spec_mat1", 18.75, Full_Ultra);
    run_image_test(ren_ctx, threads, "spec_mat2", 44.20, NoShadow);
    run_image_test(ren_ctx, threads, "spec_mat2", 35.05, NoGI);
    run_image_test(ren_ctx, threads, "spec_mat2", 28.60, NoDiffGI);
    run_image_test(ren_ctx, threads, "spec_mat2", 26.00, MedDiffGI);
    run_image_test(ren_ctx, threads, "spec_mat2", 25.55, Full);
    run_image_test(ren_ctx, threads, "spec_mat2", 25.45, Full_Ultra);
    run_image_test(ren_ctx, threads, "spec_mat3", 36.40, NoShadow);
    run_image_test(ren_ctx, threads, "spec_mat3", 34.00, NoGI);
    run_image_test(ren_ctx, threads, "spec_mat3", 29.00, NoDiffGI);
    run_image_test(ren_ctx, threads, "spec_mat3", 21.35, MedDiffGI);
    run_image_test(ren_ctx, threads, "spec_mat3", 19.20, Full);
    run_image_test(ren_ctx, threads, "spec_mat3", 21.70, Full_Ultra);
    run_image_test(ren_ctx, threads, "spec_mat4", 21.00, NoShadow);
    run_image_test(ren_ctx, threads, "spec_mat4", 21.10, NoGI);
    run_image_test(ren_ctx, threads, "spec_mat4", 21.95, NoDiffGI);
    run_image_test(ren_ctx, threads, "spec_mat4", 15.10, MedDiffGI);
    run_image_test(ren_ctx, threads, "spec_mat4", 14.25, Full);
    run_image_test(ren_ctx, threads, "spec_mat4", 14.35, Full_Ultra);
    run_image_test(ren_ctx, threads, "spec_mat5", 32.15, NoShadow);
    run_image_test(ren_ctx, threads, "spec_mat5", 30.30, NoGI);
    run_image_test(ren_ctx, threads, "spec_mat5", 28.85, NoDiffGI);
    run_image_test(ren_ctx, threads, "spec_mat5", 19.80, MedDiffGI);
    run_image_test(ren_ctx, threads, "spec_mat5", 18.15, Full);
    run_image_test(ren_ctx, threads, "spec_mat5", 18.30, Full_Ultra);

    puts(" ---------------");
    // metal material
    run_image_test(ren_ctx, threads, "metal_mat0", 32.65, NoShadow);
    run_image_test(ren_ctx, threads, "metal_mat0", 31.55, NoGI);
    run_image_test(ren_ctx, threads, "metal_mat0", 29.45, NoDiffGI);
    run_image_test(ren_ctx, threads, "metal_mat0", 26.50, MedDiffGI);
    run_image_test(ren_ctx, threads, "metal_mat0", 26.10, Full);
    run_image_test(ren_ctx, threads, "metal_mat0", 28.70, Full_Ultra);
    run_image_test(ren_ctx, threads, "metal_mat1", 23.60, NoShadow);
    run_image_test(ren_ctx, threads, "metal_mat1", 23.60, NoGI);
    run_image_test(ren_ctx, threads, "metal_mat1", 27.05, NoDiffGI);
    run_image_test(ren_ctx, threads, "metal_mat1", 24.85, MedDiffGI);
    run_image_test(ren_ctx, threads, "metal_mat1", 24.40, Full);
    run_image_test(ren_ctx, threads, "metal_mat1", 24.80, Full_Ultra);
    run_image_test(ren_ctx, threads, "metal_mat2", 37.55, NoShadow);
    run_image_test(ren_ctx, threads, "metal_mat2", 34.30, NoGI);
    run_image_test(ren_ctx, threads, "metal_mat2", 33.30, NoDiffGI);
    run_image_test(ren_ctx, threads, "metal_mat2", 31.10, MedDiffGI);
    run_image_test(ren_ctx, threads, "metal_mat2", 29.35, Full);
    run_image_test(ren_ctx, threads, "metal_mat2", 29.75, Full_Ultra);
    run_image_test(ren_ctx, threads, "metal_mat3", 38.05, NoShadow);
    run_image_test(ren_ctx, threads, "metal_mat3", 34.80, NoGI);
    run_image_test(ren_ctx, threads, "metal_mat3", 30.05, NoDiffGI);
    run_image_test(ren_ctx, threads, "metal_mat3", 22.85, MedDiffGI);
    run_image_test(ren_ctx, threads, "metal_mat3", 20.00, Full);
    run_image_test(ren_ctx, threads, "metal_mat3", 21.45, Full_Ultra);
    run_image_test(ren_ctx, threads, "metal_mat4", 25.25, NoShadow);
    run_image_test(ren_ctx, threads, "metal_mat4", 25.20, NoGI);
    run_image_test(ren_ctx, threads, "metal_mat4", 26.45, NoDiffGI);
    run_image_test(ren_ctx, threads, "metal_mat4", 19.70, MedDiffGI);
    run_image_test(ren_ctx, threads, "metal_mat4", 17.85, Full);
    run_image_test(ren_ctx, threads, "metal_mat4", 17.85, Full_Ultra);
    run_image_test(ren_ctx, threads, "metal_mat5", 37.30, NoShadow);
    run_image_test(ren_ctx, threads, "metal_mat5", 34.15, NoGI);
    run_image_test(ren_ctx, threads, "metal_mat5", 32.65, NoDiffGI);
    run_image_test(ren_ctx, threads, "metal_mat5", 24.45, MedDiffGI);
    run_image_test(ren_ctx, threads, "metal_mat5", 20.55, Full);
    run_image_test(ren_ctx, threads, "metal_mat5", 20.75, Full_Ultra);

    puts(" ---------------");
    // plastic material
    run_image_test(ren_ctx, threads, "plastic_mat0", 43.15, NoShadow);
    run_image_test(ren_ctx, threads, "plastic_mat0", 35.65, NoGI);
    run_image_test(ren_ctx, threads, "plastic_mat0", 32.60, NoDiffGI);
    run_image_test(ren_ctx, threads, "plastic_mat0", 28.55, MedDiffGI);
    run_image_test(ren_ctx, threads, "plastic_mat0", 26.80, Full);
    run_image_test(ren_ctx, threads, "plastic_mat0", 28.10, Full_Ultra);
    run_image_test(ren_ctx, threads, "plastic_mat1", 36.10, NoShadow);
    run_image_test(ren_ctx, threads, "plastic_mat1", 33.45, NoGI);
    run_image_test(ren_ctx, threads, "plastic_mat1", 27.90, NoDiffGI);
    run_image_test(ren_ctx, threads, "plastic_mat1", 24.10, MedDiffGI);
    run_image_test(ren_ctx, threads, "plastic_mat1", 23.20, Full);
    run_image_test(ren_ctx, threads, "plastic_mat1", 23.60, Full_Ultra);
    run_image_test(ren_ctx, threads, "plastic_mat2", 41.75, NoShadow);
    run_image_test(ren_ctx, threads, "plastic_mat2", 34.80, NoGI);
    run_image_test(ren_ctx, threads, "plastic_mat2", 33.20, NoDiffGI);
    run_image_test(ren_ctx, threads, "plastic_mat2", 28.90, MedDiffGI);
    run_image_test(ren_ctx, threads, "plastic_mat2", 26.35, Full);
    run_image_test(ren_ctx, threads, "plastic_mat2", 27.80, Full_Ultra);
    run_image_test(ren_ctx, threads, "plastic_mat3", 38.45, NoShadow);
    run_image_test(ren_ctx, threads, "plastic_mat3", 34.35, NoGI);
    run_image_test(ren_ctx, threads, "plastic_mat3", 31.75, NoDiffGI);
    run_image_test(ren_ctx, threads, "plastic_mat3", 24.55, MedDiffGI);
    run_image_test(ren_ctx, threads, "plastic_mat3", 21.25, Full);
    run_image_test(ren_ctx, threads, "plastic_mat3", 22.45, Full_Ultra);
    run_image_test(ren_ctx, threads, "plastic_mat4", 35.70, NoShadow);
    run_image_test(ren_ctx, threads, "plastic_mat4", 33.35, NoGI);
    run_image_test(ren_ctx, threads, "plastic_mat4", 28.30, NoDiffGI);
    run_image_test(ren_ctx, threads, "plastic_mat4", 21.85, MedDiffGI);
    run_image_test(ren_ctx, threads, "plastic_mat4", 19.75, Full);
    run_image_test(ren_ctx, threads, "plastic_mat4", 20.25, Full_Ultra);
    run_image_test(ren_ctx, threads, "plastic_mat5", 42.60, NoShadow);
    run_image_test(ren_ctx, threads, "plastic_mat5", 35.05, NoGI);
    run_image_test(ren_ctx, threads, "plastic_mat5", 30.40, NoDiffGI);
    run_image_test(ren_ctx, threads, "plastic_mat5", 25.35, MedDiffGI);
    run_image_test(ren_ctx, threads, "plastic_mat5", 20.95, Full);
    run_image_test(ren_ctx, threads, "plastic_mat5", 22.15, Full_Ultra);

    puts(" ---------------");
    // tint material
    run_image_test(ren_ctx, threads, "tint_mat0", 42.85, NoShadow);
    run_image_test(ren_ctx, threads, "tint_mat0", 35.65, NoGI);
    run_image_test(ren_ctx, threads, "tint_mat0", 34.40, NoDiffGI);
    run_image_test(ren_ctx, threads, "tint_mat0", 28.10, MedDiffGI);
    run_image_test(ren_ctx, threads, "tint_mat0", 26.20, Full);
    run_image_test(ren_ctx, threads, "tint_mat1", 32.25, NoShadow);
    run_image_test(ren_ctx, threads, "tint_mat1", 31.15, NoGI);
    run_image_test(ren_ctx, threads, "tint_mat1", 26.40, NoDiffGI);
    run_image_test(ren_ctx, threads, "tint_mat1", 22.75, MedDiffGI);
    run_image_test(ren_ctx, threads, "tint_mat1", 21.70, Full);
    run_image_test(ren_ctx, threads, "tint_mat2", 38.95, NoShadow);
    run_image_test(ren_ctx, threads, "tint_mat2", 34.25, NoGI);
    run_image_test(ren_ctx, threads, "tint_mat2", 33.85, NoDiffGI);
    run_image_test(ren_ctx, threads, "tint_mat2", 28.95, MedDiffGI);
    run_image_test(ren_ctx, threads, "tint_mat2", 26.50, Full);
    run_image_test(ren_ctx, threads, "tint_mat3", 44.25, NoShadow);
    run_image_test(ren_ctx, threads, "tint_mat3", 35.85, NoGI);
    run_image_test(ren_ctx, threads, "tint_mat3", 34.35, NoDiffGI);
    run_image_test(ren_ctx, threads, "tint_mat3", 24.90, MedDiffGI);
    run_image_test(ren_ctx, threads, "tint_mat3", 19.30, Full);
    run_image_test(ren_ctx, threads, "tint_mat4", 30.65, NoShadow);
    run_image_test(ren_ctx, threads, "tint_mat4", 30.10, NoGI);
    run_image_test(ren_ctx, threads, "tint_mat4", 26.15, NoDiffGI);
    run_image_test(ren_ctx, threads, "tint_mat4", 21.45, MedDiffGI);
    run_image_test(ren_ctx, threads, "tint_mat4", 17.05, Full);
    run_image_test(ren_ctx, threads, "tint_mat5", 40.10, NoShadow);
    run_image_test(ren_ctx, threads, "tint_mat5", 34.40, NoGI);
    run_image_test(ren_ctx, threads, "tint_mat5", 32.40, NoDiffGI);
    run_image_test(ren_ctx, threads, "tint_mat5", 23.80, MedDiffGI);
    run_image_test(ren_ctx, threads, "tint_mat5", 17.65, Full);

    puts(" ---------------");
    // clearcoat material
    run_image_test(ren_ctx, threads, "coat_mat0", 41.05, NoShadow);
    run_image_test(ren_ctx, threads, "coat_mat0", 35.75, NoGI);
    run_image_test(ren_ctx, threads, "coat_mat1", 31.85, NoShadow);
    run_image_test(ren_ctx, threads, "coat_mat1", 31.00, NoGI);
    run_image_test(ren_ctx, threads, "coat_mat2", 28.90, NoShadow);
    run_image_test(ren_ctx, threads, "coat_mat2", 28.50, NoGI);
    run_image_test(ren_ctx, threads, "coat_mat3", 38.75, NoShadow);
    run_image_test(ren_ctx, threads, "coat_mat3", 35.10, NoGI);
    run_image_test(ren_ctx, threads, "coat_mat4", 30.85, NoShadow);
    run_image_test(ren_ctx, threads, "coat_mat4", 30.55, NoGI);
    run_image_test(ren_ctx, threads, "coat_mat5", 28.00, NoShadow);
    run_image_test(ren_ctx, threads, "coat_mat5", 27.80, NoGI);

    puts(" ---------------");
    // alpha material
    run_image_test(ren_ctx, threads, "alpha_mat0", 37.10, NoShadow);
    run_image_test(ren_ctx, threads, "alpha_mat0", 30.50, NoGI);
    run_image_test(ren_ctx, threads, "alpha_mat0", 26.25, NoDiffGI);
    run_image_test(ren_ctx, threads, "alpha_mat0", 24.75, MedDiffGI);
    run_image_test(ren_ctx, threads, "alpha_mat0", 24.80, Full);
    run_image_test(ren_ctx, threads, "alpha_mat0", 25.35, Full_Ultra);
    run_image_test(ren_ctx, threads, "alpha_mat1", 38.25, NoShadow);
    run_image_test(ren_ctx, threads, "alpha_mat1", 31.85, NoGI);
    run_image_test(ren_ctx, threads, "alpha_mat1", 28.60, NoDiffGI);
    run_image_test(ren_ctx, threads, "alpha_mat1", 27.45, MedDiffGI);
    run_image_test(ren_ctx, threads, "alpha_mat1", 27.00, Full);
    run_image_test(ren_ctx, threads, "alpha_mat1", 27.75, Full_Ultra);
    run_image_test(ren_ctx, threads, "alpha_mat2", 40.90, NoShadow);
    run_image_test(ren_ctx, threads, "alpha_mat2", 36.30, NoGI);
    run_image_test(ren_ctx, threads, "alpha_mat2", 34.65, NoDiffGI);
    run_image_test(ren_ctx, threads, "alpha_mat2", 32.50, MedDiffGI);
    run_image_test(ren_ctx, threads, "alpha_mat2", 30.65, Full);
    run_image_test(ren_ctx, threads, "alpha_mat2", 31.80, Full_Ultra);
    run_image_test(ren_ctx, threads, "alpha_mat3", 45.15, NoShadow);
    run_image_test(ren_ctx, threads, "alpha_mat3", 36.70, NoGI);
    run_image_test(ren_ctx, threads, "alpha_mat3", 37.15, NoDiffGI);
    run_image_test(ren_ctx, threads, "alpha_mat3", 31.75, MedDiffGI);
    run_image_test(ren_ctx, threads, "alpha_mat3", 28.70, Full);
    run_image_test(ren_ctx, threads, "alpha_mat3", 29.55, Full_Ultra);
}
