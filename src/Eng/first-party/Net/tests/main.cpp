
#include <cstdio>

#include <atomic>

#include "../Net.h"
#include "../InterprocessLock.h"

void test_address();
void test_bitmsg();
void test_compress();
void test_hton();
void test_http();
void test_packet_queue();
void test_pcp();
void test_pmp();
void test_reliable_udp_connection();
void test_tcp_socket();
void test_types();
void test_udp_connection();
void test_udp_socket();
void test_var();

bool g_stop_on_fail = false;
std::atomic_bool g_tests_success{true};

int main() {
    printf("Net Version: %s\n", Net::Version());
    puts(" ---------------");

    // Ensure only single instance is running
    Net::InterprocessLock _("test_Net lock");

    test_address();
    test_compress();
    test_hton();
    test_http();
    test_packet_queue();
    test_types();
    test_var();
    test_bitmsg();
    //test_pcp();
    //test_pmp();
    //test_reliable_udp_connection();
    //test_tcp_socket();
    //test_udp_connection();
    //test_udp_socket();
}
