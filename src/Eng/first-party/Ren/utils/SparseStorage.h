#pragma once

#include <cassert>
#include <cstring>
#include <iterator>
#include <utility>

#include "SparseArray.h"

namespace Ren {
static const int RWTag = 0;
static const int ROTag = 1;

template <typename T, int Tag = RWTag> struct Handle {
    uint32_t index : 24;
    uint32_t generation : 8;

    Handle() : index(0x00ffffffu), generation(0u) {}
    Handle(const uint32_t _index, const uint32_t _generation) : index(_index), generation(_generation) {}
    explicit Handle(const Handle<void> &_opaque) : index(_opaque.index), generation(_opaque.generation) {}

    explicit operator bool() const { return index != 0xffffffu; }
    explicit operator uint32_t() const { return (index << 8) | generation; }

    bool operator==(const Handle &rhs) const { return index == rhs.index && generation == rhs.generation; }
    bool operator!=(const Handle &rhs) const { return index != rhs.index || generation != rhs.generation; }
    bool operator<(const Handle &rhs) const {
        if (index < rhs.index) {
            return true;
        } else if (index == rhs.index) {
            return generation < rhs.generation;
        }
        return false;
    }
    bool operator>(const Handle &rhs) const {
        if (index > rhs.index) {
            return true;
        } else if (index == rhs.index) {
            return generation > rhs.generation;
        }
        return false;
    }

    template <int HigherTag, typename = std::enable_if_t<(HigherTag > Tag)>> operator Handle<T, HigherTag>() const {
        return Handle<T, HigherTag>{index, generation};
    }

    explicit operator Handle<void>() const { return Handle<void>{index, generation}; }
};

template <typename T, int Alignment = alignof(T), typename Allocator = aligned_allocator<uint64_t, Alignment>>
class SparseStorage : Allocator {
  protected:
    uint64_t *ctrl_;
    uint8_t *generation_;
    T *data_;
    uint32_t capacity_, size_;
    uint32_t first_free_;

    static_assert(sizeof(T) >= sizeof(uint32_t));

    static size_t ctrl_size(const uint32_t cap) {
        const uint32_t word_count = (cap + 63) / 64;
        return word_count * sizeof(uint64_t);
    }

    static size_t generation_size(const uint32_t cap) { return cap * sizeof(uint8_t); }

    static size_t mem_size(const uint32_t cap) {
        size_t mem_size = ctrl_size(cap);
        mem_size += generation_size(cap);
        if (mem_size % alignof(T)) {
            mem_size += alignof(T) - (mem_size % alignof(T));
        }
        mem_size += sizeof(T) * cap;
        return mem_size;
    }

  public:
    SparseStorage() : SparseStorage(0) {}
    explicit SparseStorage(const uint32_t initial_capacity)
        : ctrl_(nullptr), generation_(nullptr), data_(nullptr), capacity_(0), size_(0), first_free_(0) {
        if (initial_capacity) {
            Reserve(initial_capacity);
        }
    }

    ~SparseStorage() {
        Clear();
        this->deallocate(ctrl_, mem_size(capacity_));
    }

    SparseStorage(const SparseStorage &rhs)
        : ctrl_(nullptr), generation_(nullptr), data_(nullptr), capacity_(0), size_(0), first_free_(0) {
        Reserve(rhs.capacity_);
        if (!ctrl_ || !rhs.ctrl_) {
            return;
        }

        memcpy(ctrl_, rhs.ctrl_, ctrl_size(rhs.capacity_));
        memcpy(generation_, rhs.generation_, generation_size(rhs.capacity_));

        for (uint32_t i = 0; i < rhs.capacity_; ++i) {
            if (ctrl_[i / 64] & (1ull << (i % 64))) {
                new (&data_[i]) T(rhs.data_[i]);
            } else {
                // copy next free index
                memcpy(data_ + i, rhs.data_ + i, sizeof(uint32_t));
            }
        }

        first_free_ = rhs.first_free_;
        size_ = rhs.size_;
    }

    SparseStorage(SparseStorage &&rhs) noexcept
        : Allocator(static_cast<Allocator &&>(rhs)), ctrl_(nullptr), generation_(nullptr), data_(nullptr), capacity_(0),
          size_(0), first_free_(0) {
        ctrl_ = std::exchange(rhs.ctrl_, nullptr);
        generation_ = std::exchange(rhs.generation_, nullptr);
        data_ = std::exchange(rhs.data_, nullptr);

        capacity_ = std::exchange(rhs.capacity_, 0);
        size_ = std::exchange(rhs.size_, 0);
        first_free_ = std::exchange(rhs.first_free_, 0);
    }

