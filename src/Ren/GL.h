#pragma once

#include <cstdint>

#ifdef __GL_API_DEF__
#define EXTERN_FUNC
#else
#define EXTERN_FUNC extern
#endif

#if defined(__ANDROID__) || defined(__native_client__) || defined(EMSCRIPTEN)
#include <GLES/egl.h>
//#if __ANDROID_API__ >= 24
#include <GLES3/gl32.h>
/*#elif __ANDROID_API__ >= 21
#include <GLES3/gl31.h>
#else
#include <GLES3/gl3.h>
#endif*/
#include <GLES2/gl2ext.h>
#include <GLES3/gl3ext.h>

#define GL_TIMESTAMP GL_TIMESTAMP_EXT

#define glQueryCounter glQueryCounterEXT
#define glGetQueryObjecti64v glGetQueryObjecti64vEXT
#define glGetQueryObjectui64v glGetQueryObjectui64vEXT

#define APIENTRY

typedef void (APIENTRY *PFNGLQUERYCOUNTEREXTPROC)(GLuint id, GLenum target);
typedef void (APIENTRY *PFNGLGETQUERYOBJECTI64VEXTPROC)(GLuint id, GLenum pname, GLint64 *params);
typedef void (APIENTRY *PFNGLGETQUERYOBJECTUI64VEXTPROC)(GLuint id, GLenum pname, GLuint64 *params);

EXTERN_FUNC PFNGLQUERYCOUNTEREXTPROC            glQueryCounterEXT;
EXTERN_FUNC PFNGLGETQUERYOBJECTI64VEXTPROC      glGetQueryObjecti64vEXT;
EXTERN_FUNC PFNGLGETQUERYOBJECTUI64VEXTPROC     glGetQueryObjectui64vEXT;
#else
//#include <GL/glew.h>

#if defined(WIN32)
#include <cstddef>

#define DECLSPEC_IMPORT __declspec(dllimport)

#if !defined(_GDI32_)
#define WINGDIAPI DECLSPEC_IMPORT
#else
#define WINGDIAPI
#endif

#endif

#define GL_MAJOR_VERSION                    0x821B
#define GL_MINOR_VERSION                    0x821C
#define GL_SHADING_LANGUAGE_VERSION         0x8B8C

#define GL_NUM_EXTENSIONS                   0x821D
#define GL_DEBUG_OUTPUT                     0x92E0
#define GL_DEBUG_OUTPUT_SYNCHRONOUS         0x8242

#define GL_DEBUG_SEVERITY_HIGH              0x9146
#define GL_DEBUG_SEVERITY_MEDIUM            0x9147
#define GL_DEBUG_SEVERITY_LOW               0x9148
#define GL_DEBUG_SEVERITY_NOTIFICATION      0x826B

#define GL_DEBUG_SOURCE_APPLICATION         0x824A
#define GL_DEBUG_TYPE_MARKER	            0x8268

#define GL_TEXTURE_MAX_ANISOTROPY_EXT       0x84FE
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT   0x84FF

#define GL_MAX_VERTEX_UNIFORM_COMPONENTS    0x8B4A
//#define GL_MAX_VERTEX_UNIFORM_VECTORS 0x8DFB

#define GL_FRAGMENT_SHADER                  0x8B30
#define GL_VERTEX_SHADER                    0x8B31
#define GL_COMPUTE_SHADER                   0x91B9

#define GL_COMPILE_STATUS                   0x8B81
#define GL_LINK_STATUS                      0x8B82

#define GL_INFO_LOG_LENGTH                  0x8B84

#define GL_ACTIVE_ATTRIBUTES                0x8B89
#define GL_ACTIVE_UNIFORMS                  0x8B86

#define GL_TEXTURE0                         0x84C0
#define GL_TEXTURE1                         0x84C1
#define GL_TEXTURE_CUBE_MAP                 0x8513

#define GL_TEXTURE_CUBE_MAP_ARRAY           0x9009

#define GL_CLAMP_TO_EDGE                    0x812F
#define GL_CLAMP_TO_BORDER                  0x812D
#define GL_GENERATE_MIPMAP_HINT             0x8192

#define GL_TEXTURE_CUBE_MAP_POSITIVE_X      0x8515
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_X      0x8516
#define GL_TEXTURE_CUBE_MAP_POSITIVE_Y      0x8517
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_Y      0x8518
#define GL_TEXTURE_CUBE_MAP_POSITIVE_Z      0x8519
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_Z      0x851A
#define GL_TEXTURE_WRAP_R                   0x8072

#define GL_ARRAY_BUFFER                     0x8892
#define GL_ELEMENT_ARRAY_BUFFER             0x8893
#define GL_UNIFORM_BUFFER                   0x8A11
#define GL_SHADER_STORAGE_BUFFER            0x90D2

#define GL_ARRAY_BUFFER_BINDING             0x8894
#define GL_ELEMENT_ARRAY_BUFFER_BINDING     0x8895

#define GL_STATIC_DRAW                      0x88E4
#define GL_DYNAMIC_DRAW                     0x88E8

#define GL_STREAM_READ                      0x88E1
#define GL_DYNAMIC_READ                     0x88E9

#define GL_DYNAMIC_COPY                     0x88EA

#define GL_READ_ONLY                        0x88B8
#define GL_WRITE_ONLY                       0x88B9

#define GL_COPY_WRITE_BUFFER                0x8f37

#define GL_DEPTH_COMPONENT16                0x81A5
#define GL_DEPTH_COMPONENT24                0x81A6

#define GL_COLOR_ATTACHMENT0                0x8CE0
#define GL_COLOR_ATTACHMENT1                0x8CE1
#define GL_COLOR_ATTACHMENT2                0x8CE2
#define GL_DEPTH_ATTACHMENT                 0x8D00
#define GL_STENCIL_ATTACHMENT               0x8D20

