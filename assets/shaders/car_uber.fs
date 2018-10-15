#ifdef GL_ES
    precision lowp float;
#endif

/*
UNIFORMS
    base_color1 : 3
    base_color2 : 4
    reflectivity : 5
    ambient_light : 6   
    sun_color : 7
    sun_dir : 8
    env_cubemap : 9
    env_cubemap_pos : 10
    fresnel_off : 11
    fresnel_pow : 12
    metallic : 13
    diffuse_texture : 14
*/

varying vec3 aVertexPosition_;
varying vec2 aVertexUVs_;
varying vec3 aVertexNormal_;

uniform vec3 base_color1;
uniform vec3 base_color2;

uniform float reflectivity;
uniform float fresnel_off;
uniform float fresnel_pow;
uniform float metallic;

uniform vec3 ambient_light;

uniform vec3 sun_color;
uniform vec3 sun_dir;

uniform sampler2D diffuse_texture;
uniform samplerCube env_cubemap;
uniform vec3 env_cubemap_pos;

void main(void) {
    float k = aVertexNormal_.z * aVertexNormal_.z;
    float fresnel = clamp(fresnel_off + pow((1.0 - k), fresnel_pow), 0.0, 1.0);

    vec4 diffuse_col = texture2D(diffuse_texture, aVertexUVs_);
    diffuse_col.rgb *= mix(base_color1, base_color2, k);
    vec3 direct_light = clamp(dot(aVertexNormal_, sun_dir), 0.0, 1.0) * sun_color;
    diffuse_col.rgb *= (direct_light + ambient_light);

    vec3 I = normalize(aVertexPosition_ - env_cubemap_pos);
    vec3 R = reflect(I, aVertexNormal_);
    vec4 reflection_col = textureCube(env_cubemap, R);
    reflection_col.rgb *= mix(vec3(1.0), base_color1, metallic);

    gl_FragColor = mix(diffuse_col, reflection_col, fresnel * reflectivity);
    //gl_FragColor.rgb = vec3(fresnel * reflectivity);
    //gl_FragColor = reflection_col * 0.001 + 0.001*diffuse_col + texture2D(diffuse_texture, aVertexUVs_);
}

