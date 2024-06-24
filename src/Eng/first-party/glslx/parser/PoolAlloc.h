#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>

#include <vector>

namespace glslx {
class PoolAllocator {
    struct MemChunk {
        uint8_t *p_data;
        uint8_t first_unused_block, unused_block_count;

        MemChunk(size_t block_size, uint8_t block_count);
        ~MemChunk();

        MemChunk(const MemChunk &rhs) = delete;
        MemChunk(MemChunk &&rhs) noexcept;

        MemChunk &operator=(const MemChunk &rhs) = delete;
        MemChunk &operator=(MemChunk &&rhs) noexcept;

        void *Alloc(size_t block_size);
        void Free(void *p, size_t block_size);
    };

    size_t block_size_;
    uint8_t block_count_;

    std::vector<MemChunk> chunks_;
    MemChunk *last_alloc_chunk_, *last_free_chunk_;

  public:
    PoolAllocator(size_t block_size, uint8_t block_count)
        : block_size_(block_size), block_count_(block_count), last_alloc_chunk_(nullptr), last_free_chunk_(nullptr) {}

    void *Alloc() {
        if (!last_alloc_chunk_ || last_alloc_chunk_->unused_block_count == 0) {
            auto it = begin(chunks_);
            for (; it != end(chunks_); ++it) {
                if (it->unused_block_count) {
                    last_alloc_chunk_ = &*it;
                    break;
                }
            }
            if (it == end(chunks_)) {
                chunks_.emplace_back(block_size_, block_count_);
                last_alloc_chunk_ = &chunks_.back();
                last_free_chunk_ = &chunks_.back();
            }
        }
        assert(last_alloc_chunk_ && last_alloc_chunk_->unused_block_count);
        return last_alloc_chunk_->Alloc(block_size_);
    }