    SparseStorage &operator=(const SparseStorage &rhs) {
        if (this == &rhs) {
            return *this;
        }
        Allocator::operator=(static_cast<Allocator &>(rhs));

        Clear();
        this->deallocate(ctrl_, mem_size(capacity_));

        ctrl_ = nullptr;
        generation_ = nullptr;
        data_ = nullptr;
        capacity_ = 0;

        Reserve(rhs.capacity_);

        if (rhs.ctrl_) {
            memcpy(ctrl_, rhs.ctrl_, ctrl_size(capacity_));
            memcpy(generation_, rhs.generation_, generation_size(capacity_));
        }

        for (uint32_t i = 0; i < rhs.capacity_; ++i) {
            if (ctrl_[i / 64] & (1ull << (i % 64))) {
                new (&data_[i]) T(rhs.data_[i]);
            } else {
                // copy next free index
                memcpy(data_ + i, rhs.data_ + i, sizeof(uint32_t));
            }
        }

        first_free_ = rhs.first_free_;
        size_ = rhs.size_;

        return (*this);
    }

    SparseStorage &operator=(SparseStorage &&rhs) noexcept {
        if (this == &rhs) {
            return *this;
        }

        Clear();
        this->deallocate(ctrl_, mem_size(capacity_));

        ctrl_ = std::exchange(rhs.ctrl_, nullptr);
        generation_ = std::exchange(rhs.generation_, nullptr);
        data_ = std::exchange(rhs.data_, nullptr);

        capacity_ = std::exchange(rhs.capacity_, 0);
        size_ = std::exchange(rhs.size_, 0);
        first_free_ = std::exchange(rhs.first_free_, 0);

        return (*this);
    }

    uint32_t size() const { return size_; }
    uint32_t capacity() const { return capacity_; }

    bool empty() const { return size_ == 0; }

    uint8_t *generation() { return generation_; }
    const uint8_t *generation() const { return generation_; }

    T *data() { return data_; }
    const T *data() const { return data_; }

    void Clear() {
        const uint32_t word_count = (capacity_ + 63) / 64;
        for (uint32_t i = 0; i < word_count && size_; ++i) {
            uint64_t mask = ctrl_[i];
            while (mask) {
                EraseUnsafe(64 * i + CountTrailingZeroes(mask));
                mask &= mask - 1;
            }
        }
    }

    void Reserve(const uint32_t new_capacity) {
        if (new_capacity <= capacity_) {
            return;
        }

        uint64_t *old_ctrl = ctrl_;
        uint8_t *old_generation = generation_;
        T *old_data = data_;
        const uint32_t old_word_count = (capacity_ + 63) / 64;
        const uint32_t new_word_count = (new_capacity + 63) / 64;

        size_t total_mem = ctrl_size(new_capacity);
        const size_t generation_start = total_mem;
        total_mem += generation_size(new_capacity);
        if (total_mem % alignof(T)) {
            total_mem += alignof(T) - (total_mem % alignof(T));
        }
        const size_t data_start = total_mem;
        total_mem += sizeof(T) * new_capacity;
        assert(total_mem == mem_size(new_capacity));

        ctrl_ = this->allocate(total_mem);
        generation_ = reinterpret_cast<uint8_t *>(uintptr_t(ctrl_) + generation_start);
        data_ = reinterpret_cast<T *>(uintptr_t(ctrl_) + data_start);
        assert(uintptr_t(data_) % alignof(T) == 0);

        // copy old control and generation
        if (old_ctrl) {
            memcpy(ctrl_, old_ctrl, old_word_count * sizeof(uint64_t));
            memcpy(generation_, old_generation, capacity_ * sizeof(uint8_t));
        }
        // fill rest with zeroes
        memset(ctrl_ + old_word_count, 0, (new_word_count - old_word_count) * sizeof(uint64_t));
        memset(generation_ + capacity_, 0, (new_capacity - capacity_) * sizeof(uint8_t));

        // move old data
        for (uint32_t i = 0; i < capacity_; ++i) {
            if (ctrl_[i / 64] & (1ull << (i % 64))) {
                new (&data_[i]) T(std::move(old_data[i]));
                old_data[i].~T();
            } else {
                // copy next free index
                memcpy(data_ + i, old_data + i, sizeof(uint32_t));
            }
        }

        this->deallocate(old_ctrl, mem_size(capacity_));

        for (uint32_t i = capacity_; i < new_capacity - 1; i++) {
            const uint32_t next_free = (i + 1);
            assert(next_free < 0xffffffu);
            memcpy(data_ + i, &next_free, sizeof(uint32_t));
        }

        memcpy(data_ + new_capacity - 1, &first_free_, sizeof(uint32_t));
        first_free_ = capacity_;

        capacity_ = new_capacity;
    }

