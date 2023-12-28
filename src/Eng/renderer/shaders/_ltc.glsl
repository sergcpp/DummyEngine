#ifndef _LTC_GLSL
#define _LTC_GLSL

///////////////////////////////
// Linearly Transformed Cosines
///////////////////////////////

const float LTC_LUT_SIZE  = 64.0;
const float LTC_LUT_SCALE = (LTC_LUT_SIZE - 1.0) / LTC_LUT_SIZE;
const float LTC_LUT_BIAS  = 0.5 / LTC_LUT_SIZE;

const float LTC_LUT_MIN_ROUGHNESS = 0.01;

//
// Rectangular light
//

#define CLIPLESS_APPROXIMATION 1

vec3 IntegrateEdgeVec(vec3 v1, vec3 v2) {
    float x = dot(v1, v2);
    float y = abs(x);

    float a = 0.8543985 + (0.4965155 + 0.0145206 * y) * y;
    float b = 3.4175940 + (4.1616724 + y) * y;
    float v = a / b;

    float theta_sintheta = (x > 0.0) ? v : 0.5 * inversesqrt(max(1.0 - x * x, 1e-7)) - v;

    return cross(v1, v2) * theta_sintheta;
}

float IntegrateEdge(vec3 v1, vec3 v2) {
    return IntegrateEdgeVec(v1, v2).z;
}

void ClipQuadToHorizon(inout vec3 L[5], out int n) {
    // detect clipping config
    int config = 0;
    if (L[0].z > 0.0) config += 1;
    if (L[1].z > 0.0) config += 2;
    if (L[2].z > 0.0) config += 4;
    if (L[3].z > 0.0) config += 8;

    // clip
    n = 0;

    if (config == 0) {
        // clip all
    } else if (config == 1) { // V1 clip V2 V3 V4
        n = 3;
        L[1] = -L[1].z * L[0] + L[0].z * L[1];
        L[2] = -L[3].z * L[0] + L[0].z * L[3];
    } else if (config == 2) { // V2 clip V1 V3 V4
        n = 3;
        L[0] = -L[0].z * L[1] + L[1].z * L[0];
        L[2] = -L[2].z * L[1] + L[1].z * L[2];
    } else if (config == 3) { // V1 V2 clip V3 V4
        n = 4;
        L[2] = -L[2].z * L[1] + L[1].z * L[2];
        L[3] = -L[3].z * L[0] + L[0].z * L[3];
    } else if (config == 4) { // V3 clip V1 V2 V4
        n = 3;
        L[0] = -L[3].z * L[2] + L[2].z * L[3];
        L[1] = -L[1].z * L[2] + L[2].z * L[1];
    } else if (config == 5) { // V1 V3 clip V2 V4) impossible
        n = 0;
    } else if (config == 6) { // V2 V3 clip V1 V4
        n = 4;
        L[0] = -L[0].z * L[1] + L[1].z * L[0];
        L[3] = -L[3].z * L[2] + L[2].z * L[3];
    } else if (config == 7) { // V1 V2 V3 clip V4
        n = 5;
        L[4] = -L[3].z * L[0] + L[0].z * L[3];
        L[3] = -L[3].z * L[2] + L[2].z * L[3];
    } else if (config == 8) { // V4 clip V1 V2 V3
        n = 3;
        L[0] = -L[0].z * L[3] + L[3].z * L[0];
        L[1] = -L[2].z * L[3] + L[3].z * L[2];
        L[2] =  L[3];
    } else if (config == 9) { // V1 V4 clip V2 V3
        n = 4;
        L[1] = -L[1].z * L[0] + L[0].z * L[1];
        L[2] = -L[2].z * L[3] + L[3].z * L[2];
    } else if (config == 10) { // V2 V4 clip V1 V3) impossible
        n = 0;
    } else if (config == 11) { // V1 V2 V4 clip V3
        n = 5;
        L[4] = L[3];
        L[3] = -L[2].z * L[3] + L[3].z * L[2];
        L[2] = -L[2].z * L[1] + L[1].z * L[2];
    } else if (config == 12) { // V3 V4 clip V1 V2
        n = 4;
        L[1] = -L[1].z * L[2] + L[2].z * L[1];
        L[0] = -L[0].z * L[3] + L[3].z * L[0];
    } else if (config == 13) { // V1 V3 V4 clip V2
        n = 5;
        L[4] = L[3];
        L[3] = L[2];
        L[2] = -L[1].z * L[2] + L[2].z * L[1];
        L[1] = -L[1].z * L[0] + L[0].z * L[1];
    } else if (config == 14) { // V2 V3 V4 clip V1
        n = 5;
        L[4] = -L[0].z * L[3] + L[3].z * L[0];
        L[0] = -L[0].z * L[1] + L[1].z * L[0];
    } else if (config == 15) { // V1 V2 V3 V4
        n = 4;
    }

    if (n == 3) {
        L[3] = L[0];
    }
    if (n == 4) {
        L[4] = L[0];
    }
}

