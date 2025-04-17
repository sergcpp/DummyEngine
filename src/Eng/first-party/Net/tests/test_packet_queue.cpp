#include "test_common.h"

#include <algorithm>
#include <numeric>
#include <random>

#include "../PacketQueue.h"

class PacketQueueTestsFixture {
  public:
    const unsigned int maximum_sequence;
    Net::PacketQueue packet_queue;

    PacketQueueTestsFixture() : maximum_sequence(255) {}
};

void test_packet_queue() {
    using namespace Net;

    printf("Test packet_queue       | ");

    { // PacketQueue insert back
        PacketQueueTestsFixture f;

        for (int i = 0; i < 100; i++) {
            PacketData data(0, 0, 0);
            data.sequence = i;
            f.packet_queue.insert_sorted(data, f.maximum_sequence);
            require(f.packet_queue.verify_sorted(f.maximum_sequence));
        }
    }
    { // PacketQueue insert front
        PacketQueueTestsFixture f;

        for (int i = 100; i >= 0; --i) {
            PacketData data(0, 0, 0);
            data.sequence = i;
            f.packet_queue.insert_sorted(data, f.maximum_sequence);
            require(f.packet_queue.verify_sorted(f.maximum_sequence));
        }
    }
    { // PacketQueue insert random
        PacketQueueTestsFixture f;

        int sequence[100];
        std::iota(std::begin(sequence), std::end(sequence), 0);

        std::mt19937 rng({});
        std::shuffle(std::begin(sequence), std::end(sequence), rng);

        for (int i = 0; i < 100; i++) {
            PacketData data(0, 0, 0);
            data.sequence = sequence[i];
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