    template <class... Args> Handle<T, RWTag> Emplace(Args &&...args) {
        if (size_ + 1 > capacity_) {
            Reserve(capacity_ ? (capacity_ * 2) : 64);
        }
        const uint32_t index = first_free_;
        assert((ctrl_[index / 64] & (1ull << (index % 64))) == 0);
        memcpy(&first_free_, data_ + index, sizeof(uint32_t));

        T *el = data_ + index;
        new (el) T(std::forward<Args>(args)...);

        ctrl_[index / 64] |= (1ull << (index % 64));

        ++size_;
        return Handle<T, RWTag>{index, generation_[index]};
    }

    Handle<T, RWTag> Push(const T &el) {
        if (size_ + 1 > capacity_) {
            Reserve(capacity_ ? (capacity_ * 2) : 64);
        }
        const uint32_t index = first_free_;
        assert((ctrl_[index / 64] & (1ull << (index % 64))) == 0);
        memcpy(&first_free_, data_ + index, sizeof(uint32_t));

        new (&data_[index]) T(el);
        ctrl_[index / 64] |= (1ull << (index % 64));

        ++size_;
        return Handle<T, RWTag>{index, generation_[index]};
    }

    void Erase(const Handle<T, RWTag> handle) {
        assert((ctrl_[handle.index / 64] & (1ull << (handle.index % 64))) && "Invalid index!");

        data_[handle.index].~T();
        ctrl_[handle.index / 64] &= ~(1ull << (handle.index % 64));
        ++generation_[handle.index];

        memcpy(data_ + handle.index, &first_free_, sizeof(uint32_t));
        first_free_ = handle.index;
        --size_;
    }

    void EraseUnsafe(const uint32_t index) {
        assert((ctrl_[index / 64] & (1ull << (index % 64))) && "Invalid index!");

        data_[index].~T();
        ctrl_[index / 64] &= ~(1ull << (index % 64));
        ++generation_[index];

        memcpy(data_ + index, &first_free_, sizeof(uint32_t));
        first_free_ = index;
        --size_;
    }

    T &operator[](const Handle<T, RWTag> handle) {
        assert((ctrl_[handle.index / 64] & (1ull << (handle.index % 64))) && "Invalid handle!");
        assert((generation_[handle.index] == handle.generation) && "Invalid handle!");
        return data_[handle.index];
    }

    const T &operator[](const Handle<T, ROTag> handle) const {
        assert((ctrl_[handle.index / 64] & (1ull << (handle.index % 64))) && "Invalid handle!");
        assert((generation_[handle.index] == handle.generation) && "Invalid handle!");
        return data_[handle.index];
    }

    T *TryGet(const Handle<T, RWTag> handle) {
        assert((ctrl_[handle.index / 64] & (1ull << (handle.index % 64))) && "Invalid handle!");
        if (generation_[handle.index] == handle.generation) {
            return &data_[handle.index];
        }
        return nullptr;
    }

    const T *TryGet(const Handle<T, ROTag> handle) const {
        assert((ctrl_[handle.index / 64] & (1ull << (handle.index % 64))) && "Invalid handle!");
        if (generation_[handle.index] == handle.generation) {
            return &data_[handle.index];
        }
        return nullptr;
    }

    T &GetUnsafe(const uint32_t index) {
        assert((ctrl_[index / 64] & (1ull << (index % 64))) && "Invalid index!");
        return data_[index];
    }

    const T &GetUnsafe(const uint32_t index) const {
        assert((ctrl_[index / 64] & (1ull << (index % 64))) && "Invalid index!");
        return data_[index];
    }

    bool IsOccupied(const uint32_t index) const { return (ctrl_[index / 64] & (1ull << (index % 64))) != 0; }

    uint32_t GetGeneration(const uint32_t index) const {
        assert((ctrl_[index / 64] & (1ull << (index % 64))) && "Invalid index!");
        return generation_[index];
    }

    void SetGeneration(const uint32_t index, const uint8_t generation) const {
        assert((ctrl_[index / 64] & (1ull << (index % 64))) && "Invalid index!");
        generation_[index] = generation;
    }

    class SparseStorageIterator {
        friend class SparseStorage<T, Alignment>;

        SparseStorage<T, Alignment> *container_;
        uint32_t index_;

        SparseStorageIterator(SparseStorage<T, Alignment> *container, uint32_t index)
            : container_(container), index_(index) {}

      public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = T *;
        using reference = T &;

        T &operator*() { return container_->GetUnsafe(index_); }
        T *operator->() { return &container_->GetUnsafe(index_); }
        SparseStorageIterator &operator++() {
            index_ = container_->NextOccupied(index_);
            return *this;
        }
        SparseStorageIterator operator++(int) {
            SparseStorageIterator tmp(*this);
            ++(*this);
            return tmp;
        }

        Handle<T, RWTag> handle() const { return Handle<T, RWTag>{index_, container_->GetGeneration(index_)}; }

