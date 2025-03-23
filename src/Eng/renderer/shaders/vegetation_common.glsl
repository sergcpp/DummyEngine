#ifndef _VEGETATION_GLSL
#define _VEGETATION_GLSL

#define PP_RES_X 32
#define PP_RES_Y 64

#define PP_EXTENT 20.48

#define MAX_ALLOWED_HIERARCHY 4

#define VEGE_NOISE_SCALE_LF (1.0 / 1536.0)
#define VEGE_NOISE_SCALE_HF (1.0 / 8.0)

const float WindNoiseSpeedBase = 0.1;
const float WindNoiseSpeedLevelMul = 1.666;

const float WindAngleRotBase = 5;
const float WindAngleRotLevelMul = 10;
const float WindAngleRotMax = 60;

const float WindParentContrib = 0.25;

const float WindMotionDampBase = 3;
const float WindMotionDampLevelMul = 0.8;

vec2 get_uv_by_index(uint index) {
    vec2 idx2D = vec2(index % PP_RES_X, index / PP_RES_X);

    const vec2 invRes = 1.0 / vec2(PP_RES_X, PP_RES_Y);
    return (idx2D + 0.5) * invRes;
}

int get_index_by_uv(vec2 uv) {
    ivec2 idx2D = ivec2(floor(uv * ivec2(PP_RES_X, PP_RES_Y)));

    return idx2D[0] + idx2D[1] * PP_RES_X;
}

int get_parent_index_and_pivot_pos(sampler2D pp_pos_tex, vec2 coords, out vec3 pivot_pos) {
    vec4 result = textureLod(pp_pos_tex, coords, 0.0);

    pivot_pos = result.xyz;

    // Unpack int from float
    int idx = int(result.w);

    return idx;
}

vec4 get_parent_pivot_direction_and_extent(sampler2D pp_dir_tex, vec2 coords) {
    vec4 result = textureLod(pp_dir_tex, coords, 0.0);
    result.xyz = result.xyz * 2.0 - 1.0;

    result.xyz = normalize(result.xyz);
    result.w *= PP_EXTENT;

    return result;
}

struct HierarchyData {
    // Maximal hierarchy level found
    int max_hierarchy_level;

    // Hierarchical attributes, 0 == current level, 1 == parent, 2 == parent of parent, etc.
    vec3 branch_pivot_pos[MAX_ALLOWED_HIERARCHY];
    vec4 branch_dir_extent[MAX_ALLOWED_HIERARCHY];
};

HierarchyData FetchHierarchyData(sampler2D pp_pos_tex, sampler2D pp_dir_tex, vec2 start_coords) {
    HierarchyData data;
    data.max_hierarchy_level = 0;

    vec2 current_uv = start_coords;
    // Iterate starting from SELF, as in HierarchyData
    for(int level = 0; level < MAX_ALLOWED_HIERARCHY; ++level) {
        int cur_idx = get_index_by_uv(current_uv);
        int parent_idx = get_parent_index_and_pivot_pos(pp_pos_tex, current_uv, data.branch_pivot_pos[level].xyz);
        data.branch_dir_extent[level] = get_parent_pivot_direction_and_extent(pp_dir_tex, current_uv);

        // False will happen at trunk level
        if (cur_idx != parent_idx) {
            data.max_hierarchy_level++;

            // Next level UVs
            current_uv = get_uv_by_index(parent_idx);
        }
    }

    return data;
}

// Geometric progression based on hierarchy level
// Probably good idea to normalize to (0, maxAllowedHierarchy) range
float get_wind_speed_mult(int idx) {
    return WindNoiseSpeedBase * pow(WindNoiseSpeedLevelMul, idx);
}

float get_angle_rotation_rate(int idx) {
    return WindAngleRotBase + WindAngleRotLevelMul * idx;
}

float get_motion_dampening_falloff_radius(int idx) {
    return WindMotionDampBase * pow(WindMotionDampLevelMul, idx);
}

vec4 make_quat(vec3 axis, float angle) {
    return vec4(axis.x * sin(angle / 2), axis.y * sin(angle / 2), axis.z * sin(angle / 2), cos(angle / 2));
}

vec3 quat_rotate(vec4 q, vec3 v) {
    vec3 t = 2 * cross(q.xyz, v);
    return v + q.w * t + cross(q.xyz, t);
}

vec3 TransformVegetation(vec3 old_pos, inout vec3 inout_normal, inout vec3 inout_tangent, sampler2D noise_tex,
                         vec4 wind_scroll, vec4 wind_params, vec4 wind_vec_ls, HierarchyData data) {
    mat2 uv_tr;
    uv_tr[0] = normalize(vec2(wind_vec_ls.x, wind_vec_ls.z));
    uv_tr[1] = vec2(uv_tr[0].y, -uv_tr[0].x);

    float parent_angle_animation = 0.0;
    vec3 pos_offset = vec3(0.0);
    vec3 normal = inout_normal, tangent = inout_tangent;

    // Iterate starting from trunk at 0
    for(int idx = 0; idx <= data.max_hierarchy_level; ++idx) {
        // Because HierarchyData and this loop have reversed ordering, we need reversed index
        int revIdx = max(data.max_hierarchy_level - idx, 0);

        vec3 branch_pivot_pos = data.branch_pivot_pos[revIdx];
        vec3 branch_dir = data.branch_dir_extent[revIdx].xyz;
        float branch_extent = data.branch_dir_extent[revIdx].w;

        vec3 branch_tip_pos = branch_pivot_pos + branch_dir * branch_extent;
        vec2 branch_tip_pos_ts = uv_tr * branch_tip_pos.xz + vec2(2.0 * branch_tip_pos.y, 0.0);

        vec3 wind_dir = get_wind_speed_mult(idx) * (wind_vec_ls.xyz + wind_vec_ls.w * textureLod(noise_tex, wind_scroll.xy + branch_tip_pos_ts, 0.0).xyz);
        float wind_speed = length(wind_dir);

        float rotation_angle_animation = 0;
        rotation_angle_animation += wind_speed * get_angle_rotation_rate(idx);
        rotation_angle_animation += parent_angle_animation * WindParentContrib;
        rotation_angle_animation = min(rotation_angle_animation, WindAngleRotMax);

        vec3 dir_from_root = (old_pos - branch_pivot_pos);
        float along_branch = dot(dir_from_root, branch_dir);
        float falloff = get_motion_dampening_falloff_radius(idx) * branch_extent;
        float motion_mask = saturate(along_branch / falloff);

        float rotationAngle = rotation_angle_animation * motion_mask;

        vec3 rotation_axis = cross(branch_dir, wind_dir);
        //Protect cross-product from returning 0
        rotation_axis = mix(branch_dir, rotation_axis, saturate(wind_speed / 0.001));
        rotation_axis = normalize(rotation_axis);

        vec4 rot_quat = make_quat(rotation_axis, rotationAngle * M_PI / 180);
        vec3 new_pos = quat_rotate(rot_quat, old_pos - branch_pivot_pos) + branch_pivot_pos;

        normal = quat_rotate(rot_quat, normal);
        tangent = quat_rotate(rot_quat, tangent);
        pos_offset += new_pos - old_pos;
        parent_angle_animation += rotation_angle_animation;
    }

    inout_normal = normal;
    inout_tangent = tangent;

    return old_pos + pos_offset;
}

#endif // _VEGETATION_GLSL