#define GL_FRAMEBUFFER                      0x8D40
#define GL_RENDERBUFFER                     0x8D41

#define GL_READ_FRAMEBUFFER                 0x8ca8
#define GL_DRAW_FRAMEBUFFER                 0x8ca9

#define GL_FRAMEBUFFER_COMPLETE             0x8CD5

#define GL_FRAMEBUFFER_BINDING              0x8CA6

#define GL_MAP_READ_BIT                     0x0001
#define GL_MAP_WRITE_BIT                    0x0002
#define GL_MAP_INVALIDATE_RANGE_BIT         0x0004
#define GL_MAP_FLUSH_EXPLICIT_BIT           0x0010
#define GL_MAP_UNSYNCHRONIZED_BIT           0x0020

#define GL_COMPRESSED_RED       0x8225
#define GL_COMPRESSED_RG        0x8226
#define GL_RG                   0x8227
#define GL_RG_INTEGER           0x8228
#define GL_R8                   0x8229
#define GL_R16                  0x822A
#define GL_RG8                  0x822B
#define GL_RG16                 0x822C
#define GL_R16F                 0x822D
#define GL_R32F                 0x822E
#define GL_RG16F                0x822F
#define GL_RG32F                0x8230
#define GL_R8I                  0x8231
#define GL_R8UI                 0x8232
#define GL_R16I                 0x8233
#define GL_R16UI                0x8234
#define GL_R32I                 0x8235
#define GL_R32UI                0x8236
#define GL_RG8I                 0x8237
#define GL_RG8UI                0x8238
#define GL_RG16I                0x8239
#define GL_RG16UI               0x823A
#define GL_RG32I                0x823B
#define GL_RG32UI               0x823C

// For compatibility with opengl es
#define GL_RG16_EXT             0x822C

#define GL_RGBA32F              0x8814
#define GL_RGB32F               0x8815

#define GL_RGB16F               0x881B
#define GL_RGBA16F              0x881A

#define GL_RGB10_A2             0x8059

#define GL_HALF_FLOAT   0x140B

#define GL_MULTISAMPLE  0x809D

#define GL_TEXTURE_2D_MULTISAMPLE 0x9100

#define GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT  0x00000001
#define GL_ELEMENT_ARRAY_BARRIER_BIT        0x00000002
#define GL_UNIFORM_BARRIER_BIT              0x00000004
#define GL_TEXTURE_FETCH_BARRIER_BIT        0x00000008
#define GL_SHADER_IMAGE_ACCESS_BARRIER_BIT  0x00000020
#define GL_COMMAND_BARRIER_BIT              0x00000040
#define GL_PIXEL_BUFFER_BARRIER_BIT         0x00000080
#define GL_TEXTURE_UPDATE_BARRIER_BIT       0x00000100
#define GL_BUFFER_UPDATE_BARRIER_BIT        0x00000200
#define GL_FRAMEBUFFER_BARRIER_BIT          0x00000400
#define GL_TRANSFORM_FEEDBACK_BARRIER_BIT   0x00000800
#define GL_ATOMIC_COUNTER_BARRIER_BIT       0x00001000
#define GL_SHADER_STORAGE_BARRIER_BIT       0x2000
#define GL_ALL_BARRIER_BITS                 0xFFFFFFFF

#define GL_TEXTURE_COMPARE_MODE     0x884C
#define GL_TEXTURE_COMPARE_FUNC     0x884D
#define GL_COMPARE_REF_TO_TEXTURE   0x884E

#define GL_TEXTURE_MAX_LEVEL 0x813D

#define GL_TEXTURE_BUFFER                 0x8C2A
#define GL_MAX_TEXTURE_BUFFER_SIZE        0x8C2B

#define GL_PIXEL_PACK_BUFFER        0x88eb

#define GL_TIMESTAMP 0x8E28

#define GL_QUERY_RESULT	0x8866

#define GL_COMPRESSED_RGBA_ASTC_4x4_KHR            0x93B0
#define GL_COMPRESSED_RGBA_ASTC_5x4_KHR            0x93B1
#define GL_COMPRESSED_RGBA_ASTC_5x5_KHR            0x93B2
#define GL_COMPRESSED_RGBA_ASTC_6x5_KHR            0x93B3
#define GL_COMPRESSED_RGBA_ASTC_6x6_KHR            0x93B4
#define GL_COMPRESSED_RGBA_ASTC_8x5_KHR            0x93B5
#define GL_COMPRESSED_RGBA_ASTC_8x6_KHR            0x93B6
#define GL_COMPRESSED_RGBA_ASTC_8x8_KHR            0x93B7
#define GL_COMPRESSED_RGBA_ASTC_10x5_KHR           0x93B8
#define GL_COMPRESSED_RGBA_ASTC_10x6_KHR           0x93B9
#define GL_COMPRESSED_RGBA_ASTC_10x8_KHR           0x93BA
#define GL_COMPRESSED_RGBA_ASTC_10x10_KHR          0x93BB
#define GL_COMPRESSED_RGBA_ASTC_12x10_KHR          0x93BC
#define GL_COMPRESSED_RGBA_ASTC_12x12_KHR          0x93BD

#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR    0x93D0
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR    0x93D1
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR    0x93D2
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR    0x93D3
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR    0x93D4
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR    0x93D5
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR    0x93D6
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR    0x93D7
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR   0x93D8
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR   0x93D9
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR   0x93DA
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR  0x93DB
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR  0x93DC
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR  0x93DD

#define GL_TEXTURE_CUBE_MAP_SEAMLESS        0x884F