        bool operator<(const SparseStorageIterator &rhs) const { return index_ < rhs.index_; }
        bool operator<=(const SparseStorageIterator &rhs) const { return index_ <= rhs.index_; }
        bool operator>(const SparseStorageIterator &rhs) const { return index_ > rhs.index_; }
        bool operator>=(const SparseStorageIterator &rhs) const { return index_ >= rhs.index_; }
        bool operator==(const SparseStorageIterator &rhs) const { return index_ == rhs.index_; }
        bool operator!=(const SparseStorageIterator &rhs) const { return index_ != rhs.index_; }
    };

    class SparseStorageConstIterator {
        friend class SparseStorage<T, Alignment>;

        const SparseStorage<T, Alignment> *container_;
        uint32_t index_;

        SparseStorageConstIterator(const SparseStorage<T, Alignment> *container, uint32_t index)
            : container_(container), index_(index) {}

      public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = T *;
        using reference = T &;

        const T &operator*() const { return container_->GetUnsafe(index_); }
        const T *operator->() const { return &container_->GetUnsafe(index_); }
        SparseStorageConstIterator &operator++() {
            index_ = container_->NextOccupied(index_);
            return *this;
        }

        SparseStorageConstIterator operator++(int) {
            SparseStorageConstIterator tmp(*this);
            ++(*this);
            return tmp;
        }

        Handle<T, ROTag> handle() const { return Handle<T, ROTag>{index_, container_->GetGeneration(index_)}; }

        bool operator<(const SparseStorageConstIterator &rhs) const { return index_ < rhs.index_; }
        bool operator<=(const SparseStorageConstIterator &rhs) const { return index_ <= rhs.index_; }
        bool operator>(const SparseStorageConstIterator &rhs) const { return index_ > rhs.index_; }
        bool operator>=(const SparseStorageConstIterator &rhs) const { return index_ >= rhs.index_; }
        bool operator==(const SparseStorageConstIterator &rhs) const { return index_ == rhs.index_; }
        bool operator!=(const SparseStorageConstIterator &rhs) const { return index_ != rhs.index_; }
    };

    using iterator = SparseStorageIterator;
    using const_iterator = SparseStorageConstIterator;

    iterator begin() {
        if (!empty()) {
            const uint32_t word_count = (capacity_ + 63) / 64;
            for (uint32_t i = 0; i < word_count; ++i) {
                const uint64_t mask = ctrl_[i];
                if (mask) {
                    return iterator(this, 64 * i + CountTrailingZeroes(mask));
                }
            }
        }
        return end();
    }
    const_iterator begin() const { return cbegin(); }

    const_iterator cbegin() const {
        if (!empty()) {
            const uint32_t word_count = (capacity_ + 63) / 64;
            for (uint32_t i = 0; i < word_count; ++i) {
                const uint64_t mask = ctrl_[i];
                if (mask) {
                    return const_iterator(this, 64 * i + CountTrailingZeroes(mask));
                }
            }
        }
        return cend();
    }

    iterator end() { return iterator(this, capacity_); }
    const_iterator end() const { return cend(); }
    const_iterator cend() const { return const_iterator(this, capacity_); }

    iterator iter_at(const uint32_t i) { return iterator(this, i); }
    const_iterator citer_at(const uint32_t i) const { return const_iterator(this, i); }

    iterator Erase(iterator it) {
        const uint32_t next_index = NextOccupied(it.index_);
        EraseUnsafe(it.index_);
        return iterator(this, next_index);
    }

  private:
    uint32_t NextOccupied(uint32_t index) const {
        assert((ctrl_[index / 64] & (1ull << (index % 64))) && "Invalid index!");

        if (++index >= capacity_) {
            return capacity_;
        }

        const uint32_t start_word = index / 64;
        const uint32_t word_count = (capacity_ + 63) / 64;

        { // Check first word
            uint64_t mask = ctrl_[start_word] & (~0ull << (index % 64));
            if (mask) {
                return 64 * start_word + CountTrailingZeroes(mask);
            }
        }

        for (uint32_t i = start_word + 1; i < word_count; ++i) {
            const uint64_t mask = ctrl_[i];
            if (mask) {
                return 64 * i + CountTrailingZeroes(mask);
            }
        }

        return capacity_;
    }
};

template <int A, int B> constexpr int _max_value = (A > B) ? A : B;

template <typename T, typename U, int Alignment = _max_value<alignof(T), alignof(U)>,
          typename Allocator = aligned_allocator<uint64_t, Alignment>>
class SparseDualStorage : Allocator {
  protected:
    uint64_t *ctrl_;
    uint8_t *generation_;
    T *data_main_;
    U *data_cold_;
    uint32_t capacity_, size_;
    uint32_t first_free_;

    static_assert(sizeof(T) >= sizeof(uint32_t));

    static size_t ctrl_size(const uint32_t cap) {
        const uint32_t word_count = (cap + 63) / 64;
        return word_count * sizeof(uint64_t);
    }