vec2 LTC_Coords(float cosTheta, float roughness) {
    float theta = sqrt(1.0 - saturate(cosTheta));
    vec2 coords = vec2(max(roughness, LTC_LUT_MIN_ROUGHNESS), theta);

    // scale and bias coordinates, for correct filtered lookup
    coords = coords * LTC_LUT_SCALE + LTC_LUT_BIAS;

    return coords;
}

vec3 LTC_Evaluate_Rect(sampler2D ltc_2, vec3 N, vec3 V, vec3 P, vec4 t1_fetch, vec3 points[4], bool two_sided) {
    // construct orthonormal basis around N
    vec3 T1, T2;
    T1 = normalize(V - N * dot(V, N));
    T2 = cross(N, T1);

    mat3 Minv = mat3(
        vec3(t1_fetch.x,       0.0, t1_fetch.y),
        vec3(      0.0,        1.0,        0.0),
        vec3(t1_fetch.z,       0.0, t1_fetch.w)
    );

    // rotate area light in (T1, T2, N) basis
    Minv = Minv * transpose(mat3(T1, T2, N));

    // polygon (allocate 5 vertices for clipping)
    vec3 L[5];
    L[0] = Minv * (points[0] - P);
    L[1] = Minv * (points[1] - P);
    L[2] = Minv * (points[2] - P);
    L[3] = Minv * (points[3] - P);

    // integrate
    float sum = 0.0;

#if CLIPLESS_APPROXIMATION
    vec3 dir = points[0].xyz - P;
    vec3 lightNormal = cross(points[1] - points[0], points[3] - points[0]);
    bool behind = (dot(dir, lightNormal) < 0.0);

    if (L[0].z < 0.0 && L[1].z < 0.0 && L[2].z < 0.0 && L[3].z < 0.0) {
        return vec3(0.0);
    }

    L[0] = normalize(L[0]);
    L[1] = normalize(L[1]);
    L[2] = normalize(L[2]);
    L[3] = normalize(L[3]);

    vec3 vsum = vec3(0.0);

    vsum += IntegrateEdgeVec(L[0], L[1]);
    vsum += IntegrateEdgeVec(L[1], L[2]);
    vsum += IntegrateEdgeVec(L[2], L[3]);
    vsum += IntegrateEdgeVec(L[3], L[0]);

    float len = length(vsum);
    float z = vsum.z / len;

    if (behind) {
        z = -z;
    }

    vec2 uv = vec2(z * 0.5 + 0.5, len);
    uv = uv * LTC_LUT_SCALE + LTC_LUT_BIAS;

    float scale = textureLod(ltc_2, uv, 0.0).w;

    sum = len * scale;

    if (behind && !two_sided) {
        sum = 0.0;
    }
#else
    int n;
    ClipQuadToHorizon(L, n);

    if (n == 0) {
        return vec3(0, 0, 0);
    }
    // project onto sphere
    L[0] = normalize(L[0]);
    L[1] = normalize(L[1]);
    L[2] = normalize(L[2]);
    L[3] = normalize(L[3]);
    L[4] = normalize(L[4]);

    // integrate
    sum += IntegrateEdge(L[0], L[1]);
    sum += IntegrateEdge(L[1], L[2]);
    sum += IntegrateEdge(L[2], L[3]);
    if (n >= 4) {
        sum += IntegrateEdge(L[3], L[4]);
    }
    if (n == 5) {
        sum += IntegrateEdge(L[4], L[0]);
    }
    sum = two_sided ? abs(sum) : max(0.0, sum);
#endif

    return vec3(sum, sum, sum);
}

//
// Disk light
//

