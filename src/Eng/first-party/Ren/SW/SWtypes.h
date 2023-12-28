#ifndef SW_TYPES_H
#define SW_TYPES_H

#include <stdint.h>

typedef uint8_t SWboolean;
typedef int32_t SWint;
typedef uint32_t SWuint;
typedef uint8_t SWubyte;
typedef uint16_t SWushort;
typedef float SWfloat;

typedef struct SWvtx_attribute {
    SWint size, stride;
    void *data;
} SWvtx_attribute;

typedef struct SWuniform {
    SWint type;
    void *data;
} SWuniform;

/* Check windows */
#if defined(_WIN32) || defined(_WIN64)
#if defined(_WIN64) && _WIN64
#define ENVIRONMENT64
#else
#define ENVIRONMENT32
#endif
#endif

/* Check GCC */
#ifdef __GNUC__
#if __x86_64__ || __ppc64__ || __aarch64__
#define ENVIRONMENT64
#else
#define ENVIRONMENT32
#endif
#endif

#ifdef __GNUC__
//#define sw_inline __attribute__((always_inline))
//#define sw_inline __attribute__((inline))
#if !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)
//#define sw_inline __always_inline
#else
#define sw_inline __inline__
#endif
#define RESTRICT __restrict__

#define ALIGNED(x, N) x __attribute__((aligned(N)));
#endif
#ifdef _MSC_VER
//#define sw_inline __forceinline

#define RESTRICT __restrict

#define ALIGNED(x, N)  __declspec(align(N)) x
#endif

#ifndef sw_inline
#define sw_inline static
#endif

#if defined(ENVIRONMENT32) && !defined(__ANDROID__)
#ifdef __GNUC__
#define FASTCALL __attribute__((fastcall))
#else
#define FASTCALL __fastcall
#endif
#else
/* should be fastcall as it is */
#define FASTCALL
#endif

/*#ifdef __cplusplus
extern "C" {
#endif
    extern struct SWcontext *sw_cur_context;
#ifdef __cplusplus
}
#endif*/

#define VS_IN SWvtx_attribute *RESTRICT attribs, SWuint index, SWuniform *uniforms
#define VS_OUT SWfloat *RESTRICT out_data
#define FS_IN SWfloat *RESTRICT f_in_data, SWuniform *RESTRICT uniforms
#define FS_OUT SWfloat *RESTRICT f_out_data, SWint *RESTRICT b_discard

#define V_FATTR(x) ((SWfloat *)((char *)attribs[x].data + index * attribs[x].stride))
#define V_POS_OUT out_data
#define V_FVARYING(x) ((SWfloat *)(out_data + 4 + x))

#define F_COL_OUT f_out_data
#define F_POS_IN f_in_data
#define F_FVARYING_IN(x) ((const SWfloat *)(f_in_data + 4 + x))

#define I_UNIFORM(x) ((SWint *)uniforms[(x)].data)
#define I_UNIFORM_S(x) (*(SWint *)uniforms[(x)].data)
#define F_UNIFORM(x) ((SWfloat *)uniforms[(x)].data)
#define F_UNIFORM_S(x) (*(SWfloat *)uniforms[(x)].data)

#define VEC4_SIZE (4 * sizeof(SWfloat))
#define VEC3_SIZE (3 * sizeof(SWfloat))
#define VEC2_SIZE (2 * sizeof(SWfloat))
#define FLOAT_SIZE sizeof(SWfloat)

#define INTERP_DATA1 void *varying1
#define INTERP_DATA2 void *varying2
#define INTERP_RES void *res

#define DISCARD                                                                          \
    (*b_discard) = 1;                                                                    \
    return

#define TEXTURE(slot, uv, col) swTexture(slot, uv, col)
/*#define TESTURE_RGB888(slot, uv, col) { \
        SWtexture *t = &sw_cur_context->textures[sw_cur_context->binded_textures[slot]];\
        swTex_RGB888_GetColorFloat_RGBA(t, uv[0], uv[1], col);                          \
    }*/

#define lerpff(f1, f2, t) ((f1) + t * ((f2) - (f1)))
#define lerpfff(f1, f2, f3, uvw) ((f1) * (uvw)[0] + (f2) * (uvw)[1] + (f3) * (uvw)[2])

typedef void(FASTCALL *vtx_shader_proc)(VS_IN, VS_OUT);
typedef void(FASTCALL *frag_shader_proc)(FS_IN, FS_OUT);

#define VSHADER void FASTCALL
#define FSHADER void FASTCALL

#define sw_swap(x, y, T)                                                                 \
    {                                                                                    \
        T tmp = x;                                                                       \
        x = y;                                                                           \
        y = tmp;                                                                         \
    }
#define sw_abs(x) ((x > 0) ? (x) : -(x))
#define sw_min(_1, _2) (((_1) < (_2)) ? (_1) : (_2))
#define sw_max(_1, _2) (((_1) > (_2)) ? (_1) : (_2))
#define sw_clamp(x, min, max) ((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))
/*#define sw_rotatei(_1, _2, _3) { SWint tmp = _1; _1 = _3; _3 = _2; _2 = tmp; }*/
#define sw_swapi(x, y)                                                                   \
    {                                                                                    \
        SWint tmp = x;                                                                   \
        x = y;                                                                           \
        y = tmp;                                                                         \
    }
#define sw_rotate_lefti(_0, _1, _2)                                                      \
    {                                                                                    \
        SWint tmp = (_0);                                                                \
        (_0) = (_1);                                                                     \
        (_1) = (_2);                                                                     \
        (_2) = tmp;                                                                      \
    }
