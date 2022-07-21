#include "test_common.h"

#include <cstring>
#include <thread>

#include "../Socket.h"

void test_tcp_socket() {

    { // TCPSocket open/close
        Net::TCPSocket socket;
        assert(!socket.IsOpen());
        assert_nothrow(socket.Open(30000));
        assert(socket.IsOpen());
        socket.Close();
        assert(!socket.IsOpen());
        assert_nothrow(socket.Open(30000));
        assert(socket.IsOpen());
    }

    { // TCPSocket same port fail
        Net::TCPSocket a, b;
        assert_nothrow(a.Open(30000, false));
        assert_throws(b.Open(30000, false));
        assert(a.IsOpen());
        assert(!b.IsOpen());
    }

    { // TCPSocket send and receive packets
        Net::TCPSocket a, b;
        assert_nothrow(a.Open(30000));
        assert_nothrow(b.Open(30001));
        const char packet[] = "packet data";
        bool a_received_packet = false;
        bool b_received_packet = false;
        assert(b.Listen());

        std::thread thr([&]() {
            for(int i = 0; i < 1000; ++i) {
                if (b.Accept()) {
                    for(int j = 0; j < 1000; ++j) {
                        char buffer[256];
                        int bytes_read = b.Receive(buffer, sizeof(buffer));
                        if (bytes_read) {
                            assert(bytes_read == sizeof(packet));
                            assert(b.Send(packet, sizeof(packet)));
                            bytes_read = a.Receive(buffer, sizeof(buffer));
                            assert(bytes_read == sizeof(packet));
                            return;
                        }
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            assert(false);
        });
        assert(a.Connect(Net::Address(127, 0, 0, 1, 30001)));
        assert(a.Send(packet, sizeof(packet)));
        thr.join();
    }
}
