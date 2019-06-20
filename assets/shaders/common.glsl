
vec3 heatmap(float t) {
    vec3 r = vec3(t) * 2.1 - vec3(1.8, 1.14, 0.3);
    return vec3(1.0) - r * r;
}

highp float rand(highp vec2 co) {
    return fract(sin(dot(co.xy, vec2(12.9898, 78.233))) * 43758.5453);
}

vec3 RGBMDecode(vec4 rgbm) {
    return 4.0 * rgbm.rgb * rgbm.a;
}

vec3 FresnelSchlickRoughness(float cos_theta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cos_theta, 5.0);
}  

const vec2 poisson_disk[16] = vec2[16](
    vec2(-0.5, 0.0),
    vec2(0.0, 0.5),
    vec2(0.5, 0.0),
    vec2(0.0, -0.5),

    vec2(0.0, 0.0),
    vec2(-0.1, -0.32),
    vec2(0.17, 0.31),
    vec2(0.35, 0.04),
    
    vec2(0.07, 0.7),
    vec2(-0.72, 0.09),
    vec2(0.73, 0.05),
    vec2(0.1, -0.71),
    
    vec2(0.72, 0.8),
    vec2(-0.75, 0.74),
    vec2(-0.8, -0.73),
    vec2(0.75, -0.81)
);

#define M_PI 3.1415926535897932384626433832795

float _getShadowPCF5x5(sampler2DShadow shadow_texture, vec3 shadow_coord) {
    // http://the-witness.net/news/2013/09/shadow-mapping-summary-part-1/

    const vec2 shadow_size = vec2($ShadRes.0, $ShadRes.0 / 2.0);
    
    float z = shadow_coord.z;
    vec2 uv = shadow_coord.xy * shadow_size;
    vec2 shadowMapSizeInv = vec2(1.0) / shadow_size;
    vec2 base_uv = floor(uv + 0.5);
    float s = (uv.x + 0.5 - base_uv.x);
    float t = (uv.y + 0.5 - base_uv.y);
    base_uv -= vec2(0.5);
    base_uv *= shadowMapSizeInv;


    float uw0 = (4.0 - 3.0 * s);
    float uw1 = 7.0;
    float uw2 = (1.0 + 3.0 * s);

    float u0 = (3.0 - 2.0 * s) / uw0 - 2.0;
    float u1 = (3.0 + s) / uw1;
    float u2 = s / uw2 + 2.0;

    float vw0 = (4.0 - 3.0 * t);
    float vw1 = 7.0;
    float vw2 = (1.0 + 3.0 * t);

    float v0 = (3.0 - 2.0 * t) / vw0 - 2.0;
    float v1 = (3.0 + t) / vw1;
    float v2 = t / vw2 + 2.0;

    float sum = 0.0;

    u0 = u0 * shadowMapSizeInv.x + base_uv.x;
    v0 = v0 * shadowMapSizeInv.y + base_uv.y;

    u1 = u1 * shadowMapSizeInv.x + base_uv.x;
    v1 = v1 * shadowMapSizeInv.y + base_uv.y;

    u2 = u2 * shadowMapSizeInv.x + base_uv.x;
    v2 = v2 * shadowMapSizeInv.y + base_uv.y;

    sum += uw0 * vw0 * texture(shadow_texture, vec3(u0, v0, z));
    sum += uw1 * vw0 * texture(shadow_texture, vec3(u1, v0, z));
    sum += uw2 * vw0 * texture(shadow_texture, vec3(u2, v0, z));

    sum += uw0 * vw1 * texture(shadow_texture, vec3(u0, v1, z));
    sum += uw1 * vw1 * texture(shadow_texture, vec3(u1, v1, z));
    sum += uw2 * vw1 * texture(shadow_texture, vec3(u2, v1, z));

    sum += uw0 * vw2 * texture(shadow_texture, vec3(u0, v2, z));
    sum += uw1 * vw2 * texture(shadow_texture, vec3(u1, v2, z));
    sum += uw2 * vw2 * texture(shadow_texture, vec3(u2, v2, z));

    sum *= 1.0f / 144.0;

    return sum;
}

float GetSunVisibility(float frag_depth, sampler2DShadow shadow_texture, vec3 aVertexShUVs[4]) {
    const vec2 shadow_softness = vec2(3.0 / $ShadRes.0, 1.5 / $ShadRes.0);
    
    float visibility = 0.0;

    highp float r = M_PI * (-1.0 + 2.0 * rand(gl_FragCoord.xy));
    highp vec2 rx = vec2(cos(r), sin(r));
    highp vec2 ry = vec2(rx.y, -rx.x);
    
    if (frag_depth < $ShadCasc0Dist) {
        /*const highp float weight = 1.0 / $ShadCasc0Samp.0;
        for (int i = 0; i < $ShadCasc0Samp; i++) {
            visibility += texture(shadow_texture, aVertexShUVs[0] + vec3((rx * poisson_disk[i].x + ry * poisson_disk[i].y) * shadow_softness, 0.0));
        }
        visibility *= weight;*/
        
        visibility = _getShadowPCF5x5(shadow_texture, aVertexShUVs[0]);
    } else if (frag_depth < $ShadCasc1Dist) {
        /*const highp float weight = 1.0 / $ShadCasc1Samp.0;
        for (int i = 0; i < $ShadCasc1Samp; i++) {
            visibility += texture(shadow_texture, aVertexShUVs[1] + vec3((rx * poisson_disk[i].x + ry * poisson_disk[i].y) * shadow_softness, 0.0));
        }
        visibility *= weight;*/
        
        visibility = _getShadowPCF5x5(shadow_texture, aVertexShUVs[1]);
    } else if (frag_depth < $ShadCasc2Dist) {
        /*const highp float weight = 1.0 / $ShadCasc2Samp.0;
        for (int i = 0; i < $ShadCasc2Samp; i++) {
            visibility += texture(shadow_texture, aVertexShUVs[2] + vec3((rx * poisson_disk[i].x + ry * poisson_disk[i].y) * shadow_softness, 0.0));
        }
        visibility *= weight;*/
        
        visibility = _getShadowPCF5x5(shadow_texture, aVertexShUVs[2]);
    } else if (frag_depth < $ShadCasc3Dist) {
        /*const highp float weight = 1.0 / $ShadCasc3Samp.0;
        for (int i = 0; i < $ShadCasc3Samp; i++) {
            visibility += texture(shadow_texture, aVertexShUVs[3] + vec3((rx * poisson_disk[i].x + ry * poisson_disk[i].y) * shadow_softness, 0.0));
        }
        */
        visibility = _getShadowPCF5x5(shadow_texture, aVertexShUVs[3]);
        
        float t = smoothstep(0.95 * $ShadCasc3Dist, $ShadCasc3Dist, frag_depth);
        visibility = mix(visibility, 1.0, t);
    } else {
        // use direct sun lightmap?
        visibility = 1.0;
    }
    
    return visibility;
}