#define GL_TEXTURE_BUFFER_OFFSET            0x919D
#define GL_TEXTURE_BUFFER_SIZE              0x919E
#define GL_TEXTURE_BUFFER_OFFSET_ALIGNMENT  0x919F

#define GL_SYNC_GPU_COMMANDS_COMPLETE     0x9117
#define GL_UNSIGNALED                     0x9118
#define GL_SIGNALED                       0x9119
#define GL_ALREADY_SIGNALED               0x911A
#define GL_TIMEOUT_EXPIRED                0x911B
#define GL_CONDITION_SATISFIED            0x911C

#ifndef APIENTRY
#if defined(WIN32)
#define WINAPI      __stdcall
#define APIENTRY    WINAPI
#endif
#endif

#ifndef APIENTRYP
//#define APIENTRYP APIENTRY *
#endif

#if defined(WIN32)
#include <GL/GL.h>
#else
#if defined(__APPLE__)
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif
#endif

typedef unsigned int GLenum;
typedef unsigned int GLuint;
typedef int GLint;
typedef short GLshort;
typedef unsigned char GLubyte;
typedef unsigned short GLushort;

typedef int64_t GLint64;
typedef uint64_t GLuint64;

typedef char GLchar;

typedef ptrdiff_t GLintptr;
typedef ptrdiff_t GLsizeiptr;

typedef struct __GLsync *GLsync;

extern "C" {

#if !defined(__APPLE__)
typedef GLuint(APIENTRY *PFNGLCREATEPROGRAMPROC)(void);
typedef void (APIENTRY *PFNGLDELETEPROGRAMPROC)(GLuint program);
typedef void (APIENTRY *PFNGLUSEPROGRAMPROC)(GLuint program);
typedef void (APIENTRY *PFNGLATTACHSHADERPROC)(GLuint program, GLuint shader);
typedef void (APIENTRY *PFNGLLINKPROGRAMPROC)(GLuint program);
typedef void (APIENTRY *PFNGLGETPROGRAMIVPROC)(GLuint program, GLenum pname, GLint *params);
typedef void (APIENTRY *PFNGLGETPROGRAMINFOLOGPROC)(GLuint program, GLsizei maxLength, GLsizei *length, GLchar *infoLog);
typedef GLint(APIENTRY *PFNGLGETATTRIBLOCATIONPROC)(GLuint program, const GLchar *name);
typedef GLint(APIENTRY *PFNGLGETUNIFORMLOCATIONPROC)(GLuint program, const GLchar *name);
typedef void (APIENTRY *PFNGLGETACTIVEATTRIBPROC)(GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, GLchar *name);
typedef void (APIENTRY *PFNGLGETACTIVEUNIFORMPROC)(GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, GLchar *name);
typedef GLuint (APIENTRY *PFNGLGETUNIFORMBLOCKINDEXPROC)(GLuint program, const GLchar *uniformBlockName);
typedef void (APIENTRY *PFNGLUNIFORMBLOCKBINDINGPROC)(GLuint program, GLuint uniformBlockIndex, GLuint uniformBlockBinding);
typedef void (APIENTRY *PFNGLVERTEXATTRIBPOINTERPROC)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid * pointer);
typedef void (APIENTRY *PFNGLENABLEVERTEXATTRIBARRAYPROC)(GLuint index);
typedef void (APIENTRY *PFNGLDISABLEVERTEXATTRIBARRAYPROC)(GLuint index);

typedef GLuint (APIENTRY *PFNGLCREATESHADERPROC)(GLenum shaderType);
typedef void (APIENTRY *PFNGLDELETESHADERPROC)(GLuint shader);
#if !defined(__linux__)
typedef void (APIENTRY *PFNGLSHADERSOURCEPROC)(GLuint shader, GLsizei count, const GLchar **string, const GLint *length);
#endif
typedef void (APIENTRY *PFNGLCOMPILESHADERPROC)(GLuint shader);
typedef void (APIENTRY *PFNGLGETSHADERIVPROC)(GLuint shader, GLenum pname, GLint *params);
typedef void (APIENTRY *PFNGLGETSHADERINFOLOGPROC)(GLuint shader, GLsizei maxLength, GLsizei *length, GLchar *infoLog);
#endif

#if !defined(__APPLE__)
#if !defined(__linux__)
typedef void (APIENTRY *PFNGLACTIVETEXTUREPROC)(GLenum texture);
#endif
typedef void (APIENTRY *PFNGLGENERATEMIPMAPPROC)(GLenum target);

typedef void (APIENTRY *PFNGLGENBUFFERSPROC)(GLsizei n, GLuint * buffers);
typedef void (APIENTRY *PFNGLDELETEBUFFERSPROC)(GLsizei n, const GLuint * buffers);
typedef void (APIENTRY *PFNGLBINDBUFFERPROC)(GLenum target, GLuint buffer);
typedef void (APIENTRY *PFNGLBUFFERDATAPROC)(GLenum target, GLsizeiptr size, const GLvoid * data, GLenum usage);
typedef void (APIENTRY *PFNGLBUFFERSUBDATAPROC)(GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid * data);
typedef void (APIENTRY *PFNGLBINDBUFFERBASEPROC)(GLenum target, GLuint index, GLuint buffer);
typedef void (APIENTRY *PFNGLBINDBUFFERRANGEPROC)(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size);
typedef void (APIENTRY *PFNGLBINDVERTEXBUFFERPROC)(GLuint bindingindex, GLuint buffer, GLintptr offset, GLsizei stride);
typedef void (APIENTRY *PFNGLCOPYBUFFERSUBDATAPROC)(GLenum readTarget, GLenum writeTarget, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size);

