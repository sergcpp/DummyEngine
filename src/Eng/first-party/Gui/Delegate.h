#pragma once

namespace Gui {
template <typename TSignature> class Delegate;

template <typename R, typename... Args> class Delegate<R(Args... args)> {
    typedef R (*stub_type)(void *p_object, Args... args);

    void *p_object_;
    stub_type p_stub_;

    template <class T, R (T::*TMethod)(Args... args)> static R method_stub(void *p_object, Args... args) {
        T *p = static_cast<T *>(p_object);
        return (p->*TMethod)(args...);
    }

    template <class T, R (T::*TMethod)(Args... args) const> static R const_method_stub(void *p_object, Args... args) {
        const T *p = static_cast<T *>(p_object);
        return (p->*TMethod)(args...);
    }

    template <R (*TMethod)(Args... args)> static R function_stub(void *p_object, Args... args) {
        return (*TMethod)(args...);
    }

    Delegate(void *p_object, stub_type p_stub) : p_object_(p_object), p_stub_(p_stub) {}

  public:
    Delegate() : p_object_(nullptr), p_stub_(nullptr) {}

    R operator()(Args... args) { return (*p_stub_)(p_object_, args...); }

    template <R (*TFunc)(Args... args)> void Bind() {
        p_object_ = nullptr;
        p_stub_ = &function_stub<TFunc>;
    }

    template <class T, R (T::*TMethod)(Args... args)> void Bind(T *p_object) {
        p_object_ = p_object;
        p_stub_ = &method_stub<T, TMethod>;
    }

    template <class T, R (T::*TMethod)(Args... args) const> void Bind(const T *p_object) {
        p_object_ = const_cast<T *>(p_object);
        p_stub_ = &const_method_stub<T, TMethod>;
    }

    template <class T, R (T::*TMethod)(Args... args)> static Delegate from_method(T *p_object) {
        return {p_object, &method_stub<T, TMethod>};
    }

    template <class T, R (T::*TMethod)(Args... args) const> static Delegate from_method(const T *p_object) {
        return {const_cast<T *>(p_object), &const_method_stub<T, TMethod>};
    }

    template <R (*TFunc)(Args... args)> static Delegate from_function() { return {nullptr, &function_stub<TFunc>}; }
};

} // namespace Gui
