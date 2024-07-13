#include "test_common.h"

#include "../InplaceFunction.h"

void test_inplace_function() {
    printf("Test inplace_function   | ");

    struct BigStruct {
        char data[8];
        int number = 42;
    } big_struct;

    Sys::InplaceFunction<int(int)> func1;
    require(func1 == nullptr);
    func1 = [big_struct](int a) { return big_struct.number + a; };
    require(func1 != nullptr);

    const int val1 = func1(2);
    require(val1 == 44);

    Sys::InplaceFunction<int(int)> func2(func1);
    const int val2 = func2(3);
    require(val2 == 45);

    Sys::InplaceFunction<int(int)> func3(std::move(func2));
    require(func2 == nullptr);
    require(func3 != nullptr);

    Sys::InplaceFunction<int(int)> func4;
    func4 = func3;

    const int val3 = func4(1);
    require(val3 == 43);

    Sys::InplaceFunction<int(int)> func5;
    func5 = std::move(func4);

    const int val4 = func5(5);
    require(val4 == 47);

    func5 = nullptr;
    require(!func5);

    printf("OK\n");
}