typedef void* (APIENTRY *PFNGLMAPBUFFERPROC)(GLenum target, GLenum access);
typedef void* (APIENTRY *PFNGLMAPBUFFERRANGEPROC)(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access);
typedef void (APIENTRY *PFNGLFLUSHMAPPEDBUFFERRANGEPROC)(GLenum target, GLintptr offset, GLsizeiptr length);
typedef GLboolean (APIENTRY *PFNGLUNMAPBUFFERPROC)(GLenum target);

typedef void (APIENTRY *PFNGLGENFRAMEBUFFERSPROC)(GLsizei n, GLuint *ids);
typedef void (APIENTRY *PFNGLDELETEFRAMEBUFFERSPROC)(GLsizei n, const GLuint * framebuffers);
typedef void (APIENTRY *PFNGLBINDFRAMEBUFFERPROC)(GLenum target, GLuint framebuffer);
typedef void (APIENTRY *PFNGLFRAMEBUFFERTEXTURE2DPROC)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
typedef void (APIENTRY *PFNGLFRAMEBUFFERTEXTURE3DPROC)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint layer);
typedef void (APIENTRY *PFNGLFRAMEBUFFERTEXTURELAYERPROC)(GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer);

typedef void (APIENTRY *PFNGLGENRENDERBUFFERSPROC)(GLsizei n, GLuint * renderbuffers);
typedef void (APIENTRY *PFNGLDELETERENDERBUFFERSPROC)(GLsizei n, const GLuint * renderbuffers);
typedef void (APIENTRY *PFNGLBINDRENDERBUFFERPROC)(GLenum target, GLuint renderbuffer);
typedef void (APIENTRY *PFNGLRENDERBUFFERSTORAGEPROC)(GLenum target, GLenum internalformat, GLsizei width, GLsizei height);

typedef void (APIENTRY *PFNGLFRAMEBUFFERRENDERBUFFERPROC)(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
typedef GLenum(APIENTRY *PFNGLCHECKFRAMEBUFFERSTATUSPROC)(GLenum target);

typedef void (APIENTRY *PFNGLDRAWBUFFERSPROC)(GLsizei n, const GLenum *bufs);
typedef void (APIENTRY *PFNGLBINDFRAGDATALOCATIONPROC)(GLuint program, GLuint colorNumber, const char * name);

typedef void (APIENTRY *PFNGLGENVERTEXARRAYSPROC)(GLsizei n, GLuint *arrays);
typedef void (APIENTRY *PFNGLBINDVERTEXARRAYPROC)(GLuint array);
typedef void (APIENTRY *PFNGLDELETEVERTEXARRAYSPROC)(GLsizei n, const GLuint *arrays);

typedef void (APIENTRY *PFNGLUNIFORM1FPROC)(GLint location, GLfloat v0);
typedef void (APIENTRY *PFNGLUNIFORM2FPROC)(GLint location, GLfloat v0, GLfloat v1);
typedef void (APIENTRY *PFNGLUNIFORM3FPROC)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
typedef void (APIENTRY *PFNGLUNIFORM4FPROC)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);

typedef void (APIENTRY *PFNGLUNIFORM1IPROC)(GLint location, GLint v0);
typedef void (APIENTRY *PFNGLUNIFORM2IPROC)(GLint location, GLint v0, GLint v1);
typedef void (APIENTRY *PFNGLUNIFORM3IPROC)(GLint location, GLint v0, GLint v1, GLint v2);

typedef void (APIENTRY *PFNGLUNIFORM1IVPROC)(GLint location, GLsizei count, const GLint *value);

typedef void (APIENTRY *PFNGLUNIFORM1UIPROC)(GLint location, GLuint v0);
typedef void (APIENTRY *PFNGLUNIFORM2UIPROC)(GLint location, GLuint v0, GLuint v1);
typedef void (APIENTRY *PFNGLUNIFORM3UIPROC)(GLint location, GLuint v0, GLuint v1, GLuint v2);
typedef void (APIENTRY *PFNGLUNIFORM4UIPROC)(GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3);

typedef void (APIENTRY *PFNGLUNIFORM3FVPROC)(GLint location, GLsizei count, const GLfloat *value);
typedef void (APIENTRY *PFNGLUNIFORM4FVPROC)(GLint location, GLsizei count, const GLfloat *value);

typedef void (APIENTRY *PFNGLUNIFORMMATRIX4FVPROC)(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);

#if !defined(__linux__)
typedef void (APIENTRY *PFNGLCOMPRESSEDTEXIMAGE2DPROC)(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const GLvoid * data);
#endif

typedef void (APIENTRY *PFNGLTEXSTORAGE2DPROC)(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height);
typedef void (APIENTRY *PFNGLTEXSTORAGE2DMULTISAMPLEPROC)(GLenum target, GLsizei samples, GLenum internalformat,
                                                          GLsizei width, GLsizei height, GLboolean fixedsamplelocations);
typedef void (APIENTRY *PFNGLTEXIMAGE2DMULTISAMPLEPROC)(GLenum target, GLsizei samples, GLenum internalformat,
                                                        GLsizei width, GLsizei height, GLboolean fixedsamplelocations);
typedef void (APIENTRY *PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC)(GLenum target, GLsizei samples, GLenum internalformat,
                                                                 GLsizei width, GLsizei height);

#if !defined(__linux__)
typedef void (APIENTRY *PFNGLTEXIMAGE3DPROC)(GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height,
                                             GLsizei depth, GLint border, GLenum format, GLenum type, const GLvoid * data);
typedef void (APIENTRY *PFNGLDRAWELEMENTSBASEVERTEXPROC)(GLenum mode, GLsizei count, GLenum type, GLvoid *indices, GLint basevertex);
#endif
typedef void (APIENTRY *PFNGLDRAWELEMENTSINSTANCEDPROC)(GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount);
typedef void (APIENTRY *PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXPROC)(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLsizei instancecount, GLint basevertex);

