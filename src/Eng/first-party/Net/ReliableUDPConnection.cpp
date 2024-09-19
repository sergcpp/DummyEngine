#include "ReliableUDPConnection.h"

#include <cstring>

Net::ReliableUDPConnection::ReliableUDPConnection(unsigned int protocol_id, float timeout, unsigned int max_sequence)
        : UDPConnection(protocol_id, timeout), packet_loss_mask_(0), reliability_system_(max_sequence) {
    ClearData();
}

Net::ReliableUDPConnection::~ReliableUDPConnection() {
    if (running()) {
        Stop();
    }
}

bool Net::ReliableUDPConnection::SendPacket(const unsigned char data[], int size) {
    if (reliability_system_.local_sequence() & packet_loss_mask_) {
        reliability_system_.PacketSent(nullptr, size);
        return true;
    }
    const int header_size = 12;
    unsigned char packet[MAX_PACKET_SIZE];

    unsigned int seq = reliability_system_.local_sequence();
    unsigned int ack = reliability_system_.remote_sequence();
    unsigned int ack_bits = reliability_system_.GenerateAckBits();

    WriteHeader(packet, seq, ack, ack_bits);
    memcpy(packet + header_size, data, (size_t) size);
    if (!UDPConnection::SendPacket(packet, size + header_size)) {
        return false;
    }
    reliability_system_.PacketSent((void *) data, size);
    return true;
}

int Net::ReliableUDPConnection::ReceivePacket(unsigned char data[], int size) {
    const int header_size = 12;
    if (size <= header_size) {
        return 0;
    }
    unsigned char packet[MAX_PACKET_SIZE];
    int received_bytes = UDPConnection::ReceivePacket(packet, size + header_size);
    if (received_bytes < header_size) {
        return 0;
    }
    unsigned int packet_sequence = 0,
            packet_ack = 0,
            packet_ack_bits = 0;
    ReadHeader(packet, packet_sequence, packet_ack, packet_ack_bits);
    reliability_system_.PacketReceived(packet_sequence, received_bytes - header_size);
    reliability_system_.ProcessAck(packet_ack, packet_ack_bits);
    memcpy(data, packet + header_size, (size_t) received_bytes - header_size);

    return received_bytes - header_size;
}

void Net::ReliableUDPConnection::Update(float dt_s) {
    UDPConnection::Update(dt_s);
    reliability_system_.Update(dt_s);
}

void Net::ReliableUDPConnection::WriteInteger(unsigned char *data, unsigned int value) {
    data[0] = (unsigned char) (value >> 24u);
    data[1] = (unsigned char) ((value >> 16u) & 0xFFu);
    data[2] = (unsigned char) ((value >> 8u) & 0xFFu);
    data[3] = (unsigned char) (value & 0xFFu);
}

void Net::ReliableUDPConnection::WriteHeader(
        unsigned char *header, unsigned int sequence, unsigned int ack, unsigned int ack_bits) {
    WriteInteger(header, sequence);
    WriteInteger(header + 4, ack);
    WriteInteger(header + 8, ack_bits);
}

void Net::ReliableUDPConnection::ReadInteger(const unsigned char *data, unsigned int &value) {
    value = (((unsigned int) data[0] << 24u) | ((unsigned int) data[1] << 16u) |
             ((unsigned int) data[2] << 8u) | ((unsigned int) data[3]));
}

void Net::ReliableUDPConnection::ReadHeader(
        const unsigned char *header, unsigned int &sequence, unsigned int &ack, unsigned int &ack_bits) {
    ReadInteger(header, sequence);
    ReadInteger(header + 4, ack);
    ReadInteger(header + 8, ack_bits);
}