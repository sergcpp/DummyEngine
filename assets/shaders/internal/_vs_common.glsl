
#include "_common.glsl"

#if !defined(VULKAN)
#define gl_InstanceIndex gl_InstanceID
#endif

#define INSTANCE_BUF_STRIDE 8

#define FetchModelMatrix(instance_buf, instance)                                    \
    transpose(mat4(texelFetch((instance_buf), (instance) * INSTANCE_BUF_STRIDE + 0),\
                   texelFetch((instance_buf), (instance) * INSTANCE_BUF_STRIDE + 1),\
                   texelFetch((instance_buf), (instance) * INSTANCE_BUF_STRIDE + 2),\
                   vec4(0.0, 0.0, 0.0, 1.0)))

#define FetchNormalMatrix(instance_buf, instance)                                   \
    mat4(texelFetch((instance_buf), (instance) * INSTANCE_BUF_STRIDE + 4),          \
         texelFetch((instance_buf), (instance) * INSTANCE_BUF_STRIDE + 5),          \
         texelFetch((instance_buf), (instance) * INSTANCE_BUF_STRIDE + 6),          \
         vec4(0.0, 0.0, 0.0, 1.0))

#define VEGE_MAX_MOVEMENT 8.0
#define VEGE_MAX_BRANCH_AMPLITUDE 1.0
#define VEGE_MAX_LEAVES_AMPLITUDE 0.2

#define VEGE_NOISE_SCALE_LF (1.0 / 256.0)
#define VEGE_NOISE_SCALE_HF (1.0 / 8.0)

vec3 TransformVegetation(in vec3 vtx_pos_ls, vec4 vtx_color, vec4 wind_scroll, vec4 wind_params, vec4 wind_vec_ls, sampler2D _noise_texture) {
    {   // Branch/detail bending
        const float scale = 4.0;

        vec3 pivot_dir = scale * (2.0 * vtx_color.xyz - vec3(1.0));
        float leaf_flex = VEGE_MAX_LEAVES_AMPLITUDE * clamp(vtx_color.w - 0.01, 0.0, 1.0);

        float movement_scale = VEGE_MAX_MOVEMENT * wind_params.x;
        float bend_scale = 0.005 * wind_params.z;
        float stretch = wind_params.w;

        vec3 pivot_pos_ls = vtx_pos_ls + pivot_dir;
        float dist = length(pivot_dir);
        float branch_flex = VEGE_MAX_BRANCH_AMPLITUDE * step(0.005, vtx_color.w) * dist / scale;

        mat2 uv_tr;
        uv_tr[0] = normalize(vec2(wind_vec_ls.x, wind_vec_ls.z));
        uv_tr[1] = vec2(uv_tr[0].y, -uv_tr[0].x);

        vec2 vtx_pos_ts = uv_tr * vtx_pos_ls.xz + vec2(2.0 * pivot_pos_ls.y, 0.0);

        vec3 noise_dir_lf = wind_vec_ls.w * texture(_noise_texture, wind_scroll.xy + VEGE_NOISE_SCALE_LF * vtx_pos_ts).rgb;
        vec3 noise_dir_hf = wind_vec_ls.w * texture(_noise_texture, wind_scroll.zw + VEGE_NOISE_SCALE_HF * vtx_pos_ts).rgb;

        vec3 new_pos_ls = vtx_pos_ls + movement_scale * (branch_flex * (bend_scale * wind_vec_ls.xyz + noise_dir_lf) + leaf_flex * noise_dir_hf);
        vtx_pos_ls = mix(pivot_pos_ls + normalize(new_pos_ls - pivot_pos_ls) * dist, new_pos_ls, stretch);
    }

    {   // Trunk bending
        const float bend_scale = 0.0015;
        float tree_mode = wind_params.y;
        float bend_factor = vtx_pos_ls.y * tree_mode * bend_scale;
        // smooth bending factor and increase its nearby height limit
        bend_factor += 1.0;
        bend_factor *= bend_factor;
        bend_factor = bend_factor * bend_factor - bend_factor;
        // displace position
        vec3 new_pos_ls = vtx_pos_ls;
        new_pos_ls.xz += wind_vec_ls.xz * bend_factor;
        // rescale
        vtx_pos_ls = normalize(new_pos_ls) * length(vtx_pos_ls);
    }

    return vtx_pos_ls;
}

