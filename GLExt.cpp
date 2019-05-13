#include "GL.h"

#include <cassert>

#if defined(__ANDROID__) || defined(__native_client__) || defined(EMSCRIPTEN)
#else
#if defined(WIN32)
#include <Windows.h>
#elif defined(__linux__)
#include <GL/glx.h>
#endif
#endif

#if defined(__ANDROID__) || defined(__native_client__) || defined(EMSCRIPTEN)
void (APIENTRY *glQueryCounterEXT)(GLuint id, GLenum target);
void (APIENTRY *glGetQueryObjecti64vEXT)(GLuint id, GLenum pname, GLint64 *params);
void (APIENTRY *glGetQueryObjectui64vEXT)(GLuint id, GLenum pname, GLuint64 *params);
#else
GLuint(APIENTRY *glCreateProgram)(void);
void (APIENTRY *glDeleteProgram)(GLuint program);
void (APIENTRY *glUseProgram)(GLuint program);
void (APIENTRY *glAttachShader)(GLuint program, GLuint shader);
void (APIENTRY *glLinkProgram)(GLuint program);
void (APIENTRY *glGetProgramiv)(GLuint program, GLenum pname, GLint *params);
void (APIENTRY *glGetProgramInfoLog)(GLuint program, GLsizei maxLength, GLsizei *length, GLchar *infoLog);
GLint(APIENTRY *glGetAttribLocation)(GLuint program, const GLchar *name);
GLint(APIENTRY *glGetUniformLocation)(GLuint program, const GLchar *name);
void (APIENTRY *glGetActiveAttrib)(GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, GLchar *name);
void (APIENTRY *glGetActiveUniform)(GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, GLchar *name);
void (APIENTRY *glVertexAttribPointer)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid * pointer);
GLuint (APIENTRY *glGetUniformBlockIndex)(GLuint program, const GLchar *uniformBlockName);
void (APIENTRY *glUniformBlockBinding)(GLuint program, GLuint uniformBlockIndex, GLuint uniformBlockBinding);
void (APIENTRY *glEnableVertexAttribArray)(GLuint index);
void (APIENTRY *glDisableVertexAttribArray)(GLuint index);

GLuint(APIENTRY *glCreateShader)(GLenum shaderType);
void (APIENTRY *glDeleteShader)(GLuint shader);
void (APIENTRY *glShaderSource)(GLuint shader, GLsizei count, const GLchar **string, const GLint *length);
void (APIENTRY *glCompileShader)(GLuint shader);
void (APIENTRY *glGetShaderiv)(GLuint shader, GLenum pname, GLint *params);
void (APIENTRY *glGetShaderInfoLog)(GLuint shader, GLsizei maxLength, GLsizei *length, GLchar *infoLog);

#if !defined(__linux__)
void (APIENTRY *glActiveTexture)(GLenum texture);
#endif
void (APIENTRY *glGenerateMipmap)(GLenum target);

void (APIENTRY *glGenBuffers)(GLsizei n, GLuint * buffers);
void (APIENTRY *glDeleteBuffers)(GLsizei n, const GLuint * buffers);
void (APIENTRY *glBindBuffer)(GLenum target, GLuint buffer);
void (APIENTRY *glBufferData)(GLenum target, GLsizeiptr size, const GLvoid * data, GLenum usage);
void (APIENTRY *glBufferSubData)(GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid * data);
void (APIENTRY *glBindBufferBase)(GLenum target, GLuint index, GLuint buffer);
void (APIENTRY *glBindVertexBuffer)(GLuint bindingindex, GLuint buffer, GLintptr offset, GLsizei stride);
void (APIENTRY *glCopyBufferSubData)(GLenum readTarget, GLenum writeTarget, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size);

void* (APIENTRY *glMapBuffer)(GLenum target, GLenum access);
void* (APIENTRY *glMapBufferRange)(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access);
GLboolean (APIENTRY *glUnmapBuffer)(GLenum target);

