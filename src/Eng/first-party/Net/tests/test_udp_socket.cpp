#include "test_common.h"

#include <cstring>

#include "../Socket.h"

void test_udp_socket() {

    { // UDPSocket open/close
        Net::UDPSocket socket;
        assert(!socket.IsOpen());
        assert_nothrow(socket.Open(30000));
        assert(socket.IsOpen());
        socket.Close();
        assert(!socket.IsOpen());
        assert_nothrow(socket.Open(30000));
        assert(socket.IsOpen());
    }

    { // UDPSocket same port fail
        Net::UDPSocket a, b;
        assert_nothrow(a.Open(30000, false));
        assert_throws(b.Open(30000, false));
        assert(a.IsOpen());
        assert(!b.IsOpen());
    }

    { // UDPSocket send and receive packets
        Net::UDPSocket a, b;
        assert_nothrow(a.Open(30000));
        assert_nothrow(b.Open(30001));
        const char packet[] = "packet data";
        bool a_received_packet = false;
        bool b_received_packet = false;
        while (!a_received_packet && !b_received_packet) {
            assert(a.Send(Net::Address(127, 0, 0, 1, 30001), packet, sizeof(packet)));
            assert(b.Send(Net::Address(127, 0, 0, 1, 30000), packet, sizeof(packet)));

            while (true) {
                Net::Address sender;
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
                Net::Address sender;
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
        assert(a_received_packet);
        assert(b_received_packet);
    }
}