    static size_t generation_size(const uint32_t cap) { return cap * sizeof(uint8_t); }

    static size_t mem_size(const uint32_t cap) {
        size_t mem_size = ctrl_size(cap);
        mem_size += generation_size(cap);
        if (mem_size % alignof(T)) {
            mem_size += alignof(T) - (mem_size % alignof(T));
        }
        mem_size += sizeof(T) * cap;
        if (mem_size % alignof(U)) {
            mem_size += alignof(U) - (mem_size % alignof(U));
        }
        mem_size += sizeof(U) * cap;
        return mem_size;
    }

  public:
    SparseDualStorage() : SparseDualStorage(0) {}
    explicit SparseDualStorage(const uint32_t initial_capacity)
        : ctrl_(nullptr), generation_(nullptr), data_main_(nullptr), data_cold_(nullptr), capacity_(0), size_(0),
          first_free_(0) {
        if (initial_capacity) {
            Reserve(initial_capacity);
        }
    }

    ~SparseDualStorage() {
        Clear();
        this->deallocate(ctrl_, mem_size(capacity_));
    }

    SparseDualStorage(const SparseDualStorage &rhs)
        : ctrl_(nullptr), generation_(nullptr), data_main_(nullptr), data_cold_(nullptr), capacity_(0), size_(0),
          first_free_(0) {
        Reserve(rhs.capacity_);
        if (!ctrl_ || !rhs.ctrl_) {
            return;
        }

        memcpy(ctrl_, rhs.ctrl_, ctrl_size(rhs.capacity_));
        memcpy(generation_, rhs.generation_, generation_size(rhs.capacity_));

        for (uint32_t i = 0; i < rhs.capacity_; ++i) {
            if (ctrl_[i / 64] & (1ull << (i % 64))) {
                new (&data_main_[i]) T(rhs.data_main_[i]);
                new (&data_cold_[i]) U(rhs.data_cold_[i]);
            } else {
                // copy next free index
                memcpy(data_main_ + i, rhs.data_main_ + i, sizeof(uint32_t));
            }
        }

        first_free_ = rhs.first_free_;
        size_ = rhs.size_;
    }

    SparseDualStorage(SparseDualStorage &&rhs) noexcept
        : Allocator(static_cast<Allocator &&>(rhs)), ctrl_(nullptr), generation_(nullptr), data_main_(nullptr),
          data_cold_(nullptr), capacity_(0), size_(0), first_free_(0) {
        ctrl_ = std::exchange(rhs.ctrl_, nullptr);
        generation_ = std::exchange(rhs.generation_, nullptr);
        data_main_ = std::exchange(rhs.data_main_, nullptr);
        data_cold_ = std::exchange(rhs.data_cold_, nullptr);

        capacity_ = std::exchange(rhs.capacity_, 0);
        size_ = std::exchange(rhs.size_, 0);
        first_free_ = std::exchange(rhs.first_free_, 0);
    }

    SparseDualStorage &operator=(const SparseDualStorage &rhs) {
        if (this == &rhs) {
            return *this;
        }
        Allocator::operator=(static_cast<Allocator &>(rhs));

        Clear();
        this->deallocate(ctrl_, mem_size(capacity_));

        ctrl_ = nullptr;
        generation_ = nullptr;
        data_main_ = nullptr;
        data_cold_ = nullptr;
        capacity_ = 0;

        Reserve(rhs.capacity_);

        if (rhs.ctrl_) {
            memcpy(ctrl_, rhs.ctrl_, ctrl_size(capacity_));
            memcpy(generation_, rhs.generation_, generation_size(capacity_));
        }

        for (uint32_t i = 0; i < rhs.capacity_; ++i) {
            if (ctrl_[i / 64] & (1ull << (i % 64))) {
                new (&data_main_[i]) T(rhs.data_main_[i]);
                new (&data_cold_[i]) U(rhs.data_cold_[i]);
            } else {
                // copy next free index
                memcpy(data_main_ + i, rhs.data_main_ + i, sizeof(uint32_t));
            }
        }

        first_free_ = rhs.first_free_;
        size_ = rhs.size_;

        return (*this);
    }

    SparseDualStorage &operator=(SparseDualStorage &&rhs) noexcept {
        if (this == &rhs) {
            return *this;
        }

        Clear();
        this->deallocate(ctrl_, mem_size(capacity_));

        ctrl_ = std::exchange(rhs.ctrl_, nullptr);
        generation_ = std::exchange(rhs.generation_, nullptr);
        data_main_ = std::exchange(rhs.data_main_, nullptr);
        data_cold_ = std::exchange(rhs.data_cold_, nullptr);

        capacity_ = std::exchange(rhs.capacity_, 0);
        size_ = std::exchange(rhs.size_, 0);
        first_free_ = std::exchange(rhs.first_free_, 0);

        return (*this);
    }

