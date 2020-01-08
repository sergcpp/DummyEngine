#pragma once

#include "GL.h"

void APIENTRY ren_glCreateTextures_emu(GLenum target, GLsizei n, GLuint *textures);

void APIENTRY ren_glTextureStorage2D_emu(GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height);
void APIENTRY ren_glTextureStorage2DCube_emu(GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height);

void APIENTRY ren_glTextureStorage3D_emu(GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth);
void APIENTRY ren_glTextureStorage3DCube_emu(GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth);

void APIENTRY ren_glTextureSubImage2D_emu(
        GLuint texture, GLint level, GLint xoffset, GLint yoffset,
        GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels);
void APIENTRY ren_glTextureSubImage2DCube_emu(
        GLuint texture, GLint level, GLint xoffset, GLint yoffset,
        GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels);

void APIENTRY ren_glTextureSubImage3D_emu(
    GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset,
    GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void *pixels);
void APIENTRY ren_glTextureSubImage3DCube_emu(
    GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset,
    GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void *pixels);

void APIENTRY ren_glCompressedTextureSubImage2D_emu(
    GLuint texture, GLint level, GLint xoffset, GLint yoffset,
    GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *data);

void APIENTRY ren_glTextureParameterf_emu(GLuint texture, GLenum pname, GLfloat param);
void APIENTRY ren_glTextureParameterfCube_emu(GLuint texture, GLenum pname, GLfloat param);
void APIENTRY ren_glTextureParameteri_emu(GLuint texture, GLenum pname, GLint param);
void APIENTRY ren_glTextureParameteriCube_emu(GLuint texture, GLenum pname, GLint param);

void APIENTRY ren_glTextureParameterfv_emu(GLuint texture, GLenum pname, const GLfloat *params);
void APIENTRY ren_glTextureParameterfvCube_emu(GLuint texture, GLenum pname, const GLfloat *params);
void APIENTRY ren_glTextureParameteriv_emu(GLuint texture, GLenum pname, const GLint *params);
void APIENTRY ren_glTextureParameterivCube_emu(GLuint texture, GLenum pname, const GLint *params);

void APIENTRY ren_glGenerateTextureMipmap_emu(GLuint texture);
void APIENTRY ren_glGenerateTextureMipmapCube_emu(GLuint texture);

void APIENTRY ren_glBindTextureUnit_emu(GLuint unit, GLuint texture);
void APIENTRY ren_glBindTextureUnitMs_emu(GLuint unit, GLuint texture);