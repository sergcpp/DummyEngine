#pragma once

#include <cstdint>

#include "SmallVector.h"
#include "Texture.h"

namespace Ren {
// TODO: Refactor this!
class ProbeStorage {
  public:
    ProbeStorage();
    ~ProbeStorage();

    ProbeStorage(const ProbeStorage &rhs) = delete;
    ProbeStorage &operator=(const ProbeStorage &rhs) = delete;

    int Allocate();
    void Free(int i);
    void Clear();

    eTexFormat format() const { return format_; }
    int res() const { return res_; }
    int size() const { return size_; }
    int capacity() const { return capacity_; }
    int max_level() const { return max_level_; }
    int reserved_temp_layer() const { return reserved_temp_layer_; }

    TexHandle handle() const { return handle_; }

    bool Resize(ApiContext *api_ctx, MemoryAllocators *mem_allocs, eTexFormat format, int res, int capacity, ILog *log);

    bool SetPixelData(int level, int layer, int face, eTexFormat format, const uint8_t *data, int data_len, ILog *log);
    bool GetPixelData(int level, int layer, int face, int buf_size, uint8_t *out_pixels, ILog *log) const;

    void Finalize();

    mutable eResState resource_state = eResState::Undefined;

  private:
    ApiContext *api_ctx_ = nullptr;
    eTexFormat format_ = eTexFormat::Undefined;
    int res_ = 0, size_ = 0, capacity_ = 0, max_level_ = 0;
    int reserved_temp_layer_ = -1;
    SmallVector<int, 32> free_indices_;
    TexHandle handle_;
#if defined(REN_VK_BACKEND)
    MemAllocation alloc_;
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
} // namespace Ren