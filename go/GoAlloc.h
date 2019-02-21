#pragma once

#include <memory>

#include "../ObjectPool.h"

template <class T>
struct GoAlloc {
    static void Delete(T *p) {
        pool.Delete(p);
    }
    static void Free(T *p) {
        pool.Free(p);
    }
    typedef std::unique_ptr<T, decltype(GoAlloc<T>::Delete)*> Ref;

    template<class... Args>
    static T *New(Args &&... args) {
        return pool.New(args...);
    }
    template<class... Args>
    static Ref NewU(Args &&... args) {
        return Ref(pool.New(args...), &Delete);
    }
    static ObjectPool<T> pool;
};

template <class T>
ObjectPool<T> GoAlloc<T>::pool(32);

template <class T>
using GoRef = typename GoAlloc<T>::Ref;


#define OVERRIDE_NEW(T)                         \
    template<class... Args>                     \
    void *operator new(size_t size, Args &&... args) {           \
        assert(size == sizeof(T));              \
        return GoAlloc<T>::New(args...);        \
    }                                           \
    void *operator new(size_t size, void *pl) { \
        assert(size == sizeof(T));              \
        return ::operator new(size, pl);        \
    }                                           \
    void operator delete(void *p) {             \
        if (p) {                                \
            GoAlloc<T>::Free((T*)p);            \
        }                                       \
    }											\
	void operator delete(void *p, void *pl) {	\
		::operator delete(p, pl);				\
	}
