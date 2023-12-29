
#include "_common.glsl"

#define INSTANCE_BUF_STRIDE 12

#define FetchModelMatrix(instance_buf, instance)                                        \
    transpose(mat4(texelFetch((instance_buf), (instance) * INSTANCE_BUF_STRIDE + 0),    \
                   texelFetch((instance_buf), (instance) * INSTANCE_BUF_STRIDE + 1),    \
                   texelFetch((instance_buf), (instance) * INSTANCE_BUF_STRIDE + 2),    \
                   vec4(0.0, 0.0, 0.0, 1.0)))

#define FetchNormalMatrix(instance_buf, instance)                                       \
    mat4(texelFetch((instance_buf), (instance) * INSTANCE_BUF_STRIDE + 4),              \
         texelFetch((instance_buf), (instance) * INSTANCE_BUF_STRIDE + 5),              \
         texelFetch((instance_buf), (instance) * INSTANCE_BUF_STRIDE + 6),              \
         vec4(0.0, 0.0, 0.0, 1.0))

#define FetchPrevModelMatrix(instance_buf, instance)                                    \
    transpose(mat4(texelFetch((instance_buf), (instance) * INSTANCE_BUF_STRIDE + 8),    \
                   texelFetch((instance_buf), (instance) * INSTANCE_BUF_STRIDE + 9),    \
                   texelFetch((instance_buf), (instance) * INSTANCE_BUF_STRIDE + 10),   \
                   vec4(0.0, 0.0, 0.0, 1.0)))