// An extended version of the implementation from
// "How to solve a cubic equation, revisited"
// http://momentsingraphics.de/?p=105
vec3 SolveCubic(vec4 Coefficient) {
    // Normalize the polynomial
    Coefficient.xyz /= Coefficient.w;
    // Divide middle coefficients by three
    Coefficient.yz /= 3.0;

    float A = Coefficient.w;
    float B = Coefficient.z;
    float C = Coefficient.y;
    float D = Coefficient.x;

    // Compute the Hessian and the discriminant
    vec3 Delta = vec3(
        -Coefficient.z * Coefficient.z + Coefficient.y,
        -Coefficient.y * Coefficient.z + Coefficient.x,
        dot(vec2(Coefficient.z, -Coefficient.y), Coefficient.xy)
    );

    float Discriminant = max(dot(vec2(4.0 * Delta.x, -Delta.y), Delta.zy), 0.0);

    vec3 RootsA, RootsD;

    vec2 xlc, xsc;

    { // Algorithm A
        float A_a = 1.0;
        float C_a = Delta.x;
        float D_a = -2.0 * B * Delta.x + Delta.y;

        // Take the cubic root of a normalized complex number
        float Theta = atan(sqrt(Discriminant), -D_a) / 3.0;

        float x_1a = 2.0 * sqrt(-C_a) * cos(Theta);
        float x_3a = 2.0 * sqrt(-C_a) * cos(Theta + (2.0 / 3.0) * M_PI);

        float xl;
        if ((x_1a + x_3a) > 2.0 * B) {
            xl = x_1a;
        } else {
            xl = x_3a;
        }
        xlc = vec2(xl - B, A);
    }

    { // Algorithm D
        float A_d = D;
        float C_d = Delta.z;
        float D_d = -D * Delta.y + 2.0 * C * Delta.z;

        // Take the cubic root of a normalized complex number
        float Theta = atan(D * sqrt(Discriminant), -D_d) / 3.0;

        float x_1d = 2.0 * sqrt(-C_d) * cos(Theta);
        float x_3d = 2.0 * sqrt(-C_d) * cos(Theta + (2.0 / 3.0) * M_PI);

        float xs;
        if (x_1d + x_3d < 2.0 * C) {
            xs = x_1d;
        } else {
            xs = x_3d;
        }
        xsc = vec2(-D, xs + C);
    }

    float E =  xlc.y * xsc.y;
    float F = -xlc.x * xsc.y - xlc.y * xsc.x;
    float G =  xlc.x * xsc.x;

    vec2 xmc = vec2(C * F - B * G, -B * F + C * E);

    vec3 Root = vec3(xsc.x / xsc.y, xmc.x / xmc.y, xlc.x / xlc.y);

    if (Root.x < Root.y && Root.x < Root.z) {
        Root.xyz = Root.yxz;
    } else if (Root.z < Root.x && Root.z < Root.y) {
        Root.xyz = Root.xzy;
    }
    return Root;
}

vec3 LTC_Evaluate_Disk(sampler2D ltc_2, vec3 N, vec3 V, vec3 P, vec4 t1_fetch, vec3 points[4], bool two_sided) {
    // construct orthonormal basis around N
    vec3 T1, T2;
    T1 = normalize(V - N * dot(V, N));
    T2 = cross(N, T1);

    // rotate area light in (T1, T2, N) basis
    mat3 R = transpose(mat3(T1, T2, N));

    // polygon (allocate 3 vertices for clipping)
    vec3 L_[3];
    L_[0] = R * (points[0] - P);
    L_[1] = R * (points[1] - P);
    L_[2] = R * (points[2] - P);

    if (L_[0].z < -0.25 && L_[1].z < -0.25 && L_[2].z < -0.25) {
        return vec3(0.0);
    }

    // init ellipse
    vec3 C  = 0.5 * (L_[0] + L_[2]);
    vec3 V1 = 0.5 * (L_[1] - L_[2]);
    vec3 V2 = 0.5 * (L_[1] - L_[0]);

    mat3 Minv = mat3(
        vec3(t1_fetch.x,       0.0, t1_fetch.y),
        vec3(      0.0,        1.0,        0.0),
        vec3(t1_fetch.z,       0.0, t1_fetch.w)
    );

    C  = Minv * C;
    V1 = Minv * V1;
    V2 = Minv * V2;

    if(!two_sided && dot(cross(V1, V2), C) < 0.0) {
        return vec3(0.0);
    }

    // compute eigenvectors of ellipse
    float a, b;
    float d11 = dot(V1, V1);
    float d22 = dot(V2, V2);
    float d12 = dot(V1, V2);
    if (abs(d12) / sqrt(d11 * d22) > 0.001) {
        float tr = d11 + d22;
        float det = max(-d12 * d12 + d11 * d22, 0.0);

        // use sqrt matrix to solve for eigenvalues
        det = sqrt(det);
        float u = 0.5 * sqrt(tr - 2.0 * det);
        float v = 0.5 * sqrt(tr + 2.0 * det);
        float e_max = (u + v) * (u + v);
        float e_min = (u - v) * (u - v);

        vec3 V1_, V2_;
        if (d11 > d22) {
            V1_ = d12 * V1 + (e_max - d11) * V2;
            V2_ = d12 * V1 + (e_min - d11) * V2;
        } else {
            V1_ = d12 * V2 + (e_max - d22) * V1;
            V2_ = d12 * V2 + (e_min - d22) * V1;
        }

        a = 1.0 / e_max;
        b = 1.0 / e_min;
        V1 = normalize(V1_);
        V2 = normalize(V2_);
    } else {
        a = 1.0 / dot(V1, V1);
        b = 1.0 / dot(V2, V2);
        V1 *= sqrt(a);
        V2 *= sqrt(b);
    }

    vec3 V3 = cross(V1, V2);
    if (dot(C, V3) < 0.0) {
        V3 *= -1.0;
    }

    float L  = dot(V3, C);
    float x0 = dot(V1, C) / L;
    float y0 = dot(V2, C) / L;

    float E1 = inversesqrt(a);
    float E2 = inversesqrt(b);

    a *= L * L;
    b *= L * L;

    float c0 = a * b;
    float c1 = a * b * (1.0 + x0 * x0 + y0 * y0) - a - b;
    float c2 = 1.0 - a * (1.0 + x0 * x0) - b * (1.0 + y0 * y0);
    float c3 = 1.0;

    vec3 roots = SolveCubic(vec4(c0, c1, c2, c3));
    float e1 = roots.x;
    float e2 = roots.y;
    float e3 = roots.z;

    vec3 avgDir = vec3(a * x0 / (a - e2), b * y0 / (b - e2), 1.0);

    mat3 rotate = mat3(V1, V2, V3);

    avgDir = rotate * avgDir;
    avgDir = normalize(avgDir);

    float L1 = sqrt(max(-e2 / e3, 0.0));
    float L2 = sqrt(max(-e2 / e1, 0.0));

    float formFactor = L1 * L2 * inversesqrt((1.0 + L1 * L1) * (1.0 + L2 * L2));

    // use tabulated horizon-clipped sphere
    vec2 uv = vec2(avgDir.z * 0.5 + 0.5, formFactor);
    uv = uv * LTC_LUT_SCALE + LTC_LUT_BIAS;

    float scale = textureLod(ltc_2, uv, 0.0).w;

    float spec = formFactor * scale;

    return vec3(spec, spec, spec);
}

