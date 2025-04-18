#include "test_common.h"

#include <cstring>
#include <thread>

#include "../ReliableUDPConnection.h"

namespace {
const int maximum_sequence = 255;
}

void test_reliable_udp_connection() {
    using namespace Net;

    printf("Test rel_udp_connection | ");

    { // Check bit index for sequence
        require(ReliabilitySystem::bit_index_for_sequence(99, 100, maximum_sequence) == 0);
        require(ReliabilitySystem::bit_index_for_sequence(90, 100, maximum_sequence) == 9);
        require(ReliabilitySystem::bit_index_for_sequence(0, 1, maximum_sequence) == 0);
        require(ReliabilitySystem::bit_index_for_sequence(255, 0, maximum_sequence) == 0);
        require(ReliabilitySystem::bit_index_for_sequence(255, 1, maximum_sequence) == 1);
        require(ReliabilitySystem::bit_index_for_sequence(254, 1, maximum_sequence) == 2);
        require(ReliabilitySystem::bit_index_for_sequence(254, 2, maximum_sequence) == 3);
    }
    { // Check generate ack bits
        PacketQueue packet_queue;
        for (int i = 0; i < 32; ++i) {
            PacketData data(i, 0, 0);
            packet_queue.insert_sorted(data, maximum_sequence);
        }
        require(ReliabilitySystem::generate_ack_bits(32, packet_queue, maximum_sequence) == 0xFFFFFFFF);
        require(ReliabilitySystem::generate_ack_bits(31, packet_queue, maximum_sequence) == 0x7FFFFFFF);
        require(ReliabilitySystem::generate_ack_bits(33, packet_queue, maximum_sequence) == 0xFFFFFFFE);
        require(ReliabilitySystem::generate_ack_bits(16, packet_queue, maximum_sequence) == 0x0000FFFF);
        require(ReliabilitySystem::generate_ack_bits(48, packet_queue, maximum_sequence) == 0xFFFF0000);
    }
    { // Check generate ack bits with wrap
        PacketQueue packet_queue;
        for (int i = 255 - 31; i <= 255; ++i) {
            PacketData data(i, 0, 0);
            packet_queue.insert_sorted(data, maximum_sequence);
        }
        require(packet_queue.size() == 32);
        require(ReliabilitySystem::generate_ack_bits(0, packet_queue, maximum_sequence) == 0xFFFFFFFF);
        require(ReliabilitySystem::generate_ack_bits(255, packet_queue, maximum_sequence) == 0x7FFFFFFF);
        require(ReliabilitySystem::generate_ack_bits(1, packet_queue, maximum_sequence) == 0xFFFFFFFE);
        require(ReliabilitySystem::generate_ack_bits(240, packet_queue, maximum_sequence) == 0x0000FFFF);
        require(ReliabilitySystem::generate_ack_bits(16, packet_queue, maximum_sequence) == 0xFFFF0000);
    }
    { // Check process ack (1)
        PacketQueue packet_queue;
        for (int i = 0; i < 33; ++i) {
            PacketData data(i, 0, 0);
            packet_queue.insert_sorted(data, maximum_sequence);
        }
        PacketQueue acked_queue;
        std::vector<unsigned int> acks;
        float rtt = 0;
        unsigned int acked_packets = 0;
        ReliabilitySystem::process_ack(32, 0xFFFFFFFF, packet_queue, acked_queue, acks, acked_packets, rtt,
                                       maximum_sequence);
        require(acks.size() == 33);
        require(acked_packets == 33);
        require(acked_queue.size() == 33);
        require(packet_queue.size() == 0);
        require(acked_queue.verify_sorted(maximum_sequence));
        for (unsigned int i = 0; i < acks.size(); ++i) {
            require(acks[i] == i);
        }
        unsigned int i = 0;
        for (PacketQueue::iterator it = acked_queue.begin(); it != acked_queue.end(); ++it, ++i) {
            require(it->sequence == i);
        }
    }
    { // Check process ack (2)
        PacketQueue pending_ack_queue;
        for (int i = 0; i < 33; ++i) {
            PacketData data(i, 0, 0);
            pending_ack_queue.insert_sorted(data, maximum_sequence);
        }
        PacketQueue acked_queue;
        std::vector<unsigned int> acks;
        float rtt = 0;
        unsigned int acked_packets = 0;
        ReliabilitySystem::process_ack(32, 0x0000FFFF, pending_ack_queue, acked_queue, acks, acked_packets, rtt,
                                       maximum_sequence);
        require(acks.size() == 17);
        require(acked_packets == 17);
        require(acked_queue.size() == 17);
        require(pending_ack_queue.size() == 33 - 17);
        require(acked_queue.verify_sorted(maximum_sequence));
        unsigned int i = 0;
        for (PacketQueue::iterator it = pending_ack_queue.begin(); it != pending_ack_queue.end(); ++it, ++i) {
            require(it->sequence == i);
        }
        i = 0;
        for (PacketQueue::iterator it = acked_queue.begin(); it != acked_queue.end(); ++it, ++i) {
            require(it->sequence == i + 16);
        }
        for (unsigned int j = 0; j < acks.size(); ++j) {
            require(acks[j] == j + 16);
        }
    }
    { // Check process ack (3)
        PacketQueue pending_ack_queue;
        for (int i = 0; i < 32; ++i) {
            PacketData data(i, 0, 0);
            pending_ack_queue.insert_sorted(data, maximum_sequence);
        }
        PacketQueue acked_queue;
        std::vector<unsigned int> acks;
        float rtt = 0;
        unsigned int acked_packets = 0;
        ReliabilitySystem::process_ack(48, 0xFFFF0000, pending_ack_queue, acked_queue, acks, acked_packets, rtt,
                                       maximum_sequence);
        require(acks.size() == 16);
        require(acked_packets == 16);
        require(acked_queue.size() == 16);
        require(pending_ack_queue.size() == 16);
        acked_queue.verify_sorted(maximum_sequence);
        unsigned int i = 0;
        for (PacketQueue::iterator it = pending_ack_queue.begin(); it != pending_ack_queue.end(); ++it, ++i) {
            require(it->sequence == i);
        }
        i = 0;
        for (PacketQueue::iterator it = acked_queue.begin(); it != acked_queue.end(); ++it, ++i) {
            require(it->sequence == i + 16);
        }
        for (unsigned int j = 0; j < acks.size(); ++j) {
            require(acks[j] == j + 16);
        }
    }
    { // Check process ack wrap around (1)
        PacketQueue pending_ack_queue;
        for (int i = 255 - 31; i <= 256; ++i) {
            PacketData data(i & 0xFF, 0, 0);
            pending_ack_queue.insert_sorted(data, maximum_sequence);
            pending_ack_queue.verify_sorted(maximum_sequence);
        }
        require(pending_ack_queue.size() == 33);
        PacketQueue acked_queue;
        std::vector<unsigned int> acks;
        float rtt = 0;
        unsigned int acked_packets = 0;
        ReliabilitySystem::process_ack(0, 0xFFFFFFFF, pending_ack_queue, acked_queue, acks, acked_packets, rtt,
                                       maximum_sequence);
        require(acks.size() == 33);
        require(acked_packets == 33);
        require(acked_queue.size() == 33);
        require(pending_ack_queue.size() == 0);
        acked_queue.verify_sorted(maximum_sequence);
        for (unsigned int i = 0; i < acks.size(); ++i) {
            require(acks[i] == ((i + 255 - 31) & 0xFF));
        }
        unsigned int i = 0;
        for (PacketQueue::iterator it = acked_queue.begin(); it != acked_queue.end(); ++it, ++i) {
            require(it->sequence == ((i + 255 - 31) & 0xFF));
        }
    }
    { // Check process ack wrap around (2)
        PacketQueue pending_ack_queue;
        for (int i = 255 - 31; i <= 256; ++i) {
            PacketData data(i & 0xFF, 0, 0);
            pending_ack_queue.insert_sorted(data, maximum_sequence);
        }
        require(pending_ack_queue.size() == 33);
        PacketQueue acked_queue;
        std::vector<unsigned int> acks;
        float rtt = 0;
        unsigned int acked_packets = 0;
        ReliabilitySystem::process_ack(0, 0x0000FFFF, pending_ack_queue, acked_queue, acks, acked_packets, rtt,
                                       maximum_sequence);
        require(acks.size() == 17);
        require(acked_packets == 17);
        require(acked_queue.size() == 17);
        require(pending_ack_queue.size() == 33 - 17);
        acked_queue.verify_sorted(maximum_sequence);
        for (unsigned int i = 0; i < acks.size(); ++i) {
            require(acks[i] == ((i + 255 - 15) & 0xFF));
        }
        unsigned int i = 0;
        for (PacketQueue::iterator it = pending_ack_queue.begin(); it != pending_ack_queue.end(); ++it, ++i) {
            require(it->sequence == i + 255 - 31);
        }
        i = 0;
        for (PacketQueue::iterator it = acked_queue.begin(); it != acked_queue.end(); ++it, ++i) {
            require(it->sequence == ((i + 255 - 15) & 0xFF));
        }
    }
    { // Check process ack wrap around (3)
        PacketQueue pending_ack_queue;
        for (int i = 255 - 31; i <= 255; ++i) {
            PacketData data(i & 0xFF, 0, 0);
            pending_ack_queue.insert_sorted(data, maximum_sequence);
        }
        require(pending_ack_queue.size() == 32);
        PacketQueue acked_queue;
        std::vector<unsigned int> acks;
        float rtt = 0;
        unsigned int acked_packets = 0;
        ReliabilitySystem::process_ack(16, 0xFFFF0000, pending_ack_queue, acked_queue, acks, acked_packets, rtt,
                                       maximum_sequence);
        require(acks.size() == 16);
        require(acked_packets == 16);
        require(acked_queue.size() == 16);
        require(pending_ack_queue.size() == 16);
        acked_queue.verify_sorted(maximum_sequence);
        for (unsigned int i = 0; i < acks.size(); ++i) {
            require(acks[i] == ((i + 255 - 15) & 0xFF));
        }
        unsigned int i = 0;
        for (PacketQueue::iterator it = pending_ack_queue.begin(); it != pending_ack_queue.end(); ++it, ++i) {
            require(it->sequence == i + 255 - 31);
        }
        i = 0;
        for (PacketQueue::iterator it = acked_queue.begin(); it != acked_queue.end(); ++it, ++i) {
            require(it->sequence == ((i + 255 - 15) & 0xFF));
        }
    }
    { // Test join
        const int server_port = 30000;
        const int client_port = 30001;
        const int protocol_id = 0x11112222;
        const float dt_s = 0.001f;
        const float timeout_s_s = 1;

        ReliableUDPConnection client(protocol_id, timeout_s_s);
        ReliableUDPConnection server(protocol_id, timeout_s_s);

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
    { // Test payload
        const int server_port = 30000;
        const int client_port = 30001;
        const int protocol_id = 0x11112222;
        const float dt_s = 0.001f;
        const float timeout_s_s = 0.1f;

        ReliableUDPConnection client(protocol_id, timeout_s_s);
        ReliableUDPConnection server(protocol_id, timeout_s_s);

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
    { // Test acks
        const int server_port = 30000;
        const int client_port = 30001;
        const int protocol_id = 0x11112222;
        const float dt_s = 0.001f;
        const float timeout_s = 0.1f;
        const unsigned int packet_count = 100;

        ReliableUDPConnection client(protocol_id, timeout_s);
        ReliableUDPConnection server(protocol_id, timeout_s);

        require_nothrow(client.Start(client_port));
        require_nothrow(server.Start(server_port));

        client.Connect(Address(127, 0, 0, 1, server_port));
        server.Listen();

        bool clientAckedPackets[packet_count];
        bool serverAckedPackets[packet_count];
        for (unsigned int i = 0; i < packet_count; ++i) {
            clientAckedPackets[i] = false;
            serverAckedPackets[i] = false;
        }

        bool all_packets_acked = false;

        while (true) {
            if (!client.connecting() && client.connect_failed()) {
                break;
            }
            if (all_packets_acked) {
                break;
            }
            static std::vector<unsigned char> sent_packet(32);
            for (unsigned int i = 0; i < sent_packet.size(); ++i) {
                sent_packet[i] = (unsigned char)i;
            }
            server.SendPacket(&sent_packet[0], int(sent_packet.size()));
            client.SendPacket(&sent_packet[0], int(sent_packet.size()));

            static std::vector<unsigned char> rcv_packet(32);
            while (true) {
                int bytes_read = client.ReceivePacket(&rcv_packet[0], int(rcv_packet.size()));
                if (bytes_read == 0) {
                    break;
                }
                require(bytes_read == rcv_packet.size());
                require(rcv_packet == sent_packet);
            }

            while (true) {
                int bytes_read = server.ReceivePacket(&rcv_packet[0], int(rcv_packet.size()));
                if (bytes_read == 0) {
                    break;
                }
                require(bytes_read == rcv_packet.size());
                require(rcv_packet == sent_packet);
            }

            int ack_count = 0;
            unsigned int *acks = NULL;
            client.reliability_system().GetAcks(&acks, ack_count);
            require((ack_count == 0 || (ack_count != 0 && acks)));
            for (int i = 0; i < ack_count; ++i) {
                unsigned int ack = acks[i];
                if (ack < packet_count) {
                    require(!clientAckedPackets[ack]);
                    clientAckedPackets[ack] = true;
                }
            }

            server.reliability_system().GetAcks(&acks, ack_count);
            require((ack_count == 0 || (ack_count != 0 && acks)));
            for (int i = 0; i < ack_count; ++i) {
                unsigned int ack = acks[i];
                if (ack < packet_count) {
                    require(!serverAckedPackets[ack]);
                    serverAckedPackets[ack] = true;
                }
            }

            unsigned int clientAckCount = 0;
            unsigned int serverAckCount = 0;
            for (unsigned int i = 0; i < packet_count; ++i) {
                clientAckCount += clientAckedPackets[i];
                serverAckCount += serverAckedPackets[i];
            }
            all_packets_acked = clientAckCount == packet_count && serverAckCount == packet_count;

            client.Update(dt_s);
            server.Update(dt_s);

            std::this_thread::sleep_for(std::chrono::milliseconds(int(dt_s * 1000)));
        }

        require(client.connected());
        require(server.connected());
    }
    { // Test ack bits
        const int server_port = 30000;
        const int client_port = 30001;
        const int protocol_id = 0x11112222;
        const float dt_s = 0.001f;
        const float timeout_s = 0.1f;
        const unsigned int packet_count = 100;

        ReliableUDPConnection client(protocol_id, timeout_s);
        ReliableUDPConnection server(protocol_id, timeout_s);

        require_nothrow(client.Start(client_port));
        require_nothrow(server.Start(server_port));

        client.Connect(Address(127, 0, 0, 1, server_port));
        server.Listen();

        bool clientAckedPackets[packet_count];
        bool serverAckedPackets[packet_count];
        for (unsigned int i = 0; i < packet_count; ++i) {
            clientAckedPackets[i] = false;
            serverAckedPackets[i] = false;
        }

        bool all_packets_acked = false;

        while (true) {
            if (!client.connecting() && client.connect_failed()) {
                break;
            }
            if (all_packets_acked) {
                break;
            }
            unsigned char snd_packet[32];
            for (unsigned int i = 0; i < sizeof(snd_packet); ++i) {
                snd_packet[i] = (unsigned char)i;
            }
            for (int i = 0; i < 10; ++i) {
                client.SendPacket(snd_packet, sizeof(snd_packet));

                while (true) {
                    unsigned char rcv_packet[32];
                    int bytes_read = client.ReceivePacket(rcv_packet, sizeof(rcv_packet));
                    if (bytes_read == 0) {
                        break;
                    }
                    require(bytes_read == sizeof(rcv_packet));
                    for (unsigned int j = 0; j < sizeof(rcv_packet); ++j) {
                        require(rcv_packet[j] == (unsigned char)j);
                    }
                }

                int ack_count = 0;
                unsigned int *acks = NULL;
                client.reliability_system().GetAcks(&acks, ack_count);
                require((ack_count == 0 || (ack_count != 0 && acks)));
                for (int j = 0; j < ack_count; ++j) {
                    const unsigned int ack = acks[j];
                    if (ack < packet_count) {
                        require(!clientAckedPackets[ack]);
                        clientAckedPackets[ack] = true;
                    }
                }

                client.Update(dt_s * 0.1f);
            }

            server.SendPacket(snd_packet, sizeof(snd_packet));

            while (true) {
                unsigned char packet[32];
                int bytes_read = server.ReceivePacket(packet, sizeof(packet));
                if (bytes_read == 0) {
                    break;
                }
                require(bytes_read == sizeof(packet));
                for (unsigned int i = 0; i < sizeof(packet); ++i) {
                    require(packet[i] == (unsigned char)i);
                }
            }

            int ack_count = 0;
            unsigned int *acks = NULL;
            server.reliability_system().GetAcks(&acks, ack_count);
            require((ack_count == 0 || (ack_count != 0 && acks)));
            for (int i = 0; i < ack_count; ++i) {
                unsigned int ack = acks[i];
                if (ack < packet_count) {
                    require(!serverAckedPackets[ack]);
                    serverAckedPackets[ack] = true;
                }
            }

            unsigned int clientAckCount = 0;
            unsigned int serverAckCount = 0;
            for (unsigned int i = 0; i < packet_count; ++i) {
                if (clientAckedPackets[i]) {
                    clientAckCount++;
                }
                if (serverAckedPackets[i]) {
                    serverAckCount++;
                }
            }
            all_packets_acked = clientAckCount == packet_count && serverAckCount == packet_count;

            server.Update(dt_s);

            std::this_thread::sleep_for(std::chrono::milliseconds(int(dt_s * 1000)));
        }

        require(client.connected());
        require(server.connected());
    }
    { // Test packet loss
        const int server_port = 30000;
        const int client_port = 30001;
        const int protocol_id = 0x11112222;
        const float dt_s = 0.001f;
        const float timeout_s = 0.1f;
        const unsigned int packet_count = 100;

        ReliableUDPConnection client(protocol_id, timeout_s);
        ReliableUDPConnection server(protocol_id, timeout_s);

        client.set_packet_loss_mask(1);
        server.set_packet_loss_mask(1);

        require_nothrow(client.Start(client_port));
        require_nothrow(server.Start(server_port));

        client.Connect(Address(127, 0, 0, 1, server_port));
        server.Listen();

        bool clientAckedPackets[packet_count];
        bool serverAckedPackets[packet_count];
        for (unsigned int i = 0; i < packet_count; ++i) {
            clientAckedPackets[i] = false;
            serverAckedPackets[i] = false;
        }

        bool all_packets_acked = false;

        while (true) {
            if (!client.connecting() && client.connect_failed()) {
                break;
            }
            if (all_packets_acked) {
                break;
            }
            unsigned char snd_packet[32];
            for (unsigned int i = 0; i < sizeof(snd_packet); ++i) {
                snd_packet[i] = (unsigned char)i;
            }
            for (int i = 0; i < 10; ++i) {
                client.SendPacket(snd_packet, sizeof(snd_packet));

                while (true) {
                    unsigned char rcv_packet[32];
                    int bytes_read = client.ReceivePacket(rcv_packet, sizeof(rcv_packet));
                    if (bytes_read == 0)
                        break;
                    require(bytes_read == sizeof(rcv_packet));
                    for (unsigned int j = 0; j < sizeof(rcv_packet); ++j) {
                        require(rcv_packet[j] == (unsigned char)j);
                    }
                }

                int ack_count = 0;
                unsigned int *acks = NULL;
                client.reliability_system().GetAcks(&acks, ack_count);
                require((ack_count == 0 || (ack_count != 0 && acks)));
                for (int j = 0; j < ack_count; ++j) {
                    const unsigned int ack = acks[j];
                    if (ack < packet_count) {
                        require(!clientAckedPackets[ack]);
                        require((ack & 1) == 0);
                        clientAckedPackets[ack] = true;
                    }
                }

                client.Update(dt_s * 0.1f);
            }

            server.SendPacket(snd_packet, sizeof(snd_packet));

            while (true) {
                unsigned char rcv_packet[32];
                int bytes_read = server.ReceivePacket(rcv_packet, sizeof(rcv_packet));
                if (bytes_read == 0) {
                    break;
                }
                require(bytes_read == sizeof(rcv_packet));
                for (unsigned int i = 0; i < sizeof(rcv_packet); ++i) {
                    require(rcv_packet[i] == (unsigned char)i);
                }
            }

            int ack_count = 0;
            unsigned int *acks = NULL;
            server.reliability_system().GetAcks(&acks, ack_count);
            require((ack_count == 0 || (ack_count != 0 && acks)));
            for (int i = 0; i < ack_count; ++i) {
                unsigned int ack = acks[i];
                if (ack < packet_count) {
                    require(!serverAckedPackets[ack]);
                    require((ack & 1) == 0);
                    serverAckedPackets[ack] = true;
                }
            }

            unsigned int clientAckCount = 0;
            unsigned int serverAckCount = 0;
            for (unsigned int i = 0; i < packet_count; ++i) {
                if ((i & 1) != 0) {
                    require(!clientAckedPackets[i]);
                    require(!serverAckedPackets[i]);
                }
                if (clientAckedPackets[i]) {
                    clientAckCount++;
                }
                if (serverAckedPackets[i]) {
                    serverAckCount++;
                }
            }
            all_packets_acked = clientAckCount == packet_count / 2 && serverAckCount == packet_count / 2;

            server.Update(dt_s);

            std::this_thread::sleep_for(std::chrono::milliseconds(int(dt_s * 1000)));
        }
        require(client.connected());
        require(server.connected());
    }
    { // Test sequence wrap around
        const int server_port = 30000;
        const int client_port = 30001;
        const int protocol_id = 0x11112222;
        const float dt_s = 0.05f;
        const float timeout_s = 1000;
        const unsigned int packet_count = 256;
        const unsigned int max_sequence = 31; // [0,31]

        ReliableUDPConnection client(protocol_id, timeout_s, max_sequence);
        ReliableUDPConnection server(protocol_id, timeout_s, max_sequence);

        require_nothrow(client.Start(client_port));
        require_nothrow(server.Start(server_port));

        client.Connect(Address(127, 0, 0, 1, server_port));
        server.Listen();

        unsigned int clientAckCount[max_sequence + 1];
        unsigned int serverAckCount[max_sequence + 1];
        for (unsigned int i = 0; i <= max_sequence; ++i) {
            clientAckCount[i] = 0;
            serverAckCount[i] = 0;
        }

        bool all_packets_acked = false;

        while (true) {
            if (!client.connecting() && client.connect_failed())
                break;

            if (all_packets_acked)
                break;

            unsigned char snd_packet[32];
            for (unsigned int i = 0; i < sizeof(snd_packet); ++i) {
                snd_packet[i] = (unsigned char)i;
            }
            server.SendPacket(snd_packet, sizeof(snd_packet));
            client.SendPacket(snd_packet, sizeof(snd_packet));

            while (true) {
                unsigned char rcv_packet[32];
                const int bytes_read = client.ReceivePacket(rcv_packet, sizeof(rcv_packet));
                if (bytes_read == 0) {
                    break;
                }
                require(bytes_read == sizeof(rcv_packet));
                for (unsigned int i = 0; i < sizeof(rcv_packet); ++i) {
                    require(rcv_packet[i] == (unsigned char)i);
                }
            }

            while (true) {
                unsigned char packet[32];
                int bytes_read = server.ReceivePacket(packet, sizeof(packet));
                if (bytes_read == 0) {
                    break;
                }
                require(bytes_read == sizeof(packet));
                for (unsigned int i = 0; i < sizeof(packet); ++i) {
                    // require(packet[i] == (unsigned char)i);
                }
            }

            int ack_count = 0;
            unsigned int *acks = NULL;
            client.reliability_system().GetAcks(&acks, ack_count);
            require((ack_count == 0 || (ack_count != 0 && acks)));
            for (int i = 0; i < ack_count; ++i) {
                unsigned int ack = acks[i];
                require(ack <= max_sequence);
                clientAckCount[ack] += 1;
            }

            server.reliability_system().GetAcks(&acks, ack_count);
            require((ack_count == 0 || (ack_count != 0 && acks)));
            for (int i = 0; i < ack_count; ++i) {
                unsigned int ack = acks[i];
                require(ack <= max_sequence);
                serverAckCount[ack]++;
            }

            unsigned int totalClientAcks = 0;
            unsigned int totalServerAcks = 0;
            for (unsigned int i = 0; i <= max_sequence; ++i) {
                totalClientAcks += clientAckCount[i];
                totalServerAcks += serverAckCount[i];
            }
            all_packets_acked = totalClientAcks >= packet_count && totalServerAcks >= packet_count;

            client.Update(dt_s);
            server.Update(dt_s);

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        require(client.connected());
        require(server.connected());
    }

    printf("OK\n");
}
