
#include <cstdio>
#include <cstdlib>

#include <atomic>
#include <chrono>
#include <cstring>
#include <stdexcept>
#include <thread>

#include "../Net.h"
#include "../InterprocessLock.h"

#include "test_common.h"

void test_address();
void test_bitmsg();
void test_compress();
void test_hash();
void test_hton();
void test_http();
void test_interprocess_lock();
void test_packet_queue();
void test_pcp();
void test_pmp();
void test_msg_channel();
void test_reliable_udp_connection();
void test_tcp_socket();
void test_udp_connection();
void test_udp_socket();
void test_var();
void test_ws_connection();
// no test_types(): its TEST_BE define changes Net::le_* type definitions and would ODR-clash with this binary

bool g_stop_on_fail = false;
std::atomic_int g_failures{};

int main(int argc, char **argv) {
    // helper mode for test_interprocess_lock: take the lock, hold it, report via exit code
    if (argc > 1 && std::strcmp(argv[1], "--lock-hold") == 0) {
        const Net::InterprocessLock _("test_Net lock hold", Net::InterprocessLock::try_to_lock_t{});
        if (!_.locked()) {
            return 3;
        }
        std::this_thread::sleep_for(std::chrono::seconds(3));
        return 0;
    }

    printf("Net Version: %s\n", Net::Version());
    puts(" ---------------");

    // Ensure only single instance is running; failing to take the lock (abandoned mutex,
    // permission problems) must not kill the run by itself
    try {
        static Net::InterprocessLock _("test_Net lock");
    } catch (const std::exception &e) {
        printf("warning: no single-instance lock: %s\n", e.what());
    }

    // hard deadline: a hung blocking call must fail the run, not stall CI forever
    std::thread watchdog([] {
        std::this_thread::sleep_for(std::chrono::minutes(5));
        fflush(stdout);
        fputs("FATAL: test_Net exceeded 5 minutes, aborting\n", stderr);
        std::_Exit(2);
    });
    watchdog.detach();

    test_address();
    test_compress();
    test_hash();
    test_hton();
    test_http();
    test_interprocess_lock();
    test_packet_queue();
    test_var();
    test_bitmsg();
    test_pcp();
    test_pmp();
    test_msg_channel();
    test_reliable_udp_connection();
    test_udp_connection();
    test_udp_socket();
    test_tcp_socket();
    test_ws_connection();

    return g_failures ? 1 : 0;
}
