#pragma once

#include "UDPConnection.h"
#include "ReliabilitySystem.h"

namespace Net {
    class ReliableUDPConnection : public UDPConnection {
    public:
        ReliableUDPConnection(unsigned int protocol_id, float timeout, unsigned int max_sequence = 0xFFFFFFFF);
        ~ReliableUDPConnection();

        void set_packet_loss_mask(unsigned int mask) {
            packet_loss_mask_ = mask;
        }

        ReliabilitySystem &reliability_system() {
            return reliability_system_;
        }

        virtual bool SendPacket(const unsigned char data[], int size);
        virtual int ReceivePacket(unsigned char data[], int size);

        void Update(float dt_s);

    protected:
        static void WriteInteger(unsigned char *data, unsigned int value);
        static void WriteHeader(unsigned char *header, unsigned int sequence, unsigned int ack, unsigned int ack_bits);
        static void ReadInteger(const unsigned char *data, unsigned int &value);
        static void ReadHeader(
                const unsigned char *header, unsigned int &sequence, unsigned int &ack, unsigned int &ack_bits);

    private:
        void ClearData() {
            reliability_system_.Reset();
        }

        unsigned int packet_loss_mask_;
        ReliabilitySystem reliability_system_;
    };
}