    void Free(void *p) {
        if (!last_free_chunk_ || (p < last_free_chunk_->p_data) ||
            (p >= last_free_chunk_->p_data + block_size_ * block_count_)) {
            for (auto it = begin(chunks_); it != end(chunks_); ++it) {
                if (p >= it->p_data && p < (it->p_data + block_size_ * block_count_)) {
                    last_free_chunk_ = &*it;
                    break;
                }
            }
        }
        assert(last_free_chunk_ && last_free_chunk_->unused_block_count < block_count_);
        last_free_chunk_->Free(p, block_size_);
    }
};

inline PoolAllocator::MemChunk::MemChunk(size_t _block_size, uint8_t _block_count) {
    p_data = new uint8_t[_block_size * _block_count];
    first_unused_block = 0;
    unused_block_count = _block_count;

    for (uint8_t i = 0; i < _block_count; i++) {
        // set first byte of unused block to index of next unused block
        p_data[i * _block_size] = i + 1;
    }
}

inline PoolAllocator::MemChunk::~MemChunk() { delete[] p_data; }

inline PoolAllocator::MemChunk::MemChunk(MemChunk &&rhs) noexcept {
    p_data = rhs.p_data;
    rhs.p_data = nullptr;
    first_unused_block = rhs.first_unused_block;
    unused_block_count = rhs.unused_block_count;
}

inline PoolAllocator::MemChunk &PoolAllocator::MemChunk::operator=(MemChunk &&rhs) noexcept {
    delete[] p_data;

    p_data = rhs.p_data;
    rhs.p_data = nullptr;
    first_unused_block = rhs.first_unused_block;
    unused_block_count = rhs.unused_block_count;

    return *this;
}

inline void *PoolAllocator::MemChunk::Alloc(size_t _block_size) {
    if (!unused_block_count) {
        return nullptr;
    }
    uint8_t *p_res = p_data + (first_unused_block * _block_size);
    // set to next unused block
    first_unused_block = *p_res;
    --unused_block_count;
    return p_res;
}

inline void PoolAllocator::MemChunk::Free(void *p, size_t _block_size) {
    assert(p >= p_data);
    auto *p_mem_to_release = (uint8_t *)p;
    // check if pointer is aligned to block size
    assert((p_mem_to_release - p_data) % _block_size == 0);
    (*p_mem_to_release) = first_unused_block;
    first_unused_block = uint8_t((p_mem_to_release - p_data) / _block_size);
    // check if result fits uint8_t
    assert(first_unused_block == ((p_mem_to_release - p_data) / _block_size));
    ++unused_block_count;
}

struct SharedState {
    uint32_t users_count = 0;
    std::vector<PoolAllocator> allocators;
};

template <typename T, typename FallBackAllocator = std::allocator<T>> class MultiPoolAllocator {
    template <typename U, typename FallBackAllocator2> friend class MultiPoolAllocator;

    size_t mem_step_;
    size_t max_object_size_;
    SharedState *shared_state_;
    FallBackAllocator fallback_allocator_;

  public:
    MultiPoolAllocator(size_t mem_step, size_t max_object_size)
        : mem_step_(mem_step), max_object_size_(max_object_size) {
        shared_state_ = new SharedState;
        shared_state_->users_count = 1;

        for (size_t s = mem_step; s < max_object_size + mem_step; s += mem_step) {
            shared_state_->allocators.emplace_back(s, 255);
        }
    }

    MultiPoolAllocator(const MultiPoolAllocator &other)
        : mem_step_(other.mem_step_), max_object_size_(other.max_object_size_), shared_state_(other.shared_state_),
          fallback_allocator_(other.fallback_allocator_) {
        shared_state_->users_count++;
    }

    MultiPoolAllocator &operator=(const MultiPoolAllocator &) = delete;

    template <typename U>
    /*explicit*/ MultiPoolAllocator(const MultiPoolAllocator<U> &other)
        : mem_step_(other.mem_step_), max_object_size_(other.max_object_size_), shared_state_(other.shared_state_),
          fallback_allocator_(other.fallback_allocator_) {
        shared_state_->users_count++;
    }

    ~MultiPoolAllocator() {
        if (shared_state_ && !--shared_state_->users_count) {
            delete shared_state_;
        }
    }

    T *allocate(const size_t n) {
        if (n == 0)
            return nullptr;

        if (sizeof(T) * n > max_object_size_) {
            return fallback_allocator_.allocate(n);
        } else {
            size_t i = (sizeof(T) * n + mem_step_ - 1) / mem_step_ - 1;
            return (T *)shared_state_->allocators[i].Alloc();
        }
    }

    void deallocate(T *const p, const size_t n) {
        if (sizeof(T) * n > max_object_size_) {
            fallback_allocator_.deallocate(p, n);
        } else {
            size_t i = (sizeof(T) * n + mem_step_ - 1) / mem_step_ - 1;
            shared_state_->allocators[i].Free(p);
        }
    }

    // The following will be the same for virtually all allocators.
    typedef T *pointer;
    typedef const T *const_pointer;
    typedef T &reference;
    typedef const T &const_reference;
    typedef T value_type;
    typedef std::size_t size_type;
    typedef ptrdiff_t difference_type;

    T *address(T &r) const { return &r; }

    const T *address(const T &s) const { return &s; }

    std::size_t max_size() const {
        // The following has been carefully written to be independent of
        // the definition of size_t and to avoid signed/unsigned warnings.
        return (static_cast<std::size_t>(0) - static_cast<std::size_t>(1)) / sizeof(T);
    }

    // The following must be the same for all allocators.
    template <typename U> struct rebind {
        typedef MultiPoolAllocator<U> other;
    };

    bool operator!=(const MultiPoolAllocator &other) const { return !(*this == other); }

    template <class U, class... Args> void construct(U *p, Args &&...args) {
        ::new ((void *)p) U(std::forward<Args>(args)...);
    }

    template <class U> void destroy(U *p) { p->~U(); }

    // Returns true if and only if storage allocated from *this
    // can be deallocated from other, and vice versa.
    bool operator==([[maybe_unused]] const MultiPoolAllocator &other) const { return true; }
};
} // namespace glslx