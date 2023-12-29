#ifndef _PRINCIPLED_GLSL
#define _PRINCIPLED_GLSL

float fresnel_dielectric_cos(float cosi, float eta) {
    // compute fresnel reflectance without explicitly computing the refracted direction
    float c = abs(cosi);
    float g = eta * eta - 1 + c * c;
    float result;

    if (g > 0) {
        g = sqrt(g);
        float A = (g - c) / (g + c);
        float B = (c * (g + c) - 1) / (c * (g - c) + 1);
        result = 0.5 * A * A * (1 + B * B);
    } else {
        result = 1.0; // TIR (no refracted component)
    }

    return result;
}

void get_lobe_weights(float base_color_lum, float spec_color_lum, float specular,
                      float metallic, float transmission, float clearcoat, out float out_diffuse_weight,
                      out float out_specular_weight, out float out_clearcoat_weight, out float out_refraction_weight) {
    // taken from Cycles
    out_diffuse_weight = base_color_lum * (1.0 - metallic) * (1.0 - transmission);
    float final_transmission = transmission * (1.0 - metallic);
    out_specular_weight =
        (specular != 0.0 || metallic != 0.0) ? spec_color_lum * (1.0 - final_transmission) : 0.0;
    out_clearcoat_weight = 0.25 * clearcoat * (1.0 - metallic);
    out_refraction_weight = final_transmission * base_color_lum;

    float total_weight =
        out_diffuse_weight + out_specular_weight + out_clearcoat_weight + out_refraction_weight;
    if (total_weight != 0.0) {
        out_diffuse_weight /= total_weight;
        out_specular_weight /= total_weight;
        out_clearcoat_weight /= total_weight;
        out_refraction_weight /= total_weight;
    }
}

#endif // _PRINCIPLED_GLSL