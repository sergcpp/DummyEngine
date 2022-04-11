#pragma once

#include <new>
#include <type_traits>
#include <utility>

//
// Mostly based on https://github.com/WG21-SG14/SG14/blob/master/SG14/inplace_function.h
//

namespace Sys {
#ifndef SYS_EXCHANGE_DEFINED
template <class T, class U = T> T exchange(T &obj, U &&new_value) {
    T old_value = std::move(obj);
    obj = std::forward<U>(new_value);
    return old_value;
}
#define SYS_EXCHANGE_DEFINED
#endif

const size_t InplaceFunctionDefaultCapacity = 16;

//
// Union that helps to determine required alignment based on storage size
//
template <size_t Capacity> union AlignmentHelper {
    struct float4 {
        alignas(16) float a[4];
    };
    struct double4 {
        alignas(32) double a[4];
    };
    template <class T> using maybe = typename std::conditional<(Capacity >= sizeof(T)), T, char>::type;
    char data[Capacity];
    maybe<int> a;
    maybe<long> b;
    maybe<long long> c;
    maybe<void *> d;
    maybe<void (*)()> e;
    maybe<float> f;
    maybe<float4> f4;
    maybe<double> dd;
    maybe<double4> d4;
    maybe<long double> h;
};

template <class Signature, size_t Capacity = InplaceFunctionDefaultCapacity,
          size_t Alignment = alignof(AlignmentHelper<Capacity>)>
class InplaceFunction;

template <class> struct IsInplaceFunction : std::false_type {};
template <class Sig, size_t Cap, size_t Align>
struct IsInplaceFunction<InplaceFunction<Sig, Cap, Align>> : std::true_type {};

template <class T> struct type_wrapper { using type = T; };

template <class R, class... Args> struct func_table_t {
    using invoke_ptr_t = R (*)(void *, Args &&...);
    using process_ptr_t = void (*)(void *, void *);
    using destroy_ptr_t = void (*)(void *);

    invoke_ptr_t invoke_ptr;
    process_ptr_t copy_ptr, move_ptr;
    destroy_ptr_t destroy_ptr;

    static void default_copy_move_func(void *, void *) {}
    static void default_destroy_func(void *) {}

    static func_table_t<R, Args...> *empty_func_table() {
        static func_table_t<R, Args...> s_empty_func_table{};
        return &s_empty_func_table;
    }

    explicit func_table_t()
        : invoke_ptr(nullptr), copy_ptr(default_copy_move_func), move_ptr(default_copy_move_func),
          destroy_ptr(default_destroy_func) {}

    template <class C>
    explicit func_table_t(type_wrapper<C>)
        : invoke_ptr([](void *storage_ptr, Args &&...args) -> R {
              return (*reinterpret_cast<C *>(storage_ptr))(static_cast<Args &&>(args)...);
          }),
          copy_ptr(
              [](void *src_ptr, void *dst_ptr) -> void { ::new (dst_ptr) C{(*reinterpret_cast<const C *>(src_ptr))}; }),
          move_ptr([](void *src_ptr, void *dst_ptr) -> void {
              ::new (dst_ptr) C{std::move(*reinterpret_cast<C *>(src_ptr))};
              reinterpret_cast<C *>(src_ptr)->~C();
          }),
          destroy_ptr([](void *src_ptr) -> void { reinterpret_cast<C *>(src_ptr)->~C(); }) {}

    func_table_t(const func_table_t &) = delete;
    func_table_t(func_table_t &&) = delete;

    func_table_t &operator=(const func_table_t &) = delete;
    func_table_t &operator=(func_table_t &&) = delete;

    ~func_table_t() = default;
};

template <class R, class... Args, size_t Capacity, size_t Alignment>
class InplaceFunction<R(Args...), Capacity, Alignment> {
    alignas(Alignment) mutable char storage_[Capacity];
    const func_table_t<R, Args...> *func_table_;

    InplaceFunction(const func_table_t<R, Args...> *func_table,
                    typename func_table_t<R, Args...>::process_ptr_t copy_or_move_ptr, char *storage)
        : func_table_{func_table} {
        copy_or_move_ptr(storage, storage_);
    }

  public:
    InplaceFunction() : func_table_(func_table_t<R, Args...>::empty_func_table()) {}

    template <class T, class C = typename std::decay<T>::type,
              class = typename std::enable_if<!IsInplaceFunction<C>::value>::type>
    InplaceFunction(T &&func) {
        static_assert(std::is_copy_constructible<C>::value);
        static_assert(sizeof(C) <= Capacity);
        static_assert(Alignment % alignof(C) == 0);

        static const func_table_t<R, Args...> func_table(type_wrapper<C>{});
        func_table_ = &func_table;

        ::new (storage_) C{std::forward<T>(func)};
    }

    InplaceFunction(std::nullptr_t) : func_table_(func_table_t<R, Args...>::empty_func_table()) {}

    ~InplaceFunction() { func_table_->destroy_ptr(storage_); }

    R operator()(Args... args) const { return func_table_->invoke_ptr(storage_, std::forward<Args>(args)...); }

    bool operator==(std::nullptr_t) const noexcept { return !operator bool(); }
    bool operator!=(std::nullptr_t) const noexcept { return operator bool(); }
    explicit constexpr operator bool() const noexcept {
        return func_table_ != func_table_t<R, Args...>::empty_func_table();
    }

    template <size_t Cap, size_t Align>
    InplaceFunction(const InplaceFunction<R(Args...), Cap, Align> &rhs)
        : InplaceFunction(rhs.func_table_, rhs.func_table_->copy_ptr, rhs.storage_) {
        static_assert(Capacity >= Cap);
        static_assert((Alignment % Align) == 0);
    }

    template <size_t Cap, size_t Align>
    InplaceFunction(InplaceFunction<R(Args...), Cap, Align> &&rhs)
        : InplaceFunction(rhs.func_table_, rhs.func_table_->move_ptr, rhs.storage_) {
        static_assert(Capacity >= Cap);
        static_assert((Alignment % Align) == 0);
        rhs.func_table_ = func_table_t<R, Args...>::empty_func_table();
    }

    InplaceFunction(const InplaceFunction &rhs) : func_table_{rhs.func_table_} {
        func_table_->copy_ptr(rhs.storage_, storage_);
    }

    InplaceFunction(InplaceFunction &&rhs)
        : func_table_{exchange(rhs.func_table_, func_table_t<R, Args...>::empty_func_table())} {
        func_table_->move_ptr(rhs.storage_, storage_);
    }

    InplaceFunction &operator=(std::nullptr_t) noexcept {
        func_table_->destroy_ptr(storage_);
        func_table_ = func_table_t<R, Args...>::empty_func_table();
        return *this;
    }

    InplaceFunction &operator=(const InplaceFunction &rhs) noexcept {
        func_table_->destroy_ptr(storage_);
        func_table_ = rhs.func_table_;
        func_table_->copy_ptr(rhs.storage_, storage_);
        return *this;
    }

    InplaceFunction &operator=(InplaceFunction &&rhs) noexcept {
        func_table_->destroy_ptr(storage_);
        func_table_ = exchange(rhs.func_table_, func_table_t<R, Args...>::empty_func_table());
        func_table_->move_ptr(rhs.storage_, storage_);
        return *this;
    }
};
} // namespace Sys