#include "test_common.h"

#include <cstring>

#include "../Socket.h"

void test_udp_socket() {
    using namespace Net;

    printf("Test udp_socket         | ");

    { // UDPSocket open/close
        UDPSocket socket;
        require(!socket.IsOpen());
        require_nothrow(socket.Open(0));
        require(socket.IsOpen());
        require(socket.local_addr().port() != 0);
        socket.Close();
        require(!socket.IsOpen());
        require_nothrow(socket.Open(30000));
        require(socket.local_addr().port() == 30000);
        require(socket.IsOpen());
    }
    { // an explicit bind address is reported back, not the gethostbyname() guess
        UDPSocket s;
        require_nothrow(s.Open(Address(127, 0, 0, 1, 0), 0));
        require(s.local_addr().a() == 127);
        require(s.local_addr().port() != 0);
    }
    { // UDPSocket same port fail
        UDPSocket a, b;
        require_nothrow(a.Open(30000, false));
        require_throws(b.Open(30000, false));
        require(a.IsOpen());
        require(!b.IsOpen());
    }
    { // a fresh socket has no data pending: -1 (would-block), not 0
        UDPSocket a;
        require_nothrow(a.Open(0));

        Address sender;
        char buffer[64];
        require(a.Receive(sender, buffer, sizeof(buffer)) == -1);
    }
    { // closed socket reports an error, not a fake zero-length datagram
        UDPSocket a;
        require_nothrow(a.Open(0));
        a.Close();

        Address sender;
        char buffer[64];
        require(a.Receive(sender, buffer, sizeof(buffer)) < 0);
    }
    { // UDPSocket send and receive packets
        UDPSocket a, b;
        require_nothrow(a.Open(0));
        require_nothrow(b.Open(0));

        const char packet[] = "packet data";
        bool a_received_packet = false;
        bool b_received_packet = false;
        for (int attempt = 0; attempt < 100 && !(a_received_packet && b_received_packet); ++attempt) {
            require(a.Send(Address(127, 0, 0, 1, b.local_addr().port()), packet, sizeof(packet)));
            require(b.Send(Address(127, 0, 0, 1, a.local_addr().port()), packet, sizeof(packet)));

            while (true) {
                Address sender;
                char buffer[256];
                int bytes_read = a.Receive(sender, buffer, sizeof(buffer));
                if (bytes_read <= 0) {
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
                if (bytes_read <= 0) {
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
