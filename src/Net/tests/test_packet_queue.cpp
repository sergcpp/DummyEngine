#include "test_common.h"

#include "../PacketQueue.h"

class PacketQueueTestsFixture {
public:
    const unsigned int  maximum_sequence;
    Net::PacketQueue    packet_queue;

    PacketQueueTestsFixture() : maximum_sequence(255) { }
};

void test_packet_queue() {

    {   // PacketQueue insert back
        PacketQueueTestsFixture f;

        for (int i = 0; i < 100; i++) {
            Net::PacketData data(0, 0, 0);
            data.sequence = (unsigned)i;
            f.packet_queue.insert_sorted(data, f.maximum_sequence);
            assert(f.packet_queue.verify_sorted(f.maximum_sequence));
        }
    }

    {   // PacketQueue insert front
        PacketQueueTestsFixture f;

        for (int i = 100; i < 0; i++) {
            Net::PacketData data(0, 0, 0);
            data.sequence = (unsigned)i;
            f.packet_queue.insert_sorted(data, f.maximum_sequence);
            assert(f.packet_queue.verify_sorted(f.maximum_sequence));
        }
    }

    {   // PacketQueue insert random
        PacketQueueTestsFixture f;

        for (int i = 100; i < 0; i++) {
            Net::PacketData data(0, 0, 0);
            data.sequence = (unsigned)(rand() & 0xFF);
            f.packet_queue.insert_sorted(data, f.maximum_sequence);
            assert(f.packet_queue.verify_sorted(f.maximum_sequence));
        }
    }

    {   // PacketQueue insert wrap around
        PacketQueueTestsFixture f;

        for (int i = 200; i <= 255; i++) {
            Net::PacketData data(0, 0, 0);
            data.sequence = (unsigned)i;
            f.packet_queue.insert_sorted(data, f.maximum_sequence);
            assert(f.packet_queue.verify_sorted(f.maximum_sequence));
        }
        for (int i = 0; i <= 50; i++) {
            Net::PacketData data(0, 0, 0);
            data.sequence = (unsigned)i;
            f.packet_queue.insert_sorted(data, f.maximum_sequence);
            assert(f.packet_queue.verify_sorted(f.maximum_sequence));
        }
    }
}
