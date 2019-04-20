#include "ProbeStorage.h"

#include <Ren/GL.h>

ProbeStorage::ProbeStorage() : res_(0), capacity_(0) {
}

ProbeStorage::~ProbeStorage() {
    if (tex_id_ != 0xffffffff) {
        GLuint tex_id = (GLuint)tex_id_;
        glDeleteTextures(1, &tex_id);
    }
}

void ProbeStorage::Resize(int res, int capacity) {
    GLuint tex_id;
    if (tex_id_ == 0xffffffff) {
        glGenTextures(1, &tex_id);
        tex_id_ = (uint32_t)tex_id;
    } else {
        tex_id = (GLuint)tex_id_;
    }

    glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, tex_id);

}