    uint32_t size() const { return size_; }
    uint32_t capacity() const { return capacity_; }

    bool empty() const { return size_ == 0; }

    uint8_t *generation() { return generation_; }
    const uint8_t *generation() const { return generation_; }

    T *data_main() { return data_main_; }
    const T *data_main() const { return data_main_; }

    T *data_cold() { return data_cold_; }
    const T *data_cold() const { return data_cold_; }

    void Clear() {
        const uint32_t word_count = (capacity_ + 63) / 64;
        for (uint32_t i = 0; i < word_count && size_; ++i) {
            uint64_t mask = ctrl_[i];
            while (mask) {
                EraseUnsafe(64 * i + CountTrailingZeroes(mask));
                mask &= mask - 1;
            }
        }
    }

    void Reserve(const uint32_t new_capacity) {
        if (new_capacity <= capacity_) {
            return;
        }

        uint64_t *old_ctrl = ctrl_;
        uint8_t *old_generation = generation_;
        T *old_data_main = data_main_;
        U *old_data_cold = data_cold_;
        const uint32_t old_word_count = (capacity_ + 63) / 64;
        const uint32_t new_word_count = (new_capacity + 63) / 64;

        size_t total_mem = ctrl_size(new_capacity);
        const size_t generation_start = total_mem;
        total_mem += generation_size(new_capacity);
        if (total_mem % alignof(T)) {
            total_mem += alignof(T) - (total_mem % alignof(T));
        }
        const size_t data_main_start = total_mem;
        total_mem += sizeof(T) * new_capacity;
        if (total_mem % alignof(U)) {
            total_mem += alignof(U) - (total_mem % alignof(U));
        }
        const size_t data_cold_start = total_mem;
        total_mem += sizeof(U) * new_capacity;
        assert(total_mem == mem_size(new_capacity));

        ctrl_ = this->allocate(total_mem);
        generation_ = reinterpret_cast<uint8_t *>(uintptr_t(ctrl_) + generation_start);
        data_main_ = reinterpret_cast<T *>(uintptr_t(ctrl_) + data_main_start);
        data_cold_ = reinterpret_cast<U *>(uintptr_t(ctrl_) + data_cold_start);
        assert(uintptr_t(data_main_) % alignof(T) == 0);
        assert(uintptr_t(data_cold_) % alignof(U) == 0);

        // copy old control and generation
        if (old_ctrl) {
            memcpy(ctrl_, old_ctrl, old_word_count * sizeof(uint64_t));
            memcpy(generation_, old_generation, capacity_ * sizeof(uint8_t));
        }
        // fill rest with zeroes
        memset(ctrl_ + old_word_count, 0, (new_word_count - old_word_count) * sizeof(uint64_t));
        memset(generation_ + capacity_, 0, (new_capacity - capacity_) * sizeof(uint8_t));

        // move old data
        for (uint32_t i = 0; i < capacity_; ++i) {
            if (ctrl_[i / 64] & (1ull << (i % 64))) {
                new (&data_main_[i]) T(std::move(old_data_main[i]));
                old_data_main[i].~T();
                new (&data_cold_[i]) U(std::move(old_data_cold[i]));
                old_data_cold[i].~U();
            } else {
                // copy next free index
                memcpy(data_main_ + i, old_data_main + i, sizeof(uint32_t));
            }
        }

        this->deallocate(old_ctrl, mem_size(capacity_));

        for (uint32_t i = capacity_; i < new_capacity - 1; i++) {
            const uint32_t next_free = i + 1;
            assert(next_free < 0xffffffu);
            memcpy(data_main_ + i, &next_free, sizeof(uint32_t));
        }

        memcpy(data_main_ + new_capacity - 1, &first_free_, sizeof(uint32_t));
        first_free_ = capacity_;

        capacity_ = new_capacity;
    }

    Handle<T, RWTag> Emplace() {
        if (size_ + 1 > capacity_) {
            Reserve(capacity_ ? (capacity_ * 2) : 64);
        }
        const uint32_t index = first_free_;
        assert((ctrl_[index / 64] & (1ull << (index % 64))) == 0);
        memcpy(&first_free_, data_main_ + index, sizeof(uint32_t));

        new (data_main_ + index) T();
        new (data_cold_ + index) U();

        ctrl_[index / 64] |= (1ull << (index % 64));

        ++size_;
        return Handle<T, RWTag>{index, generation_[index]};
    }