typedef void (APIENTRY *PFNGLDISPATCHCOMPUTEPROC)(GLuint num_groups_x, GLuint num_groups_y, GLuint num_groups_z);
typedef void (APIENTRY *PFNGLMEMORYBARRIERPROC)(GLbitfield barriers);
typedef void (APIENTRY *PFNGLGETBUFFERSUBDATAPROC)(GLenum target, GLintptr offset, GLsizeiptr size, GLvoid * data);

typedef void (APIENTRY *PFNGLTEXBUFFERPROC)(GLenum target, GLenum internalformat, GLuint buffer);
typedef void (APIENTRY *PFNGLTEXBUFFERRANGEPROC)(GLenum target, GLenum internalformat, GLuint buffer, GLintptr offset, GLsizeiptr size);

typedef void (APIENTRY *PFNGLGENQUERIESPROC)(GLsizei n, GLuint *ids);
typedef void (APIENTRY *PFNGLDELETEQUERIESPROC)(GLsizei n, const GLuint *ids);
typedef void (APIENTRY *PFNGLQUERYCOUNTERPROC)(GLuint id, GLenum target);

typedef void (APIENTRY *PFNGLGETQUERYOBJECTIVPROC)(GLuint id, GLenum pname, GLint *params);
typedef void (APIENTRY *PFNGLGETQUERYOBJECTUIVPROC)(GLuint id, GLenum pname, GLuint *params);
typedef void (APIENTRY *PFNGLGETQUERYOBJECTI64VPROC)(GLuint id, GLenum pname, GLint64 *params);
typedef void (APIENTRY *PFNGLGETQUERYOBJECTUI64V)(GLuint id, GLenum pname, GLuint64 *params);

typedef const GLubyte *(APIENTRY *PFNGLGETSTRINGIPROC)(GLenum name, GLuint index);

typedef void (APIENTRY *PFNGLGETINTEGER64VPROC)(GLenum pname, GLint64 *data);
typedef void (APIENTRY *PFNGLGETBOOLEANI_VPROC)(GLenum target, GLuint index, GLboolean *data);
typedef void (APIENTRY *PFNGLGETINTEGERI_VPROC)(GLenum target, GLuint index, GLint *data);
typedef void (APIENTRY *PFNGLGETFLOATI_VPROC)(GLenum target, GLuint index, GLfloat *data);
typedef void (APIENTRY *PFNGLGETDOUBLEI_VPROC)(GLenum target, GLuint index, GLdouble *data);
typedef void (APIENTRY *PFNGLGETINTEGER64I_VPROC)(GLenum target, GLuint index, GLint64 *data);

typedef void (APIENTRY *PFNGLGETTEXTURELEVELPARAMETERFVPROC)(GLuint texture, GLint level, GLenum pname, GLfloat *params);
typedef void (APIENTRY *PFNGLGETTEXTURELEVELPARAMETERIVPROC)(GLuint texture, GLint level, GLenum pname, GLint *params);

typedef void (APIENTRY *DEBUGPROC)(GLenum source,
                                   GLenum type,
                                   GLuint id,
                                   GLenum severity,
                                   GLsizei length,
                                   const GLchar *message,
                                   const void *userParam);
typedef void (APIENTRY *PFNGLDEBUGMESSAGECALLBACKPROC)(DEBUGPROC callback, const void * userParam);
typedef void (APIENTRY *PFNGLDEBUGMESSAGEINSERTPROC)(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const char *message);
typedef void (APIENTRY *PFNGLPUSHDEBUGGROUPPROC)(GLenum source, GLuint id, GLsizei length, const char *message);
typedef void (APIENTRY *PFNGLPOPDEBUGGROUPPROC)();

typedef void (APIENTRY *PFNGLOBJECTLABELPROC)(GLenum identifier, GLuint name, GLsizei length, const char *label);

typedef GLsync (APIENTRY *PFNGLFENCESYNCPROC)(GLenum condition, GLbitfield flags);
typedef GLenum (APIENTRY *PFNGLCLIENTWAITSYNCPROC)(GLsync sync, GLbitfield flags, GLuint64 timeout);
typedef void (APIENTRY *PFNGLDELETESYNCPROC)(GLsync sync);

#endif

#define glCreateProgram             ren_glCreateProgram
#define glDeleteProgram             ren_glDeleteProgram
#define glUseProgram                ren_glUseProgram
#define glAttachShader              ren_glAttachShader
#define glLinkProgram               ren_glLinkProgram
#define glGetProgramiv              ren_glGetProgramiv
#define glGetProgramInfoLog         ren_glGetProgramInfoLog
#define glGetAttribLocation         ren_glGetAttribLocation
#define glGetUniformLocation        ren_glGetUniformLocation
#define glGetActiveAttrib           ren_glGetActiveAttrib
#define glGetActiveUniform          ren_glGetActiveUniform
#define glGetUniformBlockIndex      ren_glGetUniformBlockIndex
#define glUniformBlockBinding       ren_glUniformBlockBinding
#define glVertexAttribPointer       ren_glVertexAttribPointer
#define glEnableVertexAttribArray   ren_glEnableVertexAttribArray
#define glDisableVertexAttribArray  ren_glDisableVertexAttribArray
#define glCreateShader              ren_glCreateShader
#define glDeleteShader              ren_glDeleteShader
#define glShaderSource              ren_glShaderSource
#define glCompileShader             ren_glCompileShader
#define glGetShaderiv               ren_glGetShaderiv
#define glGetShaderInfoLog          ren_glGetShaderInfoLog

#define glActiveTexture             ren_glActiveTexture
#define glGenerateMipmap            ren_glGenerateMipmap

