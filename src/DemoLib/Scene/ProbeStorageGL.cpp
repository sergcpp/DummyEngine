#include "ProbeStorage.h"

#include <Ren/GL.h>

ProbeStorage::ProbeStorage() : res_(0), size_(0), capacity_(0) {
}

ProbeStorage::~ProbeStorage() {
    if (tex_id_ != 0xffffffff) {
        GLuint tex_id = (GLuint)tex_id_;
        glDeleteTextures(1, &tex_id);
    }
}

int ProbeStorage::Allocate() {
    if (!free_indices_.empty()) {
        int ret = free_indices_.back();
        free_indices_.pop_back();
        return ret;
    } else {
        return size_++;
    }
}

void ProbeStorage::Free(int i) {
    if (i == size_ - 1) {
        size_--;
    } else {
        free_indices_.push_back(i);
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
        
    int _res = res;
    int level = 0;
    while (_res >= 4) {
        glTexImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, level, GL_RGBA8, _res, _res, capacity * 6, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        _res = _res / 2;
        level++;
    }
    
    glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MAX_LEVEL, level - 1);

    glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

#if !defined(__ANDROID__)
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
#endif

    res_ = res;
    capacity_ = capacity;
    max_level_ = level - 1;
}