#pragma once

#include "HashMap32.h"
#include "SparseArray.h"

namespace Ren {
template <typename T> class StrongRef;

template <typename T> class Storage : public SparseArray<T> {
    HashMap32<String, uint32_t> items_by_name_;

  public:
    Storage() {}

    Storage(const Storage &rhs) = delete;

    template <class... Args> StrongRef<T> Add(Args &&... args) {
        const uint32_t index = SparseArray<T>::emplace(args...);

        bool res = items_by_name_.Insert(SparseArray<T>::at(index).name(), index);
        assert(res);

        return {this, index};
    }

    void erase(const uint32_t i) {
        const String &name = SparseArray<T>::at(i).name();

        const bool res = items_by_name_.Erase(name);
        assert(res);

        SparseArray<T>::erase(i);
    }

    StrongRef<T> FindByName(const char *name) {
        uint32_t *p_index = items_by_name_.Find(name);
        if (p_index) {
            return {this, *p_index};
        } else {
            return {nullptr, 0};
        }
    }
};

class RefCounter {
  public:
    unsigned ref_count() const { return counter_; }

  protected:
    template <class T> friend class StrongRef;

    void add_ref() { ++counter_; }
    bool release() { return --counter_ == 0; }

    RefCounter() : counter_(0) {}
    RefCounter(const RefCounter &) : counter_(0) {}
    RefCounter &operator=(const RefCounter &) { return *this; }
    RefCounter(RefCounter &&rhs) noexcept : counter_(rhs.counter_) { rhs.counter_ = 0; }
    RefCounter &operator=(RefCounter &&rhs) noexcept {
        counter_ = rhs.counter_;
        rhs.counter_ = 0;
        return *this;
    }

  private:
    mutable unsigned counter_;
};

template <class T> class WeakRef;

template <class T> class StrongRef {
    Storage<T> *storage_;
    uint32_t index_;

    friend class WeakRef<T>;

  public:
    StrongRef() : storage_(nullptr), index_(0) {}
    StrongRef(Storage<T> *storage, uint32_t index) : storage_(storage), index_(index) {
        if (storage_) {
            T &p = storage_->at(index_);
            p.add_ref();
        }
    }
    ~StrongRef() { Release(); }

    StrongRef(const StrongRef &rhs) {
        storage_ = rhs.storage_;
        index_ = rhs.index_;

        if (storage_) {
            T &p = storage_->at(index_);
            p.add_ref();
        }
    }

    StrongRef(StrongRef &&rhs) noexcept {
        storage_ = rhs.storage_;
        rhs.storage_ = nullptr;
        index_ = rhs.index_;
        rhs.index_ = 0;
    }

    StrongRef &operator=(const StrongRef &rhs) {
        if (this == &rhs) {
            return *this;
        }

        Release();

        storage_ = rhs.storage_;
        index_ = rhs.index_;

        if (storage_) {
            T &p = storage_->at(index_);
            p.add_ref();
        }

        return *this;
    }

    StrongRef &operator=(StrongRef &&rhs) noexcept {
        Release();

        storage_ = rhs.storage_;
        rhs.storage_ = nullptr;
        index_ = rhs.index_;
        rhs.index_ = 0;

        return *this;
    }

    T *operator->() {
        assert(storage_);
        return &storage_->at(index_);
    }

    const T *operator->() const {
        assert(storage_);
        return &storage_->at(index_);
    }

    T &operator*() {
        assert(storage_);
        return storage_->at(index_);
    }

    const T &operator*() const {
        assert(storage_);
        return storage_->at(index_);
    }

    T *get() {
        assert(storage_);
        return &storage_->at(index_);
    }

    const T *get() const {
        assert(storage_);
        return &storage_->at(index_);
    }

    explicit operator bool() const { return storage_ != nullptr; }

    uint32_t index() const { return index_; }

    bool operator==(const StrongRef &rhs) {
        return storage_ == rhs.storage_ && index_ == rhs.index_;
    }

    void Release() {
        if (storage_) {
            T *p = &storage_->at(index_);
            if (p->release()) {
                storage_->erase(index_);
            }
            storage_ = nullptr;
            index_ = 0;
        }
    }
};

template <class T> class WeakRef {
    Storage<T> *storage_;
    uint32_t index_;

    friend class StrongRef<T>;

  public:
    WeakRef() : storage_(nullptr), index_(0) {}
    WeakRef(Storage<T> *storage, uint32_t index) : storage_(storage), index_(index) {}

    WeakRef(const WeakRef &rhs) = default;
    WeakRef(const StrongRef<T> &rhs) {
        storage_ = rhs.storage_;
        index_ = rhs.index_;
    }

    WeakRef(WeakRef &&rhs) noexcept = default;

    WeakRef &operator=(const WeakRef &rhs) = default;
    WeakRef &operator=(const StrongRef<T> &rhs) {
        storage_ = rhs.storage_;
        index_ = rhs.index_;

        return (*this);
    }

    WeakRef &operator=(WeakRef &&rhs) noexcept = default;

    T *operator->() {
        assert(storage_);
        return &storage_->at(index_);
    }

    const T *operator->() const {
        assert(storage_);
        return &storage_->at(index_);
    }

    T &operator*() {
        assert(storage_);
        return storage_->at(index_);
    }

    const T &operator*() const {
        assert(storage_);
        return storage_->at(index_);
    }

    T *get() {
        assert(storage_);
        return &storage_->at(index_);
    }

    const T *get() const {
        assert(storage_);
        return &storage_->at(index_);
    }

    explicit operator bool() const { return storage_ != nullptr; }

    uint32_t index() const { return index_; }

    bool operator==(const WeakRef &rhs) {
        return storage_ == rhs.storage_ && index_ == rhs.index_;
    }
    bool operator==(const StrongRef<T> &rhs) {
        return storage_ == rhs.storage_ && index_ == rhs.index_;
    }
};
} // namespace Ren