    Handle<T, RWTag> Push(const T &main, const U &cold) {
        if (size_ + 1 > capacity_) {
            Reserve(capacity_ ? (capacity_ * 2) : 64);
        }
        const uint32_t index = first_free_;
        assert((ctrl_[index / 64] & (1ull << (index % 64))) == 0);
        memcpy(&first_free_, data_main_ + index, sizeof(uint32_t));

        new (&data_main_[index]) T(main);
        new (&data_cold_[index]) U(cold);
        ctrl_[index / 64] |= (1ull << (index % 64));

        ++size_;
        return Handle<T, RWTag>{index, generation_[index]};
    }

    void Erase(const Handle<T, RWTag> handle) {
        assert((ctrl_[handle.index / 64] & (1ull << (handle.index % 64))) && "Invalid index!");

        data_main_[handle.index].~T();
        data_cold_[handle.index].~U();
        ctrl_[handle.index / 64] &= ~(1ull << (handle.index % 64));
        ++generation_[handle.index];

        memcpy(data_main_ + handle.index, &first_free_, sizeof(uint32_t));
        first_free_ = handle.index;
        --size_;
    }

    void EraseUnsafe(const uint32_t index) {
        assert((ctrl_[index / 64] & (1ull << (index % 64))) && "Invalid index!");

        data_main_[index].~T();
        data_cold_[index].~U();
        ctrl_[index / 64] &= ~(1ull << (index % 64));
        ++generation_[index];

        memcpy(data_main_ + index, &first_free_, sizeof(uint32_t));
        first_free_ = index;
        --size_;
    }

    std::pair<T &, U &> operator[](const Handle<T, RWTag> handle) {
        assert((ctrl_[handle.index / 64] & (1ull << (handle.index % 64))) && "Invalid handle!");
        assert((generation_[handle.index] == handle.generation) && "Invalid handle!");
        return {data_main_[handle.index], data_cold_[handle.index]};
    }

    std::pair<const T &, const U &> operator[](const Handle<T, ROTag> handle) const {
        assert((ctrl_[handle.index / 64] & (1ull << (handle.index % 64))) && "Invalid handle!");
        assert((generation_[handle.index] == handle.generation) && "Invalid handle!");
        return {data_main_[handle.index], data_cold_[handle.index]};
    }

    std::pair<T *, U *> TryGet(const Handle<T, RWTag> handle) {
        assert((ctrl_[handle.index / 64] & (1ull << (handle.index % 64))) && "Invalid handle!");
        if (generation_[handle.index] == handle.generation) {
            return {&data_main_[handle.index], &data_cold_[handle.index]};
        }
        return {nullptr, nullptr};
    }

    std::pair<const T *, const U *> TryGet(const Handle<T, ROTag> handle) const {
        assert((ctrl_[handle.index / 64] & (1ull << (handle.index % 64))) && "Invalid handle!");
        if (generation_[handle.index] == handle.generation) {
            return {&data_main_[handle.index], &data_cold_[handle.index]};
        }
        return {nullptr, nullptr};
    }

    std::pair<T &, U &> GetUnsafe(const uint32_t index) {
        assert((ctrl_[index / 64] & (1ull << (index % 64))) && "Invalid index!");
        return {data_main_[index], data_cold_[index]};
    }

    std::pair<const T &, const U &> GetUnsafe(const uint32_t index) const {
        assert((ctrl_[index / 64] & (1ull << (index % 64))) && "Invalid index!");
        return {data_main_[index], data_cold_[index]};
    }

    bool IsOccupied(const uint32_t index) const { return (ctrl_[index / 64] & (1ull << (index % 64))) != 0; }

    uint8_t GetGeneration(const uint32_t index) const {
        assert((ctrl_[index / 64] & (1ull << (index % 64))) && "Invalid index!");
        return generation_[index];
    }

    void SetGeneration(const uint32_t index, const uint8_t generation) const {
        assert((ctrl_[index / 64] & (1ull << (index % 64))) && "Invalid index!");
        generation_[index] = generation;
    }

    struct PairProxy {
        std::pair<T &, U &> value;
        const std::pair<T &, U &> *operator->() const { return &value; }
    };

    struct PairConstProxy {
        std::pair<const T &, const U &> value;
        const std::pair<const T &, const U &> *operator->() const { return &value; }
    };

    class SparseDualStorageIterator {
        friend class SparseDualStorage<T, U, Alignment>;

        SparseDualStorage<T, U, Alignment> *container_;
        uint32_t index_;

        SparseDualStorageIterator(SparseDualStorage<T, U, Alignment> *container, const uint32_t index)
            : container_(container), index_(index) {}

      public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = std::pair<T, U>;
        using difference_type = std::ptrdiff_t;
        using pointer = PairProxy;
        using reference = T &;

        std::pair<T &, U &> operator*() { return container_->GetUnsafe(index_); }
        PairProxy operator->() { return PairProxy{container_->GetUnsafe(index_)}; }
        SparseDualStorageIterator &operator++() {
            index_ = container_->NextOccupied(index_);
            return *this;
        }
        SparseDualStorageIterator operator++(int) {
            SparseDualStorageIterator tmp(*this);
            ++(*this);
            return tmp;
        }

