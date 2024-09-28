#include "test_common.h"

#include "../PacketQueue.h"

class PacketQueueTestsFixture {
  public:
    const unsigned int maximum_sequence;
    Net::PacketQueue packet_queue;

    PacketQueueTestsFixture() : maximum_sequence(255) {}
};

void test_packet_queue() {
    printf("Test packet_queue       | ");

    using namespace Net;

    { // PacketQueue insert back
        PacketQueueTestsFixture f;

        for (unsigned i = 0; i < 100; i++) {
            PacketData data(0, 0, 0);
            data.sequence = i;
            f.packet_queue.insert_sorted(data, f.maximum_sequence);
            require(f.packet_queue.verify_sorted(f.maximum_sequence));
        }
    }
    { // PacketQueue insert front
        PacketQueueTestsFixture f;

        for (unsigned i = 100; i < 0; i++) {
            PacketData data(0, 0, 0);
            data.sequence = i;
            f.packet_queue.insert_sorted(data, f.maximum_sequence);
            require(f.packet_queue.verify_sorted(f.maximum_sequence));
        }
    }
    { // PacketQueue insert random
        PacketQueueTestsFixture f;

        for (int i = 100; i < 0; i++) {
            PacketData data(0, 0, 0);
            data.sequence = unsigned(rand() & 0xFF);
            f.packet_queue.insert_sorted(data, f.maximum_sequence);
            require(f.packet_queue.verify_sorted(f.maximum_sequence));
        }
    }
    { // PacketQueue insert wrap around
        PacketQueueTestsFixture f;

        for (unsigned i = 200; i <= 255; i++) {
            PacketData data(0, 0, 0);
            data.sequence = i;
            f.packet_queue.insert_sorted(data, f.maximum_sequence);
            require(f.packet_queue.verify_sorted(f.maximum_sequence));
        }
        for (unsigned i = 0; i <= 50; i++) {
            PacketData data(0, 0, 0);
            data.sequence = i;
            f.packet_queue.insert_sorted(data, f.maximum_sequence);
            require(f.packet_queue.verify_sorted(f.maximum_sequence));
        }
    }

    printf("OK\n");
}