#define sw_rotate_leftf(_0, _1, _2)                                                      \
    {                                                                                    \
        SWfloat tmp = (_0);                                                              \
        (_0) = (_1);                                                                     \
        (_1) = (_2);                                                                     \
        (_2) = tmp;                                                                      \
    }
#define sw_rotate_righti(_0, _1, _2)                                                     \
    {                                                                                    \
        SWint tmp = (_0);                                                                \
        (_0) = (_2);                                                                     \
        (_2) = (_1);                                                                     \
        (_1) = tmp;                                                                      \
    }
#define sw_rotate_lefti4(_0, _1, _2, _3)                                                 \
    {                                                                                    \
        SWint tmp = (_0);                                                                \
        (_0) = (_1);                                                                     \
        (_1) = (_2);                                                                     \
        (_2) = (_3);                                                                     \
        (_3) = tmp;                                                                      \
    }

#define sw_cross(res, v1, v2)                                                            \
    res[0] = v1[1] * v2[2] - v1[2] * v2[1];                                              \
    res[1] = v1[2] * v2[0] - v1[0] * v2[2];                                              \
    res[2] = v1[0] * v2[1] - v1[1] * v2[0];

#define sw_copy(dest, src, num)                                                          \
    switch (num) {                                                                       \
    case 16:                                                                             \
        (dest)[15] = (src)[15];                                                          \
    case 15:                                                                             \
        (dest)[14] = (src)[14];                                                          \
    case 14:                                                                             \
        (dest)[13] = (src)[13];                                                          \
    case 13:                                                                             \
        (dest)[12] = (src)[12];                                                          \
    case 12:                                                                             \
        (dest)[11] = (src)[11];                                                          \
    case 11:                                                                             \
        (dest)[10] = (src)[10];                                                          \
    case 10:                                                                             \
        (dest)[9] = (src)[9];                                                            \
    case 9:                                                                              \
        (dest)[8] = (src)[8];                                                            \
    case 8:                                                                              \
        (dest)[7] = (src)[7];                                                            \
    case 7:                                                                              \
        (dest)[6] = (src)[6];                                                            \
    case 6:                                                                              \
        (dest)[5] = (src)[5];                                                            \
    case 5:                                                                              \
        (dest)[4] = (src)[4];                                                            \
    case 4:                                                                              \
        (dest)[3] = (src)[3];                                                            \
    case 3:                                                                              \
        (dest)[2] = (src)[2];                                                            \
    case 2:                                                                              \
        (dest)[1] = (src)[1];                                                            \
    case 1:                                                                              \
        (dest)[0] = (src)[0];                                                            \
    default:;                                                                            \
    }

#define sw_add(dest, src, num)                                                           \
    switch (num) {                                                                       \
    case 16:                                                                             \
        (dest)[15] += (src)[15];                                                         \
    case 15:                                                                             \
        (dest)[14] += (src)[14];                                                         \
    case 14:                                                                             \
        (dest)[13] += (src)[13];                                                         \
    case 13:                                                                             \
        (dest)[12] += (src)[12];                                                         \
    case 12:                                                                             \
        (dest)[11] += (src)[11];                                                         \
    case 11:                                                                             \
        (dest)[10] += (src)[10];                                                         \
    case 10:                                                                             \
        (dest)[9] += (src)[9];                                                           \
    case 9:                                                                              \
        (dest)[8] += (src)[8];                                                           \
    case 8:                                                                              \
        (dest)[7] += (src)[7];                                                           \
    case 7:                                                                              \
        (dest)[6] += (src)[6];                                                           \
    case 6:                                                                              \
        (dest)[5] += (src)[5];                                                           \
    case 5:                                                                              \
        (dest)[4] += (src)[4];                                                           \
    case 4:                                                                              \
        (dest)[3] += (src)[3];                                                           \
    case 3:                                                                              \
        (dest)[2] += (src)[2];                                                           \
    case 2:                                                                              \
        (dest)[1] += (src)[1];                                                           \
    case 1:                                                                              \
        (dest)[0] += (src)[0];                                                           \
    default:;                                                                            \
    }

#define sw_add_3(dest, src)                                                              \
    (dest)[0] += (src)[0];                                                               \
    (dest)[1] += (src)[1];                                                               \
    (dest)[2] += (src)[2];

#define sw_add_q(dest, src, num)                                                         \
    switch (num) {                                                                       \
    case 16:                                                                             \
    case 15:                                                                             \
    case 14:                                                                             \
    case 13:                                                                             \
        (dest)[12] += (src)[12];                                                         \
        (dest)[13] += (src)[13];                                                         \
        (dest)[14] += (src)[14];                                                         \
        (dest)[15] += (src)[15];                                                         \
    case 12:                                                                             \
    case 11:                                                                             \
    case 10:                                                                             \
    case 9:                                                                              \
        (dest)[8] += (src)[8];                                                           \
        (dest)[9] += (src)[9];                                                           \
        (dest)[10] += (src)[10];                                                         \
        (dest)[11] += (src)[11];                                                         \
    case 8:                                                                              \
    case 7:                                                                              \
    case 6:                                                                              \
    case 5:                                                                              \
        (dest)[4] += (src)[4];                                                           \
        (dest)[5] += (src)[5];                                                           \
        (dest)[6] += (src)[6];                                                           \
        (dest)[7] += (src)[7];                                                           \
    case 4:                                                                              \
    case 3:                                                                              \
    case 2:                                                                              \
    case 1:                                                                              \
        (dest)[3] += (src)[3];                                                           \
        (dest)[2] += (src)[2];                                                           \
        (dest)[1] += (src)[1];                                                           \
        (dest)[0] += (src)[0];                                                           \
    default:;                                                                            \
    }

#endif /* SW_TYPES_H */
