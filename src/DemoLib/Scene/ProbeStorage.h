#pragma once

#include <cstdint>

class ProbeStorage {
public:
    ProbeStorage();
    ~ProbeStorage();

    ProbeStorage(const ProbeStorage &rhs) = delete;
    ProbeStorage &operator=(const ProbeStorage &rhs) = delete;

    int Allocate();
    void Free(int i);

    int res() const { return res_; }
    int size() const { return size_; }
    int capacity() const { return capacity_; }
    int max_level() const { return max_level_; }

#if defined(USE_GL_RENDER)
    uint32_t tex_id() const { return tex_id_; }
#endif

    void Resize(int res, int capacity);

private:
    int res_, size_, capacity_, max_level_;
    std::vector<int> free_indices_;
#if defined(USE_GL_RENDER)
    uint32_t tex_id_ = 0xffffffff;
#endif
};