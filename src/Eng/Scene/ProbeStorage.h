#pragma once

#include <cstdint>

#include <Ren/Texture.h>
#include <Sys/SmallVector.h>

class ProbeStorage {
  public:
    ProbeStorage();
    ~ProbeStorage();

    ProbeStorage(const ProbeStorage &rhs) = delete;
    ProbeStorage &operator=(const ProbeStorage &rhs) = delete;

    int Allocate();
    void Free(int i);
    void Clear();

    Ren::eTexFormat format() const { return format_; }
    int res() const { return res_; }
    int size() const { return size_; }
    int capacity() const { return capacity_; }
    int max_level() const { return max_level_; }
    int reserved_temp_layer() const { return reserved_temp_layer_; }

    Ren::TexHandle handle() const { return handle_; }

    bool Resize(Ren::ApiContext *api_ctx, Ren::MemoryAllocators *mem_allocs, Ren::eTexFormat format, int res,
                int capacity, Ren::ILog *log);

    bool SetPixelData(int level, int layer, int face, Ren::eTexFormat format, const uint8_t *data, int data_len,
                      Ren::ILog *log);
    bool GetPixelData(int level, int layer, int face, int buf_size, uint8_t *out_pixels, Ren::ILog *log) const;

    void Finalize();

    mutable Ren::eResState resource_state = Ren::eResState::Undefined;
  private:
    Ren::ApiContext *api_ctx_ = nullptr;
    Ren::eTexFormat format_ = Ren::eTexFormat::Undefined;
    int res_ = 0, size_ = 0, capacity_ = 0, max_level_ = 0;
    int reserved_temp_layer_ = -1;
    Sys::SmallVector<int, 32> free_indices_;
    Ren::TexHandle handle_;
#if defined(USE_VK_RENDER)
    Ren::MemAllocation alloc_;
#endif

    void Destroy();
};

inline int ProbeStorage::Allocate() {
    if (!free_indices_.empty()) {
        const int ret = free_indices_.back();
        free_indices_.pop_back();
        return ret;
    } else {
        if (size_ < capacity_ - 1) {
            return size_++;
        } else {
            return -1;
        }
    }
}

inline void ProbeStorage::Free(int i) {
    if (i == size_ - 1) {
        size_--;
    } else {
        free_indices_.push_back(i);
    }
}

inline void ProbeStorage::Clear() {
    size_ = 0;
    free_indices_.clear();
}