#pragma once

#include <vector>
#include "PacketQueue.h"

namespace Net {
class ReliabilitySystem {
public:
    ReliabilitySystem(unsigned int max_sequence = 0xFFFFFFFF);

    void Reset();

    void PacketSent(void *data, int size);
    void PacketReceived(unsigned int sequence, int size);

    unsigned int GenerateAckBits() {
        return generate_ack_bits(remote_sequence(), received_queue_, max_sequence_);
    }

    void ProcessAck(unsigned int ack, unsigned int ack_bits) {
        process_ack(ack, ack_bits, pending_ack_queue_, acked_queue_, acks_, acked_packets_, rtt_, max_sequence_);
    }

    void Update(float dt_s);

    unsigned int max_sequence() const {
        return max_sequence_;
    }
    unsigned int local_sequence() const {
        return local_sequence_;
    }
    unsigned int remote_sequence() const {
        return remote_sequence_;
    }

    float rtt() const {
        return rtt_;
    }
    unsigned int sent_packets() const {
        return sent_packets_;
    }
    unsigned int recv_packets() const {
        return recv_packets_;
    }
    unsigned int lost_packets() const {
        return lost_packets_;
    }
    unsigned int acked_packets() const {
        return acked_packets_;
    }
    float sent_bandwidth() const {
        return sent_bandwidth_;
    }
    float acked_bandwidth() const {
        return acked_bandwidth_;
    }

    void GetAcks(unsigned int **acks, int &count);

    static int bit_index_for_sequence(unsigned int sequence, unsigned int ack, unsigned int max_sequence);
    static unsigned int generate_ack_bits(unsigned int ack,
                                          const PacketQueue &received_queue,
                                          unsigned int max_sequence);
    static void process_ack(unsigned int ack,
                            unsigned int ack_bits,
                            PacketQueue &pending_ack_queue,
                            PacketQueue &acked_queue,
                            std::vector<unsigned int> &acks,
                            unsigned int &acked_packets,
                            float &rtt,
                            unsigned int max_sequence);

protected:
    void AdvanceQueueTime(float dt_s);
    void UpdateQueues();
    void UpdateStats();

private:
    unsigned int max_sequence_, local_sequence_, remote_sequence_;

    unsigned int sent_packets_, recv_packets_, lost_packets_, acked_packets_;

    float sent_bandwidth_, acked_bandwidth_, rtt_, rtt_maximum_;

    std::vector<unsigned int> acks_;

    PacketQueue sent_queue_, pending_ack_queue_, received_queue_, acked_queue_;
};
}

