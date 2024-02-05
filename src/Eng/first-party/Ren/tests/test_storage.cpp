#include "test_common.h"

#include "../Storage.h"

void test_storage() {
    printf("Test storage            | ");

    class MyObj : public Ren::RefCounter {
        Ren::String name_;

      public:
        int *ref;

        MyObj() : ref(nullptr) {}
        MyObj(const char *name, int *r) : name_(name), ref(r) { (*ref)++; }
        MyObj(const MyObj &rhs) = delete;
        MyObj(MyObj &&rhs) noexcept {
            name_ = std::move(rhs.name_);
            ref = rhs.ref;
            rhs.ref = nullptr;
        }
        MyObj &operator=(const MyObj &rhs) = delete;
        MyObj &operator=(MyObj &&rhs) noexcept {
            if (ref) {
                (*ref)--;
            }
            name_ = std::move(rhs.name_);
            ref = rhs.ref;
            rhs.ref = nullptr;
            return (*this);
        }

        const Ren::String &name() { return name_; }

        ~MyObj() {
            if (ref) {
                (*ref)--;
            }
        }
    };

    {
        // test create/delete
        Ren::Storage<MyObj> my_obj_storage;
        int counter = 0;

        auto ref1 = my_obj_storage.Add("obj", &counter);
        require(counter == 1);
        ref1 = {};
        require(counter == 0);
    }

    {
        // test copy/move reference
        Ren::Storage<MyObj> my_obj_storage;
        int counter = 0;

        auto ref1 = my_obj_storage.Add("obj1", &counter);
        require(counter == 1);
        auto ref2 = ref1;
        require(counter == 1);
        ref1 = {};
        require(counter == 1);
        ref2 = {};
        require(counter == 0);

        ref1 = my_obj_storage.Add("obj2", &counter);
        require(counter == 1);
        ref2 = std::move(ref1);
        require(counter == 1);
        ref2 = {};
        require(counter == 0);
    }

    printf("OK\n");
}