//
// Line light
//

float Fpo(float d, float l) {
    return l / (d * (d * d + l * l)) + atan(l/d) / (d*d);
}

float Fwt(float d, float l) {
    return l * l / (d * (d * d + l * l));
}

float I_diffuse_line(vec3 p1, vec3 p2) {
    // tangent
    vec3 wt = normalize(p2 - p1);

    // clamping
    if (p1.z <= 0.0 && p2.z <= 0.0) {
        return 0.0;
    }
    if (p1.z < 0.0) {
        p1 = (+p1 * p2.z - p2 * p1.z) / (+p2.z - p1.z);
    }
    if (p2.z < 0.0) {
        p2 = (-p1 * p2.z + p2 * p1.z) / (-p2.z + p1.z);
    }

    // parameterization
    float l1 = dot(p1, wt);
    float l2 = dot(p2, wt);

    // shading point orthonormal projection on the line
    vec3 po = p1 - l1 * wt;

    // distance to line
    float d = length(po);

    // integral
    float I = (Fpo(d, l2) - Fpo(d, l1)) * po.z + (Fwt(d, l2) - Fwt(d, l1)) * wt.z;
    return I / M_PI;
}

float I_ltc_line(vec3 p1, vec3 p2, mat3 Minv) {
    // transform to diffuse configuration
    vec3 p1o = Minv * p1;
    vec3 p2o = Minv * p2;
    float I_diffuse = I_diffuse_line(p1o, p2o);

    // width factor
    vec3 ortho = normalize(cross(p1, p2));
    float w =  1.0 / length(inverse(transpose(Minv)) * ortho);

    return w * I_diffuse;
}

vec3 LTC_Evaluate_Line(vec3 N, vec3 V, vec3 P, vec4 t1_fetch, vec3 points[2], float radius) {
    // construct orthonormal basis around N
    vec3 T1, T2;
    T1 = normalize(V - N * dot(V, N));
    T2 = cross(N, T1);

    mat3 B = transpose(mat3(T1, T2, N));

    vec3 p1 = B * (points[0] - P);
    vec3 p2 = B * (points[1] - P);

    mat3 Minv = mat3(
        vec3(t1_fetch.x,       0.0, t1_fetch.y),
        vec3(      0.0,        1.0,        0.0),
        vec3(t1_fetch.z,       0.0, t1_fetch.w)
    );

    float Iline = radius * I_ltc_line(p1, p2, Minv);
    return vec3(clamp(Iline, 0.0, 1.0));
}

#endif // _LTC_GLSL