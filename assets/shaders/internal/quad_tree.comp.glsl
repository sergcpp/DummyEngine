#version 310 es

layout(binding = 0, rgba8) uniform readonly image2D UpperMipImage;
layout(binding = 1, rgba8) uniform writeonly image2D LowerMipImage;

layout(location = 0) uniform highp ivec4 uRes;

layout (local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

vec4 min3(vec4 v0, vec4 v1, vec4 v2) {
    return min(v0, min(v1, v2));
}

void main() {
    ivec2 icoord = ivec2(gl_GlobalInvocationID.xy);
    if (icoord.x >= uRes.x || icoord.y >= uRes.y) return;

    ivec2 icoord_hi = 2 * icoord;

    // fetch quad of pixels from upper level
    vec4 c[4];
    c[0] = imageLoad(UpperMipImage, icoord_hi + ivec2(0, 0));
    c[1] = imageLoad(UpperMipImage, icoord_hi + ivec2(1, 0));
    c[2] = imageLoad(UpperMipImage, icoord_hi + ivec2(0, 1));
    c[3] = imageLoad(UpperMipImage, icoord_hi + ivec2(1, 1));

    const ivec2 quadrants[4] = ivec2[4](
        ivec2(-1, -1), ivec2(+1, -1),
        ivec2(-1, +1), ivec2(+1, +1)
    );

    // check result of bilinear filtering of quad with neighbours
    for (int q = 0; q < 4; q++) {
        int i0 = (icoord_hi.x + 2 * quadrants[q].x + uRes.z) % uRes.z;
        int i1 = (icoord_hi.x + quadrants[q].x + uRes.z) % uRes.z;
        int j0 = (icoord_hi.y + quadrants[q].y + uRes.w) % uRes.w;
        int j1 = (icoord_hi.y + 2 * quadrants[q].y + uRes.w) % uRes.w;

        vec4 c0 = imageLoad(UpperMipImage, ivec2(i0, icoord_hi.y));
        vec4 c1 = imageLoad(UpperMipImage, ivec2(i1, icoord_hi.y));
        vec4 c2 = imageLoad(UpperMipImage, ivec2(i0, j0));
        vec4 c3 = imageLoad(UpperMipImage, ivec2(icoord_hi.x, j0));
        vec4 c4 = imageLoad(UpperMipImage, ivec2(icoord_hi.x, j1));

        c[q] = min(min3((c[q] + c0) * 0.5, (c[q] + c1) * 0.5, (c[q] + c2) * 0.5),
                   min3((c[q] + c3) * 0.5, (c[q] + c4) * 0.5, c[q]));
    }

    imageStore(LowerMipImage, icoord, min(min(c[0], c[1]), min(c[2], c[3])));
}

