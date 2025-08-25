#version 430 core
#extension GL_EXT_control_flow_attributes : require

#include "internal/_fs_common.glsl"
#include "internal/taa_common.glsl"
#include "blit_loading_interface.h"

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(location = 0) in vec2 g_vtx_uvs;

layout(location = 0) out vec4 g_out_color;

void main() {
    ivec2 uvs_px = ivec2(g_vtx_uvs);
    vec2 uv = -1.0 + 2.0 * g_vtx_uvs * g_params.texel_size;
    uv.x *= g_params.texel_size.y / g_params.texel_size.x;

    uv *= 0.5;

    // Created by inigo quilez - iq/2013 : https://www.shadertoy.com/view/4dl3zn
    // License Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported License.
    // Messed up by Weyland

    float iTime = g_params.time;

    vec3 color = vec3(0.0);
    for(int i = 0; i < 128; ++i) {
        float pha =      sin(float(i)*546.13+1.0)*0.5 + 0.5;
        float siz = pow( sin(float(i)*651.74+5.0)*0.5 + 0.5, 4.0 );
        float pox =      sin(float(i)*321.55+4.1) * g_params.texel_size.y / g_params.texel_size.x;
        float rad = 0.1+0.5*siz+sin(pha+siz)/4.0;
        vec2  pos = vec2( pox+sin(iTime/15.+pha+siz), -1.0-rad + (2.0+2.0*rad)*mod(pha+0.3*(iTime/7.)*(0.2+0.8*siz),1.0));
        float dis = length( uv - pos );
        vec3  col = mix( vec3(0.194*sin(iTime/6.0)+0.3,0.2,0.3*pha), vec3(1.1*sin(iTime/9.0)+0.3,0.2*pha,0.4), 0.5+0.5*sin(float(i)));
        float f = length(uv-pos)/rad;
        f = sqrt(clamp(1.0+(sin((iTime)*siz)*0.5)*f,0.0,1.0));
        color += col.zyx *(1.0-smoothstep( rad*0.15, rad, dis ));
    }
    color *= sqrt(1.5-0.5*length(uv));

    g_out_color = vec4(g_params.fade * color, 1.0);
}
