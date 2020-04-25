#pragma once

#include <cstddef>

#include <vector>

#include "Delegate.h"

namespace Sys {
template<typename TSignature>
class Signal;

template<typename R, typename ...Args>
class Signal<R(Args... args)> {
    std::vector<Delegate<R(Args... args)>> delegates_;
public:
    size_t size() const {
        return delegates_.size();
    }

    void clear() {
        delegates_.clear();
    }

    R FireOne(size_t i, Args... args) {
        return delegates_[i](args...);
    }

    void FireN(Args... args) {
        for (auto &d : delegates_) {
            d(args...);
        }
    }

    R FireL(Args... args) {
        if (delegates_.empty()) return R();
        for (size_t i = 0; i < delegates_.size() - 1; i++) {
            delegates_[i](args...);
        }
        return delegates_.back()(args...);
    }

    std::vector<R> FireV(Args... args) {
        std::vector<R> results;
        for (auto &d : delegates_) {
            results.push_back(d(args...));
        }
        return results;
    }

    template<R(*TFunc)(Args... args)>
    void Connect() {
        delegates_.emplace_back();
        delegates_.back().template Bind<TFunc>();
    }

    template<class T, R(T::*TMethod)(Args... args)>
    void Connect(T *p_object) {
        delegates_.emplace_back();
        delegates_.back().template Bind<T, TMethod>(p_object);
    }

    template<class T, R(T::*TMethod)(Args... args) const>
    void Connect(const T *p_object) {
        delegates_.emplace_back();
        delegates_.back().template Bind<T, TMethod>(p_object);
    }
};

template<typename TSignature, int N = 8>
class SignalN;

template<typename R, typename ...Args, int N>
class SignalN<R(Args... args), N> {
    Delegate<R(Args... args)> delegates_[N];
    int count_ = 0;
  public:
    int size() const {
        return count_;
    }

    void clear() {
        std::fill(std::begin(delegates_), std::begin(delegates_) + count_, {});
        count_ = 0;
    }

    R FireOne(size_t i, Args... args) {
        return delegates_[i](args...);
    }

    void FireN(Args... args) {
        for (int i = 0; i < count_; i++) {
            delegates_[i](args...);
        }
    }

    R FireL(Args... args) {
        if (!count_) return R();
        for (int i = 0; i < count_ - 1; i++) {
            delegates_[i](args...);
        }
        return delegates_[count_ - 1](args...);
    }

    void FireV(Args... args, R *results) {
        for (int i = 0; i < count_; i++) {
            results[i] = delegates_[i](args...);
        }
    }

    template<R(*TFunc)(Args... args)>
    void Connect() {
        delegates_[count_++].template Bind<TFunc>();
    }

    template<class T, R(T::*TMethod)(Args... args)>
    void Connect(T *p_object) {
        delegates_[count_++].template Bind<T, TMethod>(p_object);
    }

    template<class T, R(T::*TMethod)(Args... args) const>
    void Connect(const T *p_object) {
        delegates_[count_++].template Bind<T, TMethod>(p_object);
    }
};

}
