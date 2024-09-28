#include "test_common.h"

#include <cstring>
#include <thread>

#include "../UDPConnection.h"

void test_udp_connection() {
    printf("Test udp_connection     | ");

    using namespace Net;

    { // UDPConnection join
        const int server_port = 30000;
        const int client_port = 30001;
        const int protocol_id = 0x11112222;
        const float dt_s = 0.001f;
        const float timeout_s = 0.1f;

        UDPConnection client(protocol_id, timeout_s);
        UDPConnection server(protocol_id, timeout_s);

        require_nothrow(client.Start(client_port));
        require_nothrow(server.Start(server_port));

        client.Connect(Address(127, 0, 0, 1, server_port));
        server.Listen();

        while (true) {
            if (client.connected() && server.connected()) {
                break;
            }
            if (!client.connecting() && client.connect_failed()) {
                break;
            }
            unsigned char client_packet[] = "client to server";
            client.SendPacket(client_packet, sizeof(client_packet));

            unsigned char server_packet[] = "server to client";
            server.SendPacket(server_packet, sizeof(server_packet));

            while (true) {
                unsigned char packet[256];
                int bytes_read = client.ReceivePacket(packet, sizeof(packet));
                if (bytes_read == 0) {
                    break;
                }
            }

            while (true) {
                unsigned char packet[256];
                int bytes_read = server.ReceivePacket(packet, sizeof(packet));
                if (bytes_read == 0) {
                    break;
                }
            }

            client.Update(dt_s);
            server.Update(dt_s);

            std::this_thread::sleep_for(std::chrono::milliseconds(int(dt_s * 1000)));
        }

        require(client.connected());
        require(server.connected());
    }
    { // UDPConnection join timeout
        const int server_port = 30000;
        const int client_port = 30001;
        const int protocol_id = 0x11112222;
        const float dt_s = 0.001f;
        const float timeout_s = 0.1f;

        UDPConnection client(protocol_id, timeout_s);

        require_nothrow(client.Start(client_port));

        client.Connect(Address(127, 0, 0, 1, server_port));

        while (true) {
            if (!client.connecting()) {
                break;
            }
            unsigned char client_packet[] = "client to server";
            client.SendPacket(client_packet, sizeof(client_packet));

            while (true) {
                unsigned char packet[256];
                int bytes_read = client.ReceivePacket(packet, sizeof(packet));
                if (bytes_read == 0) {
                    break;
                }
            }

            client.Update(dt_s);

            std::this_thread::sleep_for(std::chrono::milliseconds(int(dt_s * 1000)));
        }

        require(!client.connected());
        require(client.connect_failed());
    }
    { // UDPConnection join busy
        const int server_port = 30000;
        const int client_port = 30001;
        const int protocol_id = 0x11112222;
        const float dt_s = 0.001f;
        const float timeout_s = 0.1f;

        // connect client to server
        UDPConnection client(protocol_id, timeout_s);
        UDPConnection server(protocol_id, timeout_s);

        require_nothrow(client.Start(client_port));
        require_nothrow(server.Start(server_port));

        client.Connect(Address(127, 0, 0, 1, server_port));
        server.Listen();

        while (true) {
            if (client.connected() && server.connected()) {
                break;
            }
            if (!client.connecting() && client.connect_failed()) {
                break;
            }
            unsigned char client_packet[] = "client to server";
            client.SendPacket(client_packet, sizeof(client_packet));

            unsigned char server_packet[] = "server to client";
            server.SendPacket(server_packet, sizeof(server_packet));

            while (true) {
                unsigned char packet[256];
                int bytes_read = client.ReceivePacket(packet, sizeof(packet));
                if (bytes_read == 0) {
                    break;
                }
            }

            while (true) {
                unsigned char packet[256];
                int bytes_read = server.ReceivePacket(packet, sizeof(packet));
                if (bytes_read == 0) {
                    break;
                }
            }

            client.Update(dt_s);
            server.Update(dt_s);

            std::this_thread::sleep_for(std::chrono::milliseconds(int(dt_s * 1000)));
        }

        require(client.connected());
        require(server.connected());

        // attempt another connection, verify connect fails (busy)
        UDPConnection busy(protocol_id, timeout_s);
        require_nothrow(busy.Start(client_port + 1));
        busy.Connect(Address(127, 0, 0, 1, server_port));

        while (true) {
            if (!busy.connecting() || busy.connected()) {
                break;
            }
            unsigned char client_packet[] = "client to server";
            client.SendPacket(client_packet, sizeof(client_packet));

            unsigned char server_packet[] = "server to client";
            server.SendPacket(server_packet, sizeof(server_packet));

            unsigned char busy_packet[] = "i'm so busy!";
            busy.SendPacket(busy_packet, sizeof(busy_packet));

            while (true) {
                unsigned char packet[256];
                int bytes_read = client.ReceivePacket(packet, sizeof(packet));
                if (bytes_read == 0) {
                    break;
                }
            }

            while (true) {
                unsigned char packet[256];
                int bytes_read = server.ReceivePacket(packet, sizeof(packet));
                if (bytes_read == 0) {
                    break;
                }
            }

            while (true) {
                unsigned char packet[256];
                int bytes_read = busy.ReceivePacket(packet, sizeof(packet));
                if (bytes_read == 0) {
                    break;
                }
            }

            client.Update(dt_s);
            server.Update(dt_s);
            busy.Update(dt_s);

            std::this_thread::sleep_for(std::chrono::milliseconds(int(dt_s * 1000)));
        }

        require(client.connected());
        require(server.connected());
        require(!busy.connected());
        require(busy.connect_failed());
    }
    { // UDPConnection rejoin
        const int server_port = 30000;
        const int client_port = 30001;
        const int crotocol_id = 0x11112222;
        const float dt_s = 0.001f;
        const float timeout_s = 0.1f;

        UDPConnection client(crotocol_id, timeout_s);
        UDPConnection server(crotocol_id, timeout_s);

        require_nothrow(client.Start(client_port));
        require_nothrow(server.Start(server_port));

        // connect client and server
        client.Connect(Address(127, 0, 0, 1, server_port));
        server.Listen();

        while (true) {
            if (client.connected() && server.connected()) {
                break;
            }
            if (!client.connecting() && client.connect_failed()) {
                break;
            }
            unsigned char client_packet[] = "client to server";
            client.SendPacket(client_packet, sizeof(client_packet));

            unsigned char server_packet[] = "server to client";
            server.SendPacket(server_packet, sizeof(server_packet));

            while (true) {
                unsigned char packet[256];
                int bytes_read = client.ReceivePacket(packet, sizeof(packet));
                if (bytes_read == 0) {
                    break;
                }
            }

            while (true) {
                unsigned char packet[256];
                int bytes_read = server.ReceivePacket(packet, sizeof(packet));
                if (bytes_read == 0) {
                    break;
                }
            }

            client.Update(dt_s);
            server.Update(dt_s);

            std::this_thread::sleep_for(std::chrono::milliseconds(int(dt_s * 1000)));
        }

        require(client.connected());
        require(server.connected());

        // let connection timeout

        while (client.connected() || server.connected()) {
            while (true) {
                unsigned char packet[256];
                int bytes_read = client.ReceivePacket(packet, sizeof(packet));
                if (bytes_read == 0) {
                    break;
                }
            }

            while (true) {
                unsigned char packet[256];
                int bytes_read = server.ReceivePacket(packet, sizeof(packet));
                if (bytes_read == 0) {
                    break;
                }
            }

            client.Update(dt_s);
            server.Update(dt_s);

            std::this_thread::sleep_for(std::chrono::milliseconds(int(dt_s * 1000)));
        }

        require(!client.connected());
        require(!server.connected());

        // reconnect client
        client.Connect(Address(127, 0, 0, 1, server_port));

        while (true) {
            if (client.connected() && server.connected()) {
                break;
            }
            if (!client.connecting() && client.connect_failed()) {
                break;
            }
            unsigned char client_packet[] = "client to server";
            client.SendPacket(client_packet, sizeof(client_packet));

            unsigned char server_packet[] = "server to client";
            server.SendPacket(server_packet, sizeof(server_packet));

            while (true) {
                unsigned char packet[256];
                int bytes_read = client.ReceivePacket(packet, sizeof(packet));
                if (bytes_read == 0) {
                    break;
                }
            }

            while (true) {
                unsigned char packet[256];
                int bytes_read = server.ReceivePacket(packet, sizeof(packet));
                if (bytes_read == 0) {
                    break;
                }
            }

            client.Update(dt_s);
            server.Update(dt_s);

            std::this_thread::sleep_for(std::chrono::milliseconds(int(dt_s * 1000)));
        }

        require(client.connected());
        require(server.connected());
    }
    { // UDPConnection payload
        const int server_port = 30000;
        const int client_port = 30001;
        const int protocol_id = 0x11112222;
        const float dt_s = 0.001f;
        const float timeout_s = 0.1f;

        UDPConnection client(protocol_id, timeout_s);
        UDPConnection server(protocol_id, timeout_s);

        require_nothrow(client.Start(client_port));
        require_nothrow(server.Start(server_port));

        client.Connect(Address(127, 0, 0, 1, server_port));
        server.Listen();

        while (true) {
            if (client.connected() && server.connected())
                break;

            if (!client.connecting() && client.connect_failed())
                break;

            unsigned char client_packet[] = "client to server";
            client.SendPacket(client_packet, sizeof(client_packet));

            unsigned char server_packet[] = "server to client";
            server.SendPacket(server_packet, sizeof(server_packet));

            while (true) {
                unsigned char packet[256];
                int bytes_read = client.ReceivePacket(packet, sizeof(packet));
                if (bytes_read == 0) {
                    break;
                }
                require(strcmp((const char *)packet, "server to client") == 0);
            }

            while (true) {
                unsigned char packet[256];
                int bytes_read = server.ReceivePacket(packet, sizeof(packet));
                if (bytes_read == 0) {
                    break;
                }
                require(strcmp((const char *)packet, "client to server") == 0);
            }

            client.Update(dt_s);
            server.Update(dt_s);

            std::this_thread::sleep_for(std::chrono::milliseconds(int(dt_s * 1000)));
        }

        require(client.connected());
        require(server.connected());
    }

    printf("OK\n");
}
