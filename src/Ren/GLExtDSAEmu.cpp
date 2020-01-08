#include "GLExtDSAEmu.h"

void APIENTRY ren_glCreateTextures_emu(GLenum target, GLsizei n, GLuint *textures) {
    glGenTextures(n, textures);
    for (GLsizei i = 0; i < n; i++) {
        glBindTexture(target, textures[i]);
    }
}

void APIENTRY ren_glTextureStorage2D_emu(GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height) {
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexStorage2D(GL_TEXTURE_2D, levels, internalformat, width, height);
}

void APIENTRY ren_glTextureStorage2DCube_emu(GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height) {
    glBindTexture(GL_TEXTURE_CUBE_MAP, texture);
    glTexStorage2D(GL_TEXTURE_CUBE_MAP, levels, internalformat, width, height);
}

void APIENTRY ren_glTextureStorage3D_emu(GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth) {
    glBindTexture(GL_TEXTURE_3D, texture);
    glTexStorage3D(GL_TEXTURE_3D, levels, internalformat, width, height, depth);
}

void APIENTRY ren_glTextureStorage3DCube_emu(GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth) {
    glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, texture);
    glTexStorage3D(GL_TEXTURE_CUBE_MAP_ARRAY, levels, internalformat, width, height, depth);
}

void APIENTRY ren_glTextureSubImage2D_emu(
        GLuint texture, GLint level, GLint xoffset, GLint yoffset,
        GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels) {
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexSubImage2D(GL_TEXTURE_2D, level, xoffset, yoffset, width, height, format, type, pixels);
}

void APIENTRY ren_glTextureSubImage2DCube_emu(
        GLuint texture, GLint level, GLint xoffset, GLint yoffset,
        GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels) {
    glBindTexture(GL_TEXTURE_CUBE_MAP, texture);
    glTexSubImage2D(GL_TEXTURE_CUBE_MAP, level, xoffset, yoffset, width, height, format, type, pixels);
}

void APIENTRY ren_glTextureSubImage3D_emu(
        GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset,
        GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void *pixels) {
    glBindTexture(GL_TEXTURE_3D, texture);
    glTexSubImage3D(GL_TEXTURE_3D, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
}

void APIENTRY ren_glTextureSubImage3DCube_emu(
        GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset,
        GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void *pixels) {
    glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, texture);
    glTexSubImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
}

void APIENTRY ren_glCompressedTextureSubImage2D_emu(
        GLuint texture, GLint level, GLint xoffset, GLint yoffset,
        GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *data) {
    glBindTexture(GL_TEXTURE_2D, texture);
    glCompressedTexSubImage2D(GL_TEXTURE_2D, level, xoffset, yoffset, width, height, format, imageSize, data);
}

void APIENTRY ren_glTextureParameterf_emu(GLuint texture, GLenum pname, GLfloat param) {
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameterf(GL_TEXTURE_2D, pname, param);
}

void APIENTRY ren_glTextureParameterfCube_emu(GLuint texture, GLenum pname, GLfloat param) {
    glBindTexture(GL_TEXTURE_CUBE_MAP, texture);
    glTexParameterf(GL_TEXTURE_CUBE_MAP, pname, param);
}

void APIENTRY ren_glTextureParameteri_emu(GLuint texture, GLenum pname, GLint param) {
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, pname, param);
}

void APIENTRY ren_glTextureParameteriCube_emu(GLuint texture, GLenum pname, GLint param) {
    glBindTexture(GL_TEXTURE_CUBE_MAP, texture);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, pname, param);
}

void APIENTRY ren_glTextureParameterfv_emu(GLuint texture, GLenum pname, const GLfloat *params) {
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameterfv(GL_TEXTURE_2D, pname, params);
}

void APIENTRY ren_glTextureParameterfvCube_emu(GLuint texture, GLenum pname, const GLfloat *params) {
    glBindTexture(GL_TEXTURE_CUBE_MAP, texture);
    glTexParameterfv(GL_TEXTURE_CUBE_MAP, pname, params);
}

void APIENTRY ren_glTextureParameteriv_emu(GLuint texture, GLenum pname, const GLint *params) {
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteriv(GL_TEXTURE_2D, pname, params);
}

void APIENTRY ren_glTextureParameterivCube_emu(GLuint texture, GLenum pname, const GLint *params) {
    glBindTexture(GL_TEXTURE_CUBE_MAP, texture);
    glTexParameteriv(GL_TEXTURE_CUBE_MAP, pname, params);
}

void APIENTRY ren_glGenerateTextureMipmap_emu(GLuint texture) {
    glBindTexture(GL_TEXTURE_2D, texture);
    glGenerateMipmap(GL_TEXTURE_2D);
}

void APIENTRY ren_glGenerateTextureMipmapCube_emu(GLuint texture) {
    glBindTexture(GL_TEXTURE_CUBE_MAP, texture);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
}

void APIENTRY ren_glBindTextureUnit_emu(GLuint unit, GLuint texture) {
    glActiveTexture((GLenum)(GL_TEXTURE0 + unit));
    glBindTexture(GL_TEXTURE_2D, texture);
}

void APIENTRY ren_glBindTextureUnitMs_emu(GLuint unit, GLuint texture) {
    glActiveTexture((GLenum)(GL_TEXTURE0 + unit));
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, texture);
}
