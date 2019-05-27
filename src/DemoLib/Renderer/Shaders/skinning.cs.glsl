R"(
#version 430

struct InVertex {
	vec3 p;
	vec3 n;
    vec3 b;
    vec2 t0;
    vec2 t1;
    vec4 bone_indices;
    vec4 bone_weights;
};

struct OutVertex {
    vec3 p;
	vec3 n;
    vec3 b;
    vec2 t0;
    vec2 t1;
};

layout(std430, binding = 0) readonly buffer Input0 {
    InVertex vertices[];
} in_data;

layout(std430, binding = 1) writeonly buffer Output {
    OutVertex vertices[];
} out_data;

layout (local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

void main() {
    uint i = gl_GlobalInvocationID.x;

    out_data.vertices[i].p = in_data.vertices[i].p;
    out_data.vertices[i].n = in_data.vertices[i].n;
    out_data.vertices[i].b = in_data.vertices[i].b;
    out_data.vertices[i].t0 = in_data.vertices[i].t0;
    out_data.vertices[i].t1 = in_data.vertices[i].t1;
}

)"