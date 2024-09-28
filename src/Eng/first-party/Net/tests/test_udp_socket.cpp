#include "test_common.h"

#include <cstring>

#include "../Socket.h"

void test_udp_socket() {
    printf("Test udp_socket         | ");

    using namespace Net;

    { // UDPSocket open/close
        UDPSocket socket;
        require(!socket.IsOpen());
        require_nothrow(socket.Open(30000));
        require(socket.IsOpen());
        socket.Close();
        require(!socket.IsOpen());
        require_nothrow(socket.Open(30000));
        require(socket.IsOpen());
    }
    { // UDPSocket same port fail
        UDPSocket a, b;
        require_nothrow(a.Open(30000, false));
        require_throws(b.Open(30000, false));
        require(a.IsOpen());
        require(!b.IsOpen());
    }
    { // UDPSocket send and receive packets
        UDPSocket a, b;
        require_nothrow(a.Open(30000));
        require_nothrow(b.Open(30001));
        const char packet[] = "packet data";
        bool a_received_packet = false;
        bool b_received_packet = false;
        while (!a_received_packet && !b_received_packet) {
            require(a.Send(Address(127, 0, 0, 1, 30001), packet, sizeof(packet)));
            require(b.Send(Address(127, 0, 0, 1, 30000), packet, sizeof(packet)));

            while (true) {
                Address sender;
                char buffer[256];
                int bytes_read = a.Receive(sender, buffer, sizeof(buffer));
                if (bytes_read == 0) {
                    break;
                }
                if (bytes_read == sizeof(packet) && strcmp(buffer, packet) == 0) {
                    a_received_packet = true;
                }
            }

            while (true) {
                Address sender;
                char buffer[256];
                int bytes_read = b.Receive(sender, buffer, sizeof(buffer));
                if (bytes_read == 0) {
                    break;
                }
                if (bytes_read == sizeof(packet) && strcmp(buffer, packet) == 0) {
                    b_received_packet = true;
                }
            }
        }
        require(a_received_packet);
        require(b_received_packet);
    }

    printf("OK\n");
}
