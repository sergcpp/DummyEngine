#version 430 core
#extension GL_ARB_shading_language_packing : require

#include "_cs_common.glsl"
#include "clear_buffer_interface.h"

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(std430, binding = OUT_BUF_SLOT) writeonly buffer OutData {
    uint g_out_data[];
};

layout (local_size_x = LOCAL_GROUP_SIZE_X, local_size_y = 1, local_size_z = 1) in;

void main() {
    if (gl_GlobalInvocationID.x >= g_params.data_len) {
        return;
    }
    g_out_data[gl_GlobalInvocationID.x] = 0;
}