void (APIENTRY *glGenFramebuffers)(GLsizei n, GLuint *ids);
void (APIENTRY *glDeleteFramebuffers)(GLsizei n, const GLuint * framebuffers);
void (APIENTRY *glBindFramebuffer)(GLenum target, GLuint framebuffer);
void (APIENTRY *glFramebufferTexture2D)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);

void (APIENTRY *glGenRenderbuffers)(GLsizei n, GLuint * renderbuffers);
void (APIENTRY *glDeleteRenderbuffers)(GLsizei n, const GLuint * renderbuffers);
void (APIENTRY *glBindRenderbuffer)(GLenum target, GLuint renderbuffer);
void (APIENTRY *glRenderbufferStorage)(GLenum target, GLenum internalformat, GLsizei width, GLsizei height);

void (APIENTRY *glFramebufferRenderbuffer)(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
GLenum (APIENTRY *glCheckFramebufferStatus)(GLenum target);

void (APIENTRY *glDrawBuffers)(GLsizei n, const GLenum *bufs);
void (APIENTRY *glBindFragDataLocation)(GLuint program, GLuint colorNumber, const char * name);

void (APIENTRY *glGenVertexArrays)(GLsizei n, GLuint *arrays);
void (APIENTRY *glBindVertexArray)(GLuint array);
void (APIENTRY *glDeleteVertexArrays)(GLsizei n, const GLuint *arrays);

void (APIENTRY *glUniform1f)(GLint location, GLfloat v0);
void (APIENTRY *glUniform2f)(GLint location, GLfloat v0, GLfloat v1);
void (APIENTRY *glUniform3f)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
void (APIENTRY *glUniform4f)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);

void (APIENTRY *glUniform1i)(GLint location, GLint v0);
void (APIENTRY *glUniform2i)(GLint location, GLint v0, GLint v1);
void (APIENTRY *glUniform3i)(GLint location, GLint v0, GLint v1, GLint v2);

void (APIENTRY *glUniform1iv)(GLint location, GLsizei count, const GLint *value);

void (APIENTRY *glUniform3fv)(GLint location, GLsizei count, const GLfloat *value);
void (APIENTRY *glUniform4fv)(GLint location, GLsizei count, const GLfloat *value);

void (APIENTRY *glUniformMatrix4fv)(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);

void (APIENTRY *glCompressedTexImage2D)(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const GLvoid * data);

void (APIENTRY *glTexStorage2D)(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height);
void (APIENTRY *glTexStorage2DMultisample)(GLenum target, GLsizei samples, GLenum internalformat,
        GLsizei width, GLsizei height, GLboolean fixedsamplelocations);
void (APIENTRY *glRenderbufferStorageMultisample)(GLenum target, GLsizei samples, GLenum internalformat,
        GLsizei width, GLsizei height);

void (APIENTRY *glDrawElementsBaseVertex)(GLenum mode, GLsizei count, GLenum type, GLvoid *indices, GLint basevertex);
void (APIENTRY *glDrawElementsInstanced)(GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount);

void (APIENTRY *glDispatchCompute)(GLuint num_groups_x, GLuint num_groups_y, GLuint num_groups_z);
void (APIENTRY *glMemoryBarrier)(GLbitfield barriers);
void (APIENTRY *glGetBufferSubData)(GLenum target, GLintptr offset, GLsizeiptr size, GLvoid * data);

void (APIENTRY *glTexBuffer)(GLenum target, GLenum internalformat, GLuint buffer);

void (APIENTRY *glGenQueries)(GLsizei n, GLuint *ids);
void (APIENTRY *glDeleteQueries)(GLsizei n, const GLuint *ids);
void (APIENTRY *glQueryCounter)(GLuint id, GLenum target);

void (APIENTRY *glGetQueryObjectiv)(GLuint id, GLenum pname, GLint * params);
void (APIENTRY *glGetQueryObjectuiv)(GLuint id, GLenum pname, GLuint * params);
void (APIENTRY *glGetQueryObjecti64v)(GLuint id, GLenum pname, GLint64 *params);
void (APIENTRY *glGetQueryObjectui64v)(GLuint id, GLenum pname, GLuint64 *params);