#define glGenBuffers                ren_glGenBuffers
#define glDeleteBuffers             ren_glDeleteBuffers
#define glBindBuffer                ren_glBindBuffer
#define glBufferData                ren_glBufferData
#define glBufferSubData             ren_glBufferSubData
#define glBindBufferBase            ren_glBindBufferBase
#define glBindBufferRange           ren_glBindBufferRange
#define glBindVertexBuffer          ren_glBindVertexBuffer
#define glCopyBufferSubData         ren_glCopyBufferSubData

#define glMapBuffer                 ren_glMapBuffer
#define glMapBufferRange            ren_glMapBufferRange
#define glFlushMappedBufferRange    ren_glFlushMappedBufferRange
#define glUnmapBuffer               ren_glUnmapBuffer

#define glGenFramebuffers           ren_glGenFramebuffers
#define glDeleteFramebuffers        ren_glDeleteFramebuffers
#define glBindFramebuffer           ren_glBindFramebuffer
#define glFramebufferTexture2D      ren_glFramebufferTexture2D
#define glFramebufferTexture3D      ren_glFramebufferTexture3D
#define glFramebufferTextureLayer   ren_glFramebufferTextureLayer

#define glGenRenderbuffers          ren_glGenRenderbuffers
#define glDeleteRenderbuffers       ren_glDeleteRenderbuffers
#define glBindRenderbuffer          ren_glBindRenderbuffer
#define glRenderbufferStorage       ren_glRenderbufferStorage

#define glFramebufferRenderbuffer   ren_glFramebufferRenderbuffer
#define glCheckFramebufferStatus    ren_glCheckFramebufferStatus

#define glDrawBuffers               ren_glDrawBuffers
#define glBindFragDataLocation      ren_glBindFragDataLocation

#define glGenVertexArrays           ren_glGenVertexArrays
#define glBindVertexArray           ren_glBindVertexArray
#define glDeleteVertexArrays        ren_glDeleteVertexArrays

#define glUniform1f                 ren_glUniform1f
#define glUniform2f                 ren_glUniform2f
#define glUniform3f                 ren_glUniform3f
#define glUniform4f                 ren_glUniform4f

#define glUniform1i                 ren_glUniform1i
#define glUniform2i                 ren_glUniform2i
#define glUniform3i                 ren_glUniform3i

#define glUniform1iv                ren_glUniform1iv

#define glUniform1ui                ren_glUniform1ui
#define glUniform2ui                ren_glUniform2ui
#define glUniform3ui                ren_glUniform3ui
#define glUniform4ui                ren_glUniform4ui

#define glUniform3fv                ren_glUniform3fv
#define glUniform4fv                ren_glUniform4fv

#define glUniformMatrix4fv          ren_glUniformMatrix4fv

#define glCompressedTexImage2D      ren_glCompressedTexImage2D

#define glTexStorage2D              ren_glTexStorage2D
#define glTexStorage2DMultisample   ren_glTexStorage2DMultisample
#define glTexImage2DMultisample     ren_glTexImage2DMultisample
#define glRenderbufferStorageMultisample ren_glRenderbufferStorageMultisample

#define glTexImage3D                ren_glTexImage3D

#define glDrawElementsBaseVertex    ren_glDrawElementsBaseVertex
#define glDrawElementsInstanced     ren_glDrawElementsInstanced
#define glDrawElementsInstancedBaseVertex ren_glDrawElementsInstancedBaseVertex

#define glDispatchCompute           ren_glDispatchCompute
#define glMemoryBarrier             ren_glMemoryBarrier
#define glGetBufferSubData          ren_glGetBufferSubData

#define glTexBuffer                 ren_glTexBuffer
#define glTexBufferRange            ren_glTexBufferRange

#define glGenQueries                ren_glGenQueries
#define glDeleteQueries             ren_glDeleteQueries
#define glQueryCounter              ren_glQueryCounter

#define glGetQueryObjectiv          ren_glGetQueryObjectiv
#define glGetQueryObjectuiv         ren_glGetQueryObjectuiv
#define glGetQueryObjecti64v        ren_glGetQueryObjecti64v
#define glGetQueryObjectui64v       ren_glGetQueryObjectui64v

#define glGetStringi                ren_glGetStringi

#define glGetInteger64v             ren_glGetInteger64v
#define glGetBooleani_v             ren_glGetBooleani_v
#define glGetIntegeri_v             ren_glGetIntegeri_v
#define glGetFloati_v               ren_glGetFloati_v
#define glGetDoublei_v              ren_glGetDoublei_v
#define glGetInteger64i_v           ren_glGetInteger64i_v

#define glGetTextureLevelParameterfv ren_glGetTextureLevelParameterfv
#define glGetTextureLevelParameteriv ren_glGetTextureLevelParameteriv

#define glDebugMessageCallback      ren_glDebugMessageCallback
#define glDebugMessageInsert        ren_glDebugMessageInsert
#define glPushDebugGroup            ren_glPushDebugGroup
#define glPopDebugGroup             ren_glPopDebugGroup

#define glObjectLabel               ren_glObjectLabel

#define glFenceSync                 ren_glFenceSync
#define glClientWaitSync            ren_glClientWaitSync
#define glDeleteSync                ren_glDeleteSync

