#include "test_common.h"

#include <cstring>
#include <thread>

#include "../Socket.h"

void test_tcp_socket() {
    using namespace Net;

    printf("Test tcp_socket         | ");

    { // TCPSocket open/close
        TCPSocket socket;
        require(!socket.IsOpen());
        require_nothrow(socket.Open(30000));
        require(socket.IsOpen());
        socket.Close();
        require(!socket.IsOpen());
        require_nothrow(socket.Open(30000));
        require(socket.IsOpen());
    }
    { // TCPSocket same port fail
        TCPSocket a, b;
        require_nothrow(a.Open(30000, false));
        require_throws(b.Open(30000, false));
        require(a.IsOpen());
        require(!b.IsOpen());
    }
    { // TCPSocket send and receive packets
        TCPSocket a, b;
        require_nothrow(a.Open(30000));
        require_nothrow(b.Open(30001));
        const char packet[] = "packet data";
        [[maybe_unused]] bool a_received_packet = false;
        [[maybe_unused]] bool b_received_packet = false;
        require(b.Listen());

        std::thread thr([&]() {
            for (int i = 0; i < 1000; ++i) {
                if (b.Accept()) {
                    for (int j = 0; j < 1000; ++j) {
                        char buffer[256];
                        int bytes_read = b.Receive(buffer, sizeof(buffer));
                        if (bytes_read) {
                            require(bytes_read == sizeof(packet));
                            require(b.Send(packet, sizeof(packet)));
                            bytes_read = a.Receive(buffer, sizeof(buffer));
                            require(bytes_read == sizeof(packet));
                            return;
                        }
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            require(false);
        });
        require(a.Connect(Address(127, 0, 0, 1, 30001)));
        require(a.Send(packet, sizeof(packet)));
        thr.join();
    }

    printf("OK\n");
}