const GLubyte *(APIENTRY *glGetStringi)(GLenum name, GLuint index);

void (APIENTRY *glGetInteger64v)(GLenum pname, GLint64 *data);
void (APIENTRY *glGetBooleani_v)(GLenum target, GLuint index, GLboolean *data);
void (APIENTRY *glGetIntegeri_v)(GLenum target, GLuint index, GLint *data);
void (APIENTRY *glGetFloati_v)(GLenum target, GLuint index, GLfloat *data);
void (APIENTRY *glGetDoublei_v)(GLenum target, GLuint index, GLdouble *data);
void (APIENTRY *glGetInteger64i_v)(GLenum target, GLuint index, GLint64 *data);

void (APIENTRY *glDebugMessageCallback)(DEBUGPROC callback, const void * userParam);
#endif

bool Ren::InitGLExtentions() {

#if defined(__ANDROID__) || defined(__native_client__) || defined(EMSCRIPTEN)
#define GetProcAddress(name) (decltype(name))eglGetProcAddress(#name);
    
    glQueryCounterEXT = GetProcAddress(glQueryCounterEXT);
    glGetQueryObjecti64vEXT = GetProcAddress(glGetQueryObjecti64vEXT);
    glGetQueryObjectui64vEXT = GetProcAddress(glGetQueryObjectui64vEXT);
#else

#if defined(WIN32)
#define GetProcAddress(name) (decltype(name))wglGetProcAddress(#name);

    if (wglGetCurrentContext() == NULL) {
        return false;
    }
#elif defined(__linux__)
#define GetProcAddress(name) (decltype(name))glXGetProcAddress((const GLubyte *) #name);
#endif

    glCreateProgram = GetProcAddress(glCreateProgram);
    glDeleteProgram = GetProcAddress(glDeleteProgram);
    glUseProgram = GetProcAddress(glUseProgram);
    glAttachShader = GetProcAddress(glAttachShader);
    glLinkProgram = GetProcAddress(glLinkProgram);
    glGetProgramiv = GetProcAddress(glGetProgramiv);
    glGetProgramInfoLog = GetProcAddress(glGetProgramInfoLog);
    glGetAttribLocation = GetProcAddress(glGetAttribLocation);
    glGetUniformLocation = GetProcAddress(glGetUniformLocation);
    glGetActiveAttrib = GetProcAddress(glGetActiveAttrib);
    glGetActiveUniform = GetProcAddress(glGetActiveUniform);
    glGetUniformBlockIndex = GetProcAddress(glGetUniformBlockIndex);
    glUniformBlockBinding = GetProcAddress(glUniformBlockBinding);
    glVertexAttribPointer = GetProcAddress(glVertexAttribPointer);
    glEnableVertexAttribArray = GetProcAddress(glEnableVertexAttribArray);
    glDisableVertexAttribArray = GetProcAddress(glDisableVertexAttribArray);

    glCreateShader = GetProcAddress(glCreateShader);
    glDeleteShader = GetProcAddress(glDeleteShader);
    glShaderSource = GetProcAddress(glShaderSource);
    glCompileShader = GetProcAddress(glCompileShader);
    glGetShaderiv = GetProcAddress(glGetShaderiv);
    glGetShaderInfoLog = GetProcAddress(glGetShaderInfoLog);

#if !defined(__linux__)
    glActiveTexture = GetProcAddress(glActiveTexture);
#endif
    glGenerateMipmap = GetProcAddress(glGenerateMipmap);

    glGenBuffers = GetProcAddress(glGenBuffers);
    glDeleteBuffers = GetProcAddress(glDeleteBuffers);
    glBindBuffer = GetProcAddress(glBindBuffer);
    glBufferData = GetProcAddress(glBufferData);
    glBufferSubData = GetProcAddress(glBufferSubData);
    glBindBufferBase = GetProcAddress(glBindBufferBase);
    glBindVertexBuffer = GetProcAddress(glBindVertexBuffer);
    glCopyBufferSubData = GetProcAddress(glCopyBufferSubData);

    glMapBuffer = GetProcAddress(glMapBuffer);
    glMapBufferRange = GetProcAddress(glMapBufferRange);
    glUnmapBuffer = GetProcAddress(glUnmapBuffer);

    glGenFramebuffers = GetProcAddress(glGenFramebuffers);
    glDeleteFramebuffers = GetProcAddress(glDeleteFramebuffers);
    glBindFramebuffer = GetProcAddress(glBindFramebuffer);
    glFramebufferTexture2D = GetProcAddress(glFramebufferTexture2D);

    glGenRenderbuffers = GetProcAddress(glGenRenderbuffers);
    glDeleteRenderbuffers = GetProcAddress(glDeleteRenderbuffers);
    glBindRenderbuffer = GetProcAddress(glBindRenderbuffer);
    glRenderbufferStorage = GetProcAddress(glRenderbufferStorage);

    glFramebufferRenderbuffer = GetProcAddress(glFramebufferRenderbuffer);
    glCheckFramebufferStatus = GetProcAddress(glCheckFramebufferStatus);

    glDrawBuffers = GetProcAddress(glDrawBuffers);
    glBindFragDataLocation = GetProcAddress(glBindFragDataLocation);
    glDeleteVertexArrays = GetProcAddress(glDeleteVertexArrays);

    glGenVertexArrays = GetProcAddress(glGenVertexArrays);
    glBindVertexArray = GetProcAddress(glBindVertexArray);

    glUniform1f = GetProcAddress(glUniform1f);
    glUniform2f = GetProcAddress(glUniform2f);
    glUniform3f = GetProcAddress(glUniform3f);
    glUniform4f = GetProcAddress(glUniform4f);

    glUniform1i = GetProcAddress(glUniform1i);
    glUniform2i = GetProcAddress(glUniform2i);
    glUniform3i = GetProcAddress(glUniform3i);

    glUniform1iv = GetProcAddress(glUniform1iv);

    glUniform3fv = GetProcAddress(glUniform3fv);
    glUniform4fv = GetProcAddress(glUniform4fv);

    glUniformMatrix4fv = GetProcAddress(glUniformMatrix4fv);

    glCompressedTexImage2D = GetProcAddress(glCompressedTexImage2D);

    glTexStorage2D = GetProcAddress(glTexStorage2D);
    glTexStorage2DMultisample = GetProcAddress(glTexStorage2DMultisample);
    glRenderbufferStorageMultisample = GetProcAddress(glRenderbufferStorageMultisample);

    glDrawElementsBaseVertex = GetProcAddress(glDrawElementsBaseVertex);
    glDrawElementsInstanced = GetProcAddress(glDrawElementsInstanced);

    glDispatchCompute = GetProcAddress(glDispatchCompute);
    glMemoryBarrier = GetProcAddress(glMemoryBarrier);
    glGetBufferSubData = GetProcAddress(glGetBufferSubData);

    glTexBuffer = GetProcAddress(glTexBuffer);

    glGenQueries = GetProcAddress(glGenQueries);
    glDeleteQueries = GetProcAddress(glDeleteQueries);
    glQueryCounter = GetProcAddress(glQueryCounter);

    glGetQueryObjectiv = GetProcAddress(glGetQueryObjectiv);
    glGetQueryObjectuiv = GetProcAddress(glGetQueryObjectuiv);
    glGetQueryObjecti64v = GetProcAddress(glGetQueryObjecti64v);
    glGetQueryObjectui64v = GetProcAddress(glGetQueryObjectui64v);

    glGetStringi = GetProcAddress(glGetStringi);

    glGetInteger64v = GetProcAddress(glGetInteger64v);
    glGetBooleani_v = GetProcAddress(glGetBooleani_v);
    glGetIntegeri_v = GetProcAddress(glGetIntegeri_v);
    glGetFloati_v = GetProcAddress(glGetFloati_v);
    glGetDoublei_v = GetProcAddress(glGetDoublei_v);
    glGetInteger64i_v = GetProcAddress(glGetInteger64i_v);

    glDebugMessageCallback = GetProcAddress(glDebugMessageCallback);
#endif

    return true;
}