EXTERN_FUNC PFNGLCREATEPROGRAMPROC              ren_glCreateProgram;
EXTERN_FUNC PFNGLDELETEPROGRAMPROC              ren_glDeleteProgram;
EXTERN_FUNC PFNGLUSEPROGRAMPROC                 ren_glUseProgram;
EXTERN_FUNC PFNGLATTACHSHADERPROC               ren_glAttachShader;
EXTERN_FUNC PFNGLLINKPROGRAMPROC                ren_glLinkProgram;
EXTERN_FUNC PFNGLGETPROGRAMIVPROC               ren_glGetProgramiv;
EXTERN_FUNC PFNGLGETPROGRAMINFOLOGPROC          ren_glGetProgramInfoLog;
EXTERN_FUNC PFNGLGETATTRIBLOCATIONPROC          ren_glGetAttribLocation;
EXTERN_FUNC PFNGLGETUNIFORMLOCATIONPROC         ren_glGetUniformLocation;
EXTERN_FUNC PFNGLGETACTIVEATTRIBPROC            ren_glGetActiveAttrib;
EXTERN_FUNC PFNGLGETACTIVEUNIFORMPROC           ren_glGetActiveUniform;
EXTERN_FUNC PFNGLGETUNIFORMBLOCKINDEXPROC       ren_glGetUniformBlockIndex;
EXTERN_FUNC PFNGLUNIFORMBLOCKBINDINGPROC        ren_glUniformBlockBinding;
EXTERN_FUNC PFNGLVERTEXATTRIBPOINTERPROC        ren_glVertexAttribPointer;
EXTERN_FUNC PFNGLENABLEVERTEXATTRIBARRAYPROC    ren_glEnableVertexAttribArray;
EXTERN_FUNC PFNGLDISABLEVERTEXATTRIBARRAYPROC   ren_glDisableVertexAttribArray;

EXTERN_FUNC PFNGLCREATESHADERPROC               ren_glCreateShader;
EXTERN_FUNC PFNGLDELETESHADERPROC               ren_glDeleteShader;
EXTERN_FUNC PFNGLSHADERSOURCEPROC               ren_glShaderSource;
EXTERN_FUNC PFNGLCOMPILESHADERPROC              ren_glCompileShader;
EXTERN_FUNC PFNGLGETSHADERIVPROC                ren_glGetShaderiv;
EXTERN_FUNC PFNGLGETSHADERINFOLOGPROC           ren_glGetShaderInfoLog;

EXTERN_FUNC PFNGLACTIVETEXTUREPROC              ren_glActiveTexture;
EXTERN_FUNC PFNGLGENERATEMIPMAPPROC             ren_glGenerateMipmap;

EXTERN_FUNC PFNGLGENBUFFERSPROC                 ren_glGenBuffers;
EXTERN_FUNC PFNGLDELETEBUFFERSPROC              ren_glDeleteBuffers;
EXTERN_FUNC PFNGLBINDBUFFERPROC                 ren_glBindBuffer;
EXTERN_FUNC PFNGLBUFFERDATAPROC                 ren_glBufferData;
EXTERN_FUNC PFNGLBUFFERSUBDATAPROC              ren_glBufferSubData;
EXTERN_FUNC PFNGLBINDBUFFERBASEPROC             ren_glBindBufferBase;
EXTERN_FUNC PFNGLBINDBUFFERRANGEPROC            ren_glBindBufferRange;
EXTERN_FUNC PFNGLBINDVERTEXBUFFERPROC           ren_glBindVertexBuffer;
EXTERN_FUNC PFNGLCOPYBUFFERSUBDATAPROC          ren_glCopyBufferSubData;

EXTERN_FUNC PFNGLMAPBUFFERPROC                  ren_glMapBuffer;
EXTERN_FUNC PFNGLMAPBUFFERRANGEPROC             ren_glMapBufferRange;
EXTERN_FUNC PFNGLFLUSHMAPPEDBUFFERRANGEPROC     ren_glFlushMappedBufferRange;
EXTERN_FUNC PFNGLUNMAPBUFFERPROC                ren_glUnmapBuffer;

EXTERN_FUNC PFNGLGENFRAMEBUFFERSPROC            ren_glGenFramebuffers;
EXTERN_FUNC PFNGLDELETEFRAMEBUFFERSPROC         ren_glDeleteFramebuffers;
EXTERN_FUNC PFNGLBINDFRAMEBUFFERPROC            ren_glBindFramebuffer;
EXTERN_FUNC PFNGLFRAMEBUFFERTEXTURE2DPROC       ren_glFramebufferTexture2D;
EXTERN_FUNC PFNGLFRAMEBUFFERTEXTURE3DPROC       ren_glFramebufferTexture3D;
EXTERN_FUNC PFNGLFRAMEBUFFERTEXTURELAYERPROC    ren_glFramebufferTextureLayer;

EXTERN_FUNC PFNGLGENRENDERBUFFERSPROC           ren_glGenRenderbuffers;
EXTERN_FUNC PFNGLDELETERENDERBUFFERSPROC        ren_glDeleteRenderbuffers;
EXTERN_FUNC PFNGLBINDRENDERBUFFERPROC           ren_glBindRenderbuffer;
EXTERN_FUNC PFNGLRENDERBUFFERSTORAGEPROC        ren_glRenderbufferStorage;

EXTERN_FUNC PFNGLFRAMEBUFFERRENDERBUFFERPROC    ren_glFramebufferRenderbuffer;
EXTERN_FUNC PFNGLCHECKFRAMEBUFFERSTATUSPROC     ren_glCheckFramebufferStatus;

EXTERN_FUNC PFNGLDRAWBUFFERSPROC                ren_glDrawBuffers;
EXTERN_FUNC PFNGLBINDFRAGDATALOCATIONPROC       ren_glBindFragDataLocation;

EXTERN_FUNC PFNGLGENVERTEXARRAYSPROC            ren_glGenVertexArrays;
EXTERN_FUNC PFNGLBINDVERTEXARRAYPROC            ren_glBindVertexArray;
EXTERN_FUNC PFNGLDELETEVERTEXARRAYSPROC         ren_glDeleteVertexArrays;

