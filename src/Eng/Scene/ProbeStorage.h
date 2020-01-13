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

    Ren::eTexColorFormat format() const { return format_; }
    int res() const { return res_; }
    int size() const { return size_; }
    int capacity() const { return capacity_; }
    int max_level() const { return max_level_; }
    int reserved_temp_layer() const { return reserved_temp_layer_; }

#if defined(USE_GL_RENDER)
    uint32_t tex_id() const { return tex_id_; }
#endif

    void Resize(Ren::eTexColorFormat format, int res, int capacity, Ren::ILog *log);

    bool SetPixelData(int level, int layer, int face, Ren::eTexColorFormat format,
            const uint8_t *data, int data_len, Ren::ILog *log);
    bool GetPixelData(int level, int layer, int face, int buf_size, uint8_t *out_pixels, Ren::ILog *log) const;
private:
    Ren::eTexColorFormat format_;
    int res_, size_, capacity_, max_level_;
    int reserved_temp_layer_;
    std::vector<int> free_indices_;
#if defined(USE_GL_RENDER)
    uint32_t tex_id_ = 0;
#endif
};