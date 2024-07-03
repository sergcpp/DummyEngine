#define __GL_API_DEF__
#include "GL.h"
#undef __GL_API_DEF__

#include <cassert>

#if defined(__ANDROID__) || defined(__native_client__) || defined(EMSCRIPTEN)
#else
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#elif defined(__linux__)
#include <GL/glx.h>
#undef Success
#elif defined(__APPLE__)
// #include <OpenGL/OpenGL.h>
#endif
#endif

#include "GLExtDSAEmu.h"
#include "Log.h"

// #define GL_DISABLE_DSA

#undef None // defined in X.h
#undef near // defined in minwindef.h
#undef far
#undef min
#undef max

bool Ren::InitGLExtentions(ILog *log) {
#if defined(__ANDROID__) || defined(__native_client__) || defined(EMSCRIPTEN)
#define GetProcAddress(name) eglGetProcAddress(#name);
#define LOAD_GL_FUN(x)                                                                                                 \
    ren_##x = (decltype(ren_##x))eglGetProcAddress(#x);                                                                \
    if (!(ren_##x)) {                                                                                                  \
        log->Error("Failed to load %s", #x);                                                                           \
        return false;                                                                                                  \
    }

    glQueryCounterEXT = (PFNGLQUERYCOUNTEREXTPROC)GetProcAddress(glQueryCounterEXT);
    glGetQueryObjecti64vEXT = (PFNGLGETQUERYOBJECTI64VEXTPROC)GetProcAddress(glGetQueryObjecti64vEXT);
    glGetQueryObjectui64vEXT = (PFNGLGETQUERYOBJECTUI64VEXTPROC)GetProcAddress(glGetQueryObjectui64vEXT);

    //
    // direct state access emulation
    //

    ren_glCreateTextures = ren_glCreateTextures_emu;

    ren_glTextureStorage2D_Comp = ren_glTextureStorage2D_Comp_emu;
    ren_glTextureStorage3D_Comp = ren_glTextureStorage3D_Comp_emu;

    ren_glTextureSubImage2D_Comp = ren_glTextureSubImage2D_Comp_emu;
    ren_glTextureSubImage3D_Comp = ren_glTextureSubImage3D_Comp_emu;

    ren_glCompressedTextureSubImage2D_Comp = ren_glCompressedTextureSubImage2D_Comp_emu;
    ren_glCompressedTextureSubImage3D_Comp = ren_glCompressedTextureSubImage3D_Comp_emu;

    ren_glTextureParameterf_Comp = ren_glTextureParameterf_Comp_emu;
    ren_glTextureParameteri_Comp = ren_glTextureParameteri_Comp_emu;

    ren_glTextureParameterfv_Comp = ren_glTextureParameterfv_Comp_emu;
    ren_glTextureParameteriv_Comp = ren_glTextureParameteriv_Comp_emu;

    ren_glGenerateTextureMipmap_Comp = ren_glGenerateTextureMipmap_Comp_emu;

    ren_glBindTextureUnit_Comp = ren_glBindTextureUnit_Comp_emu;
#else

#if defined(_WIN32)
#define GetProcAddress(name) wglGetProcAddress(#name);
#define LOAD_GL_FUN(x)                                                                                                 \
    ren_##x = (decltype(ren_##x))wglGetProcAddress(#x);                                                                \
    if (!(ren_##x)) {                                                                                                  \
        log->Error("Failed to load %s", #x);                                                                           \
        return false;                                                                                                  \
    }

    if (wglGetCurrentContext() == NULL) {
        return false;
    }
#elif defined(__linux__)
#define GetProcAddress(name) glXGetProcAddress((const GLubyte *)#name);
#define LOAD_GL_FUN(x)                                                                                                 \
    ren_##x = (decltype(ren_##x))glXGetProcAddress((const GLubyte *)#x);                                               \
    if (!(ren_##x)) {                                                                                                  \
        log->Error("Failed to load %s", #x);                                                                           \
        return false;                                                                                                  \
    }
#elif defined(__APPLE__)
#define GetProcAddress(name) nullptr;
#endif

#if !defined(__APPLE__)
    LOAD_GL_FUN(glCreateProgram)
    LOAD_GL_FUN(glDeleteProgram)
    LOAD_GL_FUN(glUseProgram)
    LOAD_GL_FUN(glAttachShader)
    LOAD_GL_FUN(glLinkProgram)
    LOAD_GL_FUN(glGetProgramiv)
    LOAD_GL_FUN(glGetProgramInfoLog)
    LOAD_GL_FUN(glGetAttribLocation)
    LOAD_GL_FUN(glGetUniformLocation)
    LOAD_GL_FUN(glGetActiveAttrib)
    LOAD_GL_FUN(glGetActiveUniform)
    LOAD_GL_FUN(glGetUniformBlockIndex)
    LOAD_GL_FUN(glUniformBlockBinding)
    LOAD_GL_FUN(glVertexAttribPointer)
    LOAD_GL_FUN(glVertexAttribIPointer)
    LOAD_GL_FUN(glEnableVertexAttribArray)
    LOAD_GL_FUN(glDisableVertexAttribArray)

    LOAD_GL_FUN(glCreateShader)
    LOAD_GL_FUN(glDeleteShader)
    LOAD_GL_FUN(glShaderSource)
    LOAD_GL_FUN(glShaderBinary)
    LOAD_GL_FUN(glCompileShader)
    LOAD_GL_FUN(glSpecializeShader)
    LOAD_GL_FUN(glGetShaderiv)
    LOAD_GL_FUN(glGetShaderInfoLog)

#if !defined(__linux__)
    LOAD_GL_FUN(glActiveTexture)
#endif
    LOAD_GL_FUN(glGenerateMipmap)

    LOAD_GL_FUN(glGenBuffers)
    LOAD_GL_FUN(glDeleteBuffers)
    LOAD_GL_FUN(glBindBuffer)
    LOAD_GL_FUN(glBufferData)
    LOAD_GL_FUN(glBufferSubData)
    LOAD_GL_FUN(glBindBufferBase)
    LOAD_GL_FUN(glBindBufferRange)
    LOAD_GL_FUN(glBindVertexBuffer)
    LOAD_GL_FUN(glCopyBufferSubData)
    LOAD_GL_FUN(glBufferStorage)

    LOAD_GL_FUN(glMapBuffer)
    LOAD_GL_FUN(glMapBufferRange)
    LOAD_GL_FUN(glFlushMappedBufferRange)
    LOAD_GL_FUN(glUnmapBuffer)

    LOAD_GL_FUN(glGenFramebuffers)
    LOAD_GL_FUN(glDeleteFramebuffers)
    LOAD_GL_FUN(glBindFramebuffer)
    LOAD_GL_FUN(glFramebufferTexture2D)
    LOAD_GL_FUN(glFramebufferTexture3D)
    LOAD_GL_FUN(glFramebufferTextureLayer)

    LOAD_GL_FUN(glGenRenderbuffers)
    LOAD_GL_FUN(glDeleteRenderbuffers)
    LOAD_GL_FUN(glBindRenderbuffer)
    LOAD_GL_FUN(glRenderbufferStorage)

    LOAD_GL_FUN(glFramebufferRenderbuffer)
    LOAD_GL_FUN(glCheckFramebufferStatus)

    LOAD_GL_FUN(glDrawBuffers)
    LOAD_GL_FUN(glBindFragDataLocation)

    LOAD_GL_FUN(glGenVertexArrays)
    LOAD_GL_FUN(glBindVertexArray)
    LOAD_GL_FUN(glDeleteVertexArrays)

    LOAD_GL_FUN(glUniform1f)
    LOAD_GL_FUN(glUniform2f)
    LOAD_GL_FUN(glUniform3f)
    LOAD_GL_FUN(glUniform4f)

    LOAD_GL_FUN(glUniform1i)
    LOAD_GL_FUN(glUniform2i)
    LOAD_GL_FUN(glUniform3i)
    LOAD_GL_FUN(glUniform4i)

    LOAD_GL_FUN(glUniform1iv)
    LOAD_GL_FUN(glUniform2iv)
    LOAD_GL_FUN(glUniform3iv)
    LOAD_GL_FUN(glUniform4iv)

    LOAD_GL_FUN(glUniform1ui)
    LOAD_GL_FUN(glUniform2ui)
    LOAD_GL_FUN(glUniform3ui)
    LOAD_GL_FUN(glUniform4ui)

    LOAD_GL_FUN(glUniform1uiv)
    LOAD_GL_FUN(glUniform2uiv)
    LOAD_GL_FUN(glUniform3uiv)
    LOAD_GL_FUN(glUniform4uiv)

    LOAD_GL_FUN(glUniform3fv)
    LOAD_GL_FUN(glUniform4fv)

    LOAD_GL_FUN(glUniformMatrix4fv)
    LOAD_GL_FUN(glUniformMatrix3x4fv)

    LOAD_GL_FUN(glCompressedTexImage2D)
    LOAD_GL_FUN(glCompressedTexImage3D)

    LOAD_GL_FUN(glCompressedTexSubImage2D)
    LOAD_GL_FUN(glCompressedTexSubImage3D)

    LOAD_GL_FUN(glTexStorage2D)
    LOAD_GL_FUN(glTexStorage2DMultisample)
    LOAD_GL_FUN(glTexImage2DMultisample)
    LOAD_GL_FUN(glRenderbufferStorageMultisample)

    LOAD_GL_FUN(glTexStorage3D)
    LOAD_GL_FUN(glTexSubImage3D)
    LOAD_GL_FUN(glCopyImageSubData)

#if !defined(__linux__)
    LOAD_GL_FUN(glTexImage3D)
#endif

    LOAD_GL_FUN(glDrawElementsBaseVertex)
    LOAD_GL_FUN(glDrawElementsInstanced)
    LOAD_GL_FUN(glDrawElementsInstancedBaseVertex)

    LOAD_GL_FUN(glDispatchCompute)
    LOAD_GL_FUN(glDispatchComputeIndirect)
    LOAD_GL_FUN(glMemoryBarrier)
    LOAD_GL_FUN(glGetBufferSubData)

    LOAD_GL_FUN(glTexBuffer)
    LOAD_GL_FUN(glTexBufferRange)

    LOAD_GL_FUN(glGenQueries)
    LOAD_GL_FUN(glDeleteQueries)
    LOAD_GL_FUN(glQueryCounter)

    LOAD_GL_FUN(glGetQueryObjectiv)
    LOAD_GL_FUN(glGetQueryObjectuiv)
    LOAD_GL_FUN(glGetQueryObjecti64v)
    LOAD_GL_FUN(glGetQueryObjectui64v)

    LOAD_GL_FUN(glGetStringi)

    LOAD_GL_FUN(glGetInteger64v)
    LOAD_GL_FUN(glGetBooleani_v)
    LOAD_GL_FUN(glGetIntegeri_v)
    LOAD_GL_FUN(glGetFloati_v)
    LOAD_GL_FUN(glGetDoublei_v)
    LOAD_GL_FUN(glGetInteger64i_v)

    LOAD_GL_FUN(glGetTextureLevelParameterfv)
    LOAD_GL_FUN(glGetTextureLevelParameteriv)

    LOAD_GL_FUN(glGetTextureImage)
    LOAD_GL_FUN(glGetTextureSubImage)

    LOAD_GL_FUN(glGetCompressedTextureImage)
    LOAD_GL_FUN(glGetCompressedTextureSubImage)

    LOAD_GL_FUN(glDebugMessageCallback)
    LOAD_GL_FUN(glDebugMessageInsert)
    LOAD_GL_FUN(glPushDebugGroup)
    LOAD_GL_FUN(glPopDebugGroup)

    LOAD_GL_FUN(glObjectLabel)

    LOAD_GL_FUN(glFenceSync)
    LOAD_GL_FUN(glWaitSync)
    LOAD_GL_FUN(glClientWaitSync)
    LOAD_GL_FUN(glDeleteSync)

    LOAD_GL_FUN(glBlendFunci)
    LOAD_GL_FUN(glClearBufferfv)

    LOAD_GL_FUN(glClearBufferSubData)
    LOAD_GL_FUN(glClearTexImage)

    LOAD_GL_FUN(glClearDepthf)

    LOAD_GL_FUN(glBindImageTexture)

    LOAD_GL_FUN(glClipControl)
#endif

    //
    // direct state access
    //

#if !defined(GL_DISABLE_DSA) && !defined(__APPLE__)
    LOAD_GL_FUN(glCreateTextures)
    if (!ren_glCreateTextures) {
        ren_glCreateTextures = ren_glCreateTextures_emu;
    }

    LOAD_GL_FUN(glTextureStorage2D)
    if (ren_glTextureStorage2D) {
        ren_glTextureStorage2D_Comp = [](GLenum /*target*/, GLuint texture, GLsizei levels, GLenum internalformat,
                                         GLsizei width, GLsizei height) {
            ren_glTextureStorage2D(texture, levels, internalformat, width, height);
        };
    } else {
        ren_glTextureStorage2D_Comp = ren_glTextureStorage2D_Comp_emu;
    }

    LOAD_GL_FUN(glTextureStorage3D)
    if (ren_glTextureStorage3D) {
        ren_glTextureStorage3D_Comp = [](GLenum /*target*/, GLuint texture, GLsizei levels, GLenum internalformat,
                                         GLsizei width, GLsizei height, GLsizei depth) {
            ren_glTextureStorage3D(texture, levels, internalformat, width, height, depth);
        };
    } else {
        ren_glTextureStorage3D_Comp = ren_glTextureStorage3D_Comp_emu;
    }

    LOAD_GL_FUN(glTextureSubImage2D)
    if (ren_glTextureSubImage2D) {
        ren_glTextureSubImage2D_Comp = [](GLenum /*target*/, GLuint texture, GLint level, GLint xoffset, GLint yoffset,
                                          GLsizei width, GLsizei height, GLenum format, GLenum type,
                                          const void *pixels) {
            ren_glTextureSubImage2D(texture, level, xoffset, yoffset, width, height, format, type, pixels);
        };
    } else {
        ren_glTextureSubImage2D_Comp = ren_glTextureSubImage2D_Comp_emu;
    }

    LOAD_GL_FUN(glTextureSubImage3D)
    if (ren_glTextureSubImage3D) {
        ren_glTextureSubImage3D_Comp = [](GLenum /*target*/, GLuint texture, GLint level, GLint xoffset, GLint yoffset,
                                          GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format,
                                          GLenum type, const void *pixels) {
            ren_glTextureSubImage3D(texture, level, xoffset, yoffset, zoffset, width, height, depth, format, type,
                                    pixels);
        };
    } else {
        ren_glTextureSubImage3D_Comp = ren_glTextureSubImage3D_Comp_emu;
    }

    LOAD_GL_FUN(glCompressedTextureSubImage2D)
    if (ren_glCompressedTextureSubImage2D) {
        ren_glCompressedTextureSubImage2D_Comp = [](GLenum /*target*/, GLuint texture, GLint level, GLint xoffset,
                                                    GLint yoffset, GLsizei width, GLsizei height, GLenum format,
                                                    GLsizei imageSize, const void *data) {
            ren_glCompressedTextureSubImage2D(texture, level, xoffset, yoffset, width, height, format, imageSize, data);
        };
    } else {
        ren_glCompressedTextureSubImage2D_Comp = ren_glCompressedTextureSubImage2D_Comp_emu;
    }

    LOAD_GL_FUN(glCompressedTextureSubImage3D)
    if (ren_glCompressedTextureSubImage3D) {
        ren_glCompressedTextureSubImage3D_Comp = [](GLenum /*target*/, GLuint texture, GLint level, GLint xoffset,
                                                    GLint yoffset, GLint zoffset, GLsizei width, GLsizei height,
                                                    GLsizei depth, GLenum format, GLsizei imageSize, const void *data) {
            ren_glCompressedTextureSubImage3D(texture, level, xoffset, yoffset, zoffset, width, height, depth, format,
                                              imageSize, data);
        };
    } else {
        ren_glCompressedTextureSubImage3D_Comp = ren_glCompressedTextureSubImage3D_Comp_emu;
    }

    LOAD_GL_FUN(glTextureParameterf)
    if (ren_glTextureParameterf) {
        ren_glTextureParameterf_Comp = [](GLenum /*target*/, GLuint texture, GLenum pname, GLfloat param) {
            ren_glTextureParameterf(texture, pname, param);
        };
    } else {
        ren_glTextureParameterf_Comp = ren_glTextureParameterf_Comp_emu;
    }
    LOAD_GL_FUN(glTextureParameteri)
    if (ren_glTextureParameteri) {
        ren_glTextureParameteri_Comp = [](GLenum /*target*/, GLuint texture, GLenum pname, GLint param) {
            ren_glTextureParameteri(texture, pname, param);
        };
    } else {
        ren_glTextureParameteri_Comp = ren_glTextureParameteri_Comp_emu;
    }

    LOAD_GL_FUN(glTextureParameterfv)
    if (ren_glTextureParameterfv) {
        ren_glTextureParameterfv_Comp = [](GLenum /*target*/, GLuint texture, GLenum pname, const GLfloat *params) {
            ren_glTextureParameterfv(texture, pname, params);
        };
    } else {
        ren_glTextureParameterfv_Comp = ren_glTextureParameterfv_Comp_emu;
    }
    LOAD_GL_FUN(glTextureParameteriv)
    if (ren_glTextureParameteriv) {
        ren_glTextureParameteriv_Comp = [](GLenum /*target*/, GLuint texture, GLenum pname, const GLint *params) {
            ren_glTextureParameteriv(texture, pname, params);
        };
    } else {
        ren_glTextureParameteriv_Comp = ren_glTextureParameteriv_Comp_emu;
    }

    LOAD_GL_FUN(glGenerateTextureMipmap)
    if (ren_glGenerateTextureMipmap) {
        ren_glGenerateTextureMipmap_Comp = [](GLenum /*target*/, GLuint texture) {
            ren_glGenerateTextureMipmap(texture);
        };
    } else {
        ren_glGenerateTextureMipmap_Comp = ren_glGenerateTextureMipmap_Comp_emu;
    }

    LOAD_GL_FUN(glBindTextureUnit)
    if (ren_glBindTextureUnit) {
        ren_glBindTextureUnit_Comp = [](GLenum /*target*/, GLuint unit, GLuint texture) {
            ren_glBindTextureUnit(unit, texture);
        };
    } else {
        ren_glBindTextureUnit_Comp = ren_glBindTextureUnit_Comp_emu;
    }

    LOAD_GL_FUN(glNamedBufferStorage)
    if (ren_glNamedBufferStorage) {
        ren_glNamedBufferStorage_Comp = [](GLenum /*target*/, GLuint buffer, GLsizeiptr size, const void *data,
                                           GLbitfield flags) { ren_glNamedBufferStorage(buffer, size, data, flags); };
    } else {
        ren_glNamedBufferStorage_Comp = ren_glNamedBufferStorage_Comp_emu;
    }
#else
    ren_glCreateTextures = ren_glCreateTextures_emu;

    ren_glTextureStorage2D_Comp = ren_glTextureStorage2D_Comp_emu;
    ren_glTextureStorage3D_Comp = ren_glTextureStorage3D_Comp_emu;

    ren_glTextureSubImage2D_Comp = ren_glTextureSubImage2D_Comp_emu;
    ren_glTextureSubImage3D_Comp = ren_glTextureSubImage3D_Comp_emu;

    ren_glCompressedTextureSubImage2D_Comp = ren_glCompressedTextureSubImage2D_Comp_emu;
    ren_glCompressedTextureSubImage3D_Comp = ren_glCompressedTextureSubImage3D_Comp_emu;

    ren_glTextureParameterf_Comp = ren_glTextureParameterf_Comp_emu;
    ren_glTextureParameteri_Comp = ren_glTextureParameteri_Comp_emu;

    ren_glTextureParameterfv_Comp = ren_glTextureParameterfv_Comp_emu;
    ren_glTextureParameteriv_Comp = ren_glTextureParameteriv_Comp_emu;

    ren_glGenerateTextureMipmap_Comp = ren_glGenerateTextureMipmap_Comp_emu;

    ren_glBindTextureUnit_Comp = ren_glBindTextureUnit_Comp_emu;
#endif

    //
    // Bindless texture
    //
    LOAD_GL_FUN(glGetTextureHandleARB)
    if (!ren_glGetTextureHandleARB) {
        ren_glGetTextureHandleARB = (PFNGLGETTEXTUREHANDLEARB)GetProcAddress(glGetTextureHandleNV);
    }
    LOAD_GL_FUN(glGetTextureSamplerHandleARB)
    if (!ren_glGetTextureSamplerHandleARB) {
        ren_glGetTextureSamplerHandleARB = (PFNGLGETTEXTURESAMPLERHANDLEARB)GetProcAddress(glGetTextureSamplerHandleNV);
    }

    LOAD_GL_FUN(glMakeTextureHandleResidentARB)
    if (!ren_glMakeTextureHandleResidentARB) {
        ren_glMakeTextureHandleResidentARB =
            (PFNGLMAKETEXTUREHANDLERESIDENTARB)GetProcAddress(glMakeTextureHandleResidentNV);
    }
    LOAD_GL_FUN(glMakeTextureHandleNonResidentARB)
    if (!ren_glMakeTextureHandleNonResidentARB) {
        ren_glMakeTextureHandleNonResidentARB =
            (PFNGLMAKETEXTUREHANDLENONRESIDENTARB)GetProcAddress(glMakeTextureHandleNonResidentNV);
    }

    LOAD_GL_FUN(glGetImageHandleARB)
    if (!ren_glGetImageHandleARB) {
        ren_glGetImageHandleARB = (PFNGLGETIMAGEHANDLEARB)GetProcAddress(glGetImageHandleNV);
    }

    LOAD_GL_FUN(glMakeImageHandleResidentARB)
    if (!ren_glMakeImageHandleResidentARB) {
        ren_glMakeImageHandleResidentARB = (PFNGLMAKEIMAGEHANDLERESIDENTARB)GetProcAddress(glMakeImageHandleResidentNV);
    }
    LOAD_GL_FUN(glMakeImageHandleNonResidentARB)
    if (!ren_glMakeImageHandleNonResidentARB) {
        ren_glMakeImageHandleNonResidentARB =
            (PFNGLMAKEIMAGEHANDLENONRESIDENTARB)GetProcAddress(glMakeImageHandleNonResidentNV);
    }

    LOAD_GL_FUN(glUniformHandleui64ARB)
    if (!ren_glUniformHandleui64ARB) {
        ren_glUniformHandleui64ARB = (PFNGLUNIFORMHANDLEUI64ARB)GetProcAddress(glUniformHandleui64NV);
    }
    LOAD_GL_FUN(glUniformHandleui64vARB)
    if (!ren_glUniformHandleui64vARB) {
        ren_glUniformHandleui64vARB = (PFNGLUNIFORMHANDLEUI64VARB)GetProcAddress(glUniformHandleui64vNV);
    }
    LOAD_GL_FUN(glProgramUniformHandleui64ARB)
    if (!ren_glProgramUniformHandleui64ARB) {
        ren_glProgramUniformHandleui64ARB =
            (PFNGLPROGRAMUNIFORMHANDLEUI64ARB)GetProcAddress(glProgramUniformHandleui64NV);
    }
    LOAD_GL_FUN(glProgramUniformHandleui64vARB)
    if (!ren_glProgramUniformHandleui64vARB) {
        ren_glProgramUniformHandleui64vARB =
            (PFNGLPROGRAMUNIFORMHANDLEUI64VARB)GetProcAddress(glProgramUniformHandleui64vNV);
    }

    LOAD_GL_FUN(glIsTextureHandleResidentARB)
    if (!ren_glIsTextureHandleResidentARB) {
        ren_glIsTextureHandleResidentARB = (PFNGLISTEXTUREHANDLERESIDENTARB)GetProcAddress(glIsTextureHandleResidentNV);
    }
    LOAD_GL_FUN(glIsImageHandleResidentARB)
    if (!ren_glIsImageHandleResidentARB) {
        ren_glIsImageHandleResidentARB = (PFNGLISIMAGEHANDLERESIDENTARB)GetProcAddress(glIsImageHandleResidentNV);
    }

    //
    // Sampler objects
    //

    LOAD_GL_FUN(glGenSamplers)
    LOAD_GL_FUN(glDeleteSamplers)
    LOAD_GL_FUN(glIsSampler)
    LOAD_GL_FUN(glBindSampler)
    LOAD_GL_FUN(glSamplerParameteri)
    LOAD_GL_FUN(glSamplerParameterf)
#endif

    return true;
}