EXTERN_FUNC PFNGLUNIFORM1FPROC                  ren_glUniform1f;
EXTERN_FUNC PFNGLUNIFORM2FPROC                  ren_glUniform2f;
EXTERN_FUNC PFNGLUNIFORM3FPROC                  ren_glUniform3f;
EXTERN_FUNC PFNGLUNIFORM4FPROC                  ren_glUniform4f;

EXTERN_FUNC PFNGLUNIFORM1IPROC                  ren_glUniform1i;
EXTERN_FUNC PFNGLUNIFORM2IPROC                  ren_glUniform2i;
EXTERN_FUNC PFNGLUNIFORM3IPROC                  ren_glUniform3i;

EXTERN_FUNC PFNGLUNIFORM1IVPROC                 ren_glUniform1iv;

EXTERN_FUNC PFNGLUNIFORM1UIPROC                 ren_glUniform1ui;
EXTERN_FUNC PFNGLUNIFORM2UIPROC                 ren_glUniform2ui;
EXTERN_FUNC PFNGLUNIFORM3UIPROC                 ren_glUniform3ui;
EXTERN_FUNC PFNGLUNIFORM4UIPROC                 ren_glUniform4ui;

EXTERN_FUNC PFNGLUNIFORM3FVPROC                 ren_glUniform3fv;
EXTERN_FUNC PFNGLUNIFORM4FVPROC                 ren_glUniform4fv;

EXTERN_FUNC PFNGLUNIFORMMATRIX4FVPROC           ren_glUniformMatrix4fv;

EXTERN_FUNC PFNGLCOMPRESSEDTEXIMAGE2DPROC       ren_glCompressedTexImage2D;

EXTERN_FUNC PFNGLTEXSTORAGE2DPROC               ren_glTexStorage2D;
EXTERN_FUNC PFNGLTEXSTORAGE2DMULTISAMPLEPROC    ren_glTexStorage2DMultisample;
EXTERN_FUNC PFNGLTEXIMAGE2DMULTISAMPLEPROC      ren_glTexImage2DMultisample;
EXTERN_FUNC PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC ren_glRenderbufferStorageMultisample;

EXTERN_FUNC PFNGLTEXIMAGE3DPROC                 ren_glTexImage3D;

EXTERN_FUNC PFNGLDRAWELEMENTSBASEVERTEXPROC     ren_glDrawElementsBaseVertex;
EXTERN_FUNC PFNGLDRAWELEMENTSINSTANCEDPROC      ren_glDrawElementsInstanced;
EXTERN_FUNC PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXPROC ren_glDrawElementsInstancedBaseVertex;

EXTERN_FUNC PFNGLDISPATCHCOMPUTEPROC            ren_glDispatchCompute;
EXTERN_FUNC PFNGLMEMORYBARRIERPROC              ren_glMemoryBarrier;
EXTERN_FUNC PFNGLGETBUFFERSUBDATAPROC           ren_glGetBufferSubData;

EXTERN_FUNC PFNGLTEXBUFFERPROC                  ren_glTexBuffer;
EXTERN_FUNC PFNGLTEXBUFFERRANGEPROC             ren_glTexBufferRange;

EXTERN_FUNC PFNGLGENQUERIESPROC                 ren_glGenQueries;
EXTERN_FUNC PFNGLDELETEQUERIESPROC              ren_glDeleteQueries;
EXTERN_FUNC PFNGLQUERYCOUNTERPROC               ren_glQueryCounter;

EXTERN_FUNC PFNGLGETQUERYOBJECTIVPROC           ren_glGetQueryObjectiv;
EXTERN_FUNC PFNGLGETQUERYOBJECTUIVPROC          ren_glGetQueryObjectuiv;
EXTERN_FUNC PFNGLGETQUERYOBJECTI64VPROC         ren_glGetQueryObjecti64v;
EXTERN_FUNC PFNGLGETQUERYOBJECTUI64V            ren_glGetQueryObjectui64v;

EXTERN_FUNC PFNGLGETSTRINGIPROC                 ren_glGetStringi;

EXTERN_FUNC PFNGLGETINTEGER64VPROC              ren_glGetInteger64v;
EXTERN_FUNC PFNGLGETBOOLEANI_VPROC              ren_glGetBooleani_v;
EXTERN_FUNC PFNGLGETINTEGERI_VPROC              ren_glGetIntegeri_v;
EXTERN_FUNC PFNGLGETFLOATI_VPROC                ren_glGetFloati_v;
EXTERN_FUNC PFNGLGETDOUBLEI_VPROC               ren_glGetDoublei_v;
EXTERN_FUNC PFNGLGETINTEGER64I_VPROC            ren_glGetInteger64i_v;

EXTERN_FUNC PFNGLGETTEXTURELEVELPARAMETERFVPROC ren_glGetTextureLevelParameterfv;
EXTERN_FUNC PFNGLGETTEXTURELEVELPARAMETERIVPROC ren_glGetTextureLevelParameteriv;

EXTERN_FUNC PFNGLDEBUGMESSAGECALLBACKPROC       ren_glDebugMessageCallback;
EXTERN_FUNC PFNGLDEBUGMESSAGEINSERTPROC         ren_glDebugMessageInsert;
EXTERN_FUNC PFNGLPUSHDEBUGGROUPPROC             ren_glPushDebugGroup;
EXTERN_FUNC PFNGLPOPDEBUGGROUPPROC              ren_glPopDebugGroup;

EXTERN_FUNC PFNGLOBJECTLABELPROC                ren_glObjectLabel;

EXTERN_FUNC PFNGLFENCESYNCPROC                  ren_glFenceSync;
EXTERN_FUNC PFNGLCLIENTWAITSYNCPROC             ren_glClientWaitSync;
EXTERN_FUNC PFNGLDELETESYNCPROC                 ren_glDeleteSync;
}

#undef EXTERN_FUNC
#endif

namespace Ren {
    bool InitGLExtentions();
}