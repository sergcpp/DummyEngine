#pragma once

#include "HashMap32.h"
#include "SparseArray.h"

namespace Ren {
template <typename T, typename StorageType> class StrongRef;

template <typename T> class Storage : public SparseArray<T> {
    HashMap32<String, uint32_t> items_by_name_;

  public:
    Storage() = default;

    Storage(const Storage &rhs) = delete;

    template <class... Args> StrongRef<T, Storage> Add(Args &&... args) {
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

    StrongRef<T, Storage> FindByName(const char *name) {
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
    unsigned ref_count() const { return ctrl_->strong_refs; }

  protected:
    template <typename T, typename StorageType> friend class StrongRef;
    template <typename T, typename StorageType> friend class WeakRef;

    RefCounter() {
        ctrl_ = new CtrlBlock;
        ctrl_->strong_refs = ctrl_->weak_refs = 0;
    }
    RefCounter(const RefCounter &) {
        ctrl_ = new CtrlBlock;
        ctrl_->strong_refs = ctrl_->weak_refs = 0;
    }
    RefCounter &operator=(const RefCounter &) { return *this; }
    RefCounter(RefCounter &&rhs) noexcept {
        ctrl_ = std::exchange(rhs.ctrl_, nullptr);
    }
    RefCounter &operator=(RefCounter &&rhs) noexcept {
        if (ctrl_) {
            assert(ctrl_->strong_refs == 0);
            if (ctrl_->weak_refs == 0) {
                delete ctrl_;
            }
        }

        ctrl_ = std::exchange(rhs.ctrl_, nullptr);
        return (*this);
    }
    ~RefCounter() {
        if (ctrl_) {
            assert(ctrl_->strong_refs == 0);
            if (ctrl_->weak_refs == 0) {
                delete ctrl_;
            }
        }
    }

  private:
    struct CtrlBlock {
        uint32_t strong_refs;
        uint32_t weak_refs;
    };

    mutable CtrlBlock *ctrl_;
};

template <typename T, typename StorageType> class WeakRef;

template <typename T, typename StorageType = Storage<T>> class StrongRef {
    StorageType *storage_;
    uint32_t index_;

    friend class WeakRef<T, StorageType>;

  public:
    StrongRef() : storage_(nullptr), index_(0) {}
    StrongRef(StorageType *storage, uint32_t index) : storage_(storage), index_(index) {
        if (storage_) {
            const T &p = storage_->at(index_);
            ++p.ctrl_->strong_refs;
        }
    }
    ~StrongRef() { Release(); }

    StrongRef(const StrongRef &rhs) {
        storage_ = rhs.storage_;
        index_ = rhs.index_;

        if (storage_) {
            const T &p = storage_->at(index_);
            ++p.ctrl_->strong_refs;
        }
    }

    StrongRef(const WeakRef<T, StorageType> &rhs) {
        storage_ = rhs.storage_;
        index_ = rhs.index_;

        assert(storage_);
        const T &p = storage_->at(index_);
        ++p.ctrl_->strong_refs;
    }

    StrongRef(StrongRef &&rhs) noexcept {
        storage_ = std::exchange(rhs.storage_, nullptr);
        index_ = std::exchange(rhs.index_, 0);
    }

    StrongRef &operator=(const StrongRef &rhs) {
        Release();

        storage_ = rhs.storage_;
        index_ = rhs.index_;

        if (storage_) {
            const T &p = storage_->at(index_);
            ++p.ctrl_->strong_refs;
        }

        return (*this);
    }

    StrongRef &operator=(StrongRef &&rhs) noexcept {
        if (&rhs == this) {
            return (*this);
        }

        Release();

        storage_ = std::exchange(rhs.storage_, nullptr);
        index_ = std::exchange(rhs.index_, 0);

        return (*this);
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

    bool operator==(const StrongRef &rhs) const { return storage_ == rhs.storage_ && index_ == rhs.index_; }

    void Release() {
        if (storage_) {
            const T &p = storage_->at(index_);
            if (--p.ctrl_->strong_refs == 0) {
                storage_->erase(index_);
            }
            storage_ = nullptr;
            index_ = 0;
        }
    }
};

template <typename T, typename StorageType = Storage<T>> class WeakRef {
    StorageType *storage_;
    RefCounter::CtrlBlock *ctrl_;
    uint32_t index_;

    friend class StrongRef<T, StorageType>;

  public:
    WeakRef() : storage_(nullptr), ctrl_(nullptr), index_(0) {}
    WeakRef(StorageType *storage, uint32_t index) : storage_(storage), ctrl_(nullptr), index_(index) {
        assert(storage);
        const T &p = storage_->at(index_);
        ctrl_ = p.ctrl_;
        ++ctrl_->weak_refs;
    }
    ~WeakRef() { Release(); }

    WeakRef(const WeakRef &rhs) {
        storage_ = rhs.storage_;
        ctrl_ = rhs.ctrl_;
        index_ = rhs.index_;

        if (ctrl_) {
            ++ctrl_->weak_refs;
        }
    }

    WeakRef(const StrongRef<T> &rhs) {
        storage_ = rhs.storage_;
        ctrl_ = nullptr;
        index_ = rhs.index_;

        if (storage_) {
            const T &p = storage_->at(index_);
            ctrl_ = p.ctrl_;
            ++ctrl_->weak_refs;
        }
    }

    WeakRef(WeakRef &&rhs) noexcept {
        storage_ = std::exchange(rhs.storage_, nullptr);
        ctrl_ = std::exchange(rhs.ctrl_, nullptr);
        index_ = std::exchange(rhs.index_, 0);
    }

    WeakRef &operator=(const WeakRef &rhs) {
        Release();

        storage_ = rhs.storage_;
        ctrl_ = rhs.ctrl_;
        index_ = rhs.index_;

        if (ctrl_) {
            ++ctrl_->weak_refs;
        }

        return *this;
    }

    WeakRef &operator=(const StrongRef<T> &rhs) {
        Release();

        storage_ = rhs.storage_;
        ctrl_ = nullptr;
        index_ = rhs.index_;

        if (storage_) {
            const T &p = storage_->at(index_);
            ctrl_ = p.ctrl_;
            ++ctrl_->weak_refs;
        }

        return *this;
    }

    WeakRef &operator=(WeakRef &&rhs) noexcept {
        if (&rhs == this) {
            return (*this);
        }

        Release();

        storage_ = std::exchange(rhs.storage_, nullptr);
        ctrl_ = std::exchange(rhs.ctrl_, nullptr);
        index_ = std::exchange(rhs.index_, 0);

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

    explicit operator bool() const { return ctrl_ && ctrl_->strong_refs != 0; }

    uint32_t index() const { return index_; }

    bool operator==(const WeakRef &rhs) const { return ctrl_ == rhs.ctrl_ && index_ == rhs.index_; }
    bool operator!=(const WeakRef &rhs) const { return !operator==(rhs); }
    bool operator==(const StrongRef<T> &rhs) const { return storage_ == rhs.storage_ && index_ == rhs.index_; }
    bool operator!=(const StrongRef<T> &rhs) const { return !operator==(rhs); }

    void Release() {
        if (ctrl_) {
            if (--ctrl_->weak_refs == 0 && ctrl_->strong_refs == 0) {
                delete ctrl_;
            }
            storage_ = nullptr;
            ctrl_ = nullptr;
            index_ = 0;
        }
    }
};

} // namespace Ren
