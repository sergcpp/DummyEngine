#include "GLExtDSAEmu.h"

void APIENTRY ren_glCreateTextures_emu(GLenum target, GLsizei n, GLuint *textures) {
    glGenTextures(n, textures);
    for (GLsizei i = 0; i < n; i++) {
        glBindTexture(target, textures[i]);
    }
}

void APIENTRY ren_glTextureStorage2D_Comp_emu(GLenum target, GLuint texture, GLsizei levels, GLenum internalformat,
                                              GLsizei width, GLsizei height) {
    glBindTexture(target, texture);
    glTexStorage2D(target, levels, internalformat, width, height);
}

void APIENTRY ren_glTextureStorage3D_Comp_emu(GLenum target, GLuint texture, GLsizei levels, GLenum internalformat,
                                              GLsizei width, GLsizei height, GLsizei depth) {
    glBindTexture(target, texture);
    glTexStorage3D(target, levels, internalformat, width, height, depth);
}

void APIENTRY ren_glTextureSubImage2D_Comp_emu(GLenum target, GLuint texture, GLint level, GLint xoffset, GLint yoffset,
                                               GLsizei width, GLsizei height, GLenum format, GLenum type,
                                               const void *pixels) {
    glBindTexture(target, texture);
    glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
}

void APIENTRY ren_glTextureSubImage3D_Comp_emu(GLenum target, GLuint texture, GLint level, GLint xoffset, GLint yoffset,
                                               GLint zoffset, GLsizei width, GLsizei height, GLsizei depth,
                                               GLenum format, GLenum type, const void *pixels) {
    if (target >= GL_TEXTURE_CUBE_MAP_POSITIVE_X && target <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z) {
        glBindTexture(GL_TEXTURE_CUBE_MAP, texture);
        glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
    } else {
        glBindTexture(target, texture);
        glTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
    }
}

void APIENTRY ren_glCompressedTextureSubImage2D_Comp_emu(GLenum target, GLuint texture, GLint level, GLint xoffset,
                                                         GLint yoffset, GLsizei width, GLsizei height, GLenum format,
                                                         GLsizei imageSize, const void *data) {
    glBindTexture(target, texture);
    glCompressedTexSubImage2D(target, level, xoffset, yoffset, width, height, format, imageSize, data);
}

void APIENTRY ren_glCompressedTextureSubImage3D_Comp_emu(GLenum target, GLuint texture, GLint level, GLint xoffset,
                                                         GLint yoffset, GLint zoffset, GLsizei width, GLsizei height,
                                                         GLsizei depth, GLenum format, GLsizei imageSize,
                                                         const void *data) {
    glBindTexture(target, texture);
    glCompressedTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, data);
}

void APIENTRY ren_glTextureParameterf_Comp_emu(GLenum target, GLuint texture, GLenum pname, GLfloat param) {
    glBindTexture(target, texture);
    glTexParameterf(target, pname, param);
}

void APIENTRY ren_glTextureParameteri_Comp_emu(GLenum target, GLuint texture, GLenum pname, GLint param) {
    glBindTexture(target, texture);
    glTexParameteri(target, pname, param);
}

void APIENTRY ren_glTextureParameterfv_Comp_emu(GLenum target, GLuint texture, GLenum pname, const GLfloat *params) {
    glBindTexture(target, texture);
    glTexParameterfv(target, pname, params);
}

void APIENTRY ren_glTextureParameteriv_Comp_emu(GLenum target, GLuint texture, GLenum pname, const GLint *params) {
    glBindTexture(target, texture);
    glTexParameteriv(target, pname, params);
}

void APIENTRY ren_glGenerateTextureMipmap_Comp_emu(GLenum target, GLuint texture) {
    glBindTexture(target, texture);
    glGenerateMipmap(target);
}

void APIENTRY ren_glBindTextureUnit_Comp_emu(GLenum target, GLuint unit, GLuint texture) {
    glActiveTexture(GLenum(GL_TEXTURE0 + unit));
    glBindTexture(target, texture);
}

#ifndef __ANDROID__
void APIENTRY ren_glNamedBufferStorage_Comp_emu(GLenum target, GLuint buffer, GLsizeiptr size, const void *data,
                                                GLbitfield flags) {
    glBindBuffer(target, buffer);
    glBufferStorage(target, size, data, flags);
}
#endif