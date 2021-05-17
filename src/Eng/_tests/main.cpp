
#include <cstdio>

#include <vtune/ittnotify.h>
__itt_domain* __g_itt_domain = __itt_domain_create("Global");

//void test_object_pool();
void test_cmdline();
void test_unicode();
void test_widgets();

int main() {
    //test_object_pool();
    test_cmdline();
    test_unicode();
    test_widgets();
    puts("OK");
}
