#pragma once

#include <typeinfo>
#include <type_traits>

namespace Sys {
    // taken from https://gist.github.com/tibordp/6909880

    template <size_t arg1, size_t ...args>
    struct _compile_time_max;

    template <size_t arg>
    struct _compile_time_max<arg> {
        static const size_t value = arg;
    };

    template <size_t arg1, size_t arg2, size_t ...args>
    struct _compile_time_max<arg1, arg2, args...> {
        static const size_t value = arg1 >= arg2 ? _compile_time_max<arg1, args...>::value : _compile_time_max<arg2, args...>::value;
    };

    template<typename... Ts>
    struct _variant_helper;

    template<typename F, typename... Ts>
    struct _variant_helper<F, Ts...> {
        inline static void destroy(size_t id, void *data) {
            if (id == typeid(F).hash_code()) {
                reinterpret_cast<F*>(data)->~F();
            } else {
                _variant_helper<Ts...>::destroy(id, data);
            }
        }

        inline static void move(size_t old_t, void *old_v, void *new_v) {
            if (old_t == typeid(F).hash_code()) {
                new (new_v) F(std::move(*reinterpret_cast<F*>(old_v)));
            } else {
                _variant_helper<Ts...>::move(old_t, old_v, new_v);
            }
        }

        inline static void copy(size_t old_t, const void *old_v, void *new_v) {
            if (old_t == typeid(F).hash_code()) {
                new (new_v) F(*reinterpret_cast<const F*>(old_v));
            } else {
                _variant_helper<Ts...>::copy(old_t, old_v, new_v);
            }
        }
    };

    template<> struct _variant_helper<> {
        inline static void destroy(size_t id, void * data) {}
        inline static void move(size_t old_t, void * old_v, void * new_v) {}
        inline static void copy(size_t old_t, const void * old_v, void * new_v) {}
    };

    template<typename... Ts>
    struct Variant {
    private:
        static const size_t
            data_size = _compile_time_max<sizeof(Ts)...>::value,
            data_align = _compile_time_max<alignof(Ts)...>::value;

        using data_t = typename std::aligned_storage<data_size, data_align>::type;
        using helper_t = _variant_helper<Ts...>;

        static inline size_t invalid_type() {
            return typeid(void).hash_code();
        }

        size_t type_id_;
        data_t data_;
    public:
        Variant() : type_id_(invalid_type()) {}

        Variant(const Variant<Ts...> &old) : type_id_(old.type_id_) {
            helper_t::copy(old.type_id_, &old.data_, &data_);
        }

        Variant(Variant<Ts...> &&old) : type_id_(old.type_id_) {
            helper_t::move(old.type_id_, &old.data_, &data_);
        }

        // Serves as both the move and the copy asignment operator.
        Variant<Ts...> &operator= (Variant<Ts...> old) {
            std::swap(type_id_, old.type_id_);
            std::swap(data_, old.data_);

            return *this;
        }

        template<typename T>
        bool is() {
            return (type_id_ == typeid(T).hash_code());
        }

        bool valid() {
            return (type_id_ != invalid_type());
        }

        template<typename T, typename... Args>
        void set(Args &&...args) {
            // First we destroy the current contents    
            helper_t::destroy(type_id_, &data_);
            new (&data_) T(std::forward<Args>(args)...);
            type_id_ = typeid(T).hash_code();
        }

        template<typename T>
        T &get() {
            // It is a dynamic_cast-like behaviour
            if (type_id_ == typeid(T).hash_code()) {
                return *reinterpret_cast<T*>(&data_);
            } else {
                throw std::bad_cast();
            }
        }

        ~Variant() {
            helper_t::destroy(type_id_, &data_);
        }
    };
}