        Handle<T, RWTag> handle() const { return Handle<T, RWTag>{index_, container_->GetGeneration(index_)}; }

        bool operator<(const SparseDualStorageIterator &rhs) const { return index_ < rhs.index_; }
        bool operator<=(const SparseDualStorageIterator &rhs) const { return index_ <= rhs.index_; }
        bool operator>(const SparseDualStorageIterator &rhs) const { return index_ > rhs.index_; }
        bool operator>=(const SparseDualStorageIterator &rhs) const { return index_ >= rhs.index_; }
        bool operator==(const SparseDualStorageIterator &rhs) const { return index_ == rhs.index_; }
        bool operator!=(const SparseDualStorageIterator &rhs) const { return index_ != rhs.index_; }
    };

    class SparseDualStorageConstIterator {
        friend class SparseDualStorage<T, U, Alignment>;

        const SparseDualStorage<T, U, Alignment> *container_;
        uint32_t index_;

        SparseDualStorageConstIterator(const SparseDualStorage<T, U, Alignment> *container, const uint32_t index)
            : container_(container), index_(index) {}

      public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = std::pair<T, U>;
        using difference_type = std::ptrdiff_t;
        using pointer = PairConstProxy;
        using reference = T &;

        std::pair<const T &, const U &> operator*() const { return container_->GetUnsafe(index_); }
        PairConstProxy operator->() const { return PairConstProxy{container_->GetUnsafe(index_)}; }
        SparseDualStorageConstIterator &operator++() {
            index_ = container_->NextOccupied(index_);
            return *this;
        }

        SparseDualStorageConstIterator operator++(int) {
            SparseDualStorageConstIterator tmp(*this);
            ++(*this);
            return tmp;
        }

        Handle<T, ROTag> handle() const { return Handle<T, ROTag>{index_, container_->GetGeneration(index_)}; }

        bool operator<(const SparseDualStorageConstIterator &rhs) const { return index_ < rhs.index_; }
        bool operator<=(const SparseDualStorageConstIterator &rhs) const { return index_ <= rhs.index_; }
        bool operator>(const SparseDualStorageConstIterator &rhs) const { return index_ > rhs.index_; }
        bool operator>=(const SparseDualStorageConstIterator &rhs) const { return index_ >= rhs.index_; }
        bool operator==(const SparseDualStorageConstIterator &rhs) const { return index_ == rhs.index_; }
        bool operator!=(const SparseDualStorageConstIterator &rhs) const { return index_ != rhs.index_; }
    };

    using iterator = SparseDualStorageIterator;
    using const_iterator = SparseDualStorageConstIterator;

    iterator begin() {
        if (!empty()) {
            const uint32_t word_count = (capacity_ + 63) / 64;
            for (uint32_t i = 0; i < word_count; ++i) {
                const uint64_t mask = ctrl_[i];
                if (mask) {
                    return iterator(this, 64 * i + CountTrailingZeroes(mask));
                }
            }
        }
        return end();
    }
    const_iterator begin() const { return cbegin(); }

    const_iterator cbegin() const {
        if (!empty()) {
            const uint32_t word_count = (capacity_ + 63) / 64;
            for (uint32_t i = 0; i < word_count; ++i) {
                const uint64_t mask = ctrl_[i];
                if (mask) {
                    return const_iterator(this, 64 * i + CountTrailingZeroes(mask));
                }
            }
        }
        return cend();
    }

    iterator end() { return iterator(this, capacity_); }
    const_iterator end() const { return cend(); }
    const_iterator cend() const { return const_iterator(this, capacity_); }

    iterator iter_at(uint32_t i) { return iterator(this, i); }
    const_iterator citer_at(uint32_t i) const { return const_iterator(this, i); }

    iterator Erase(iterator it) {
        const uint32_t next_index = NextOccupied(it.index_);
        EraseUnsafe(it.index_);
        return iterator(this, next_index);
    }

  private:
    uint32_t NextOccupied(uint32_t index) const {
        assert((ctrl_[index / 64] & (1ull << (index % 64))) && "Invalid index!");

        if (++index >= capacity_) {
            return capacity_;
        }

        const uint32_t start_word = index / 64;
        const uint32_t word_count = (capacity_ + 63) / 64;

        { // Check first word
            uint64_t mask = ctrl_[start_word] & (~0ull << (index % 64));
            if (mask) {
                return 64 * start_word + CountTrailingZeroes(mask);
            }
        }

        for (uint32_t i = start_word + 1; i < word_count; ++i) {
            const uint64_t mask = ctrl_[i];
            if (mask) {
                return 64 * i + CountTrailingZeroes(mask);
            }
        }

        return capacity_;
    }
};
} // namespace Ren
