#include "test_common.h"

#include <chrono>
#include <thread>

#include "../NAT_PMP.h"

void test_pmp() {
    printf("Test pmp                | ");

    using namespace Net;

    { // Should retrieve external ip first
        PMPSession s1(ePMPProto::UDP, Address(127, 0, 0, 1, 5351), 30000, 30005);
        require(s1.state() == PMPSession::eState::RetrieveExternalIP);
    }
    { // Retrieve external ip timeout
        PMPSession s1(ePMPProto::UDP, Address(127, 0, 0, 1, 5351), 30000, 30005);
        s1.Update(64000);
        require(s1.state() == PMPSession::eState::IdleUnsupported);
        // should be idle forever
        s1.Update(100000000);
        require(s1.state() == PMPSession::eState::IdleUnsupported);
    }
    { // Retrieve external ip unsupported version
        char recv_buf[128];

        UDPSocket fake_gateway;
        Address fake_gateway_address(127, 0, 0, 1, 30001);
        fake_gateway.Open(fake_gateway_address.port());

        PMPSession s1(ePMPProto::UDP, fake_gateway_address, 30000, 30005);

        unsigned int time_acc = 0;

        while (time_acc < 1000 && s1.state() == PMPSession::eState::RetrieveExternalIP) {
            Address sender;
            if (fake_gateway.Receive(sender, recv_buf, sizeof(recv_buf)) == sizeof(PMPExternalIPRequest) &&
                sender == Address(127, 0, 0, 1, s1.local_addr().port())) {
                PMPUnsupportedVersionResponse resp(ePMPOpCode::ExternalIPRequest);
                resp.set_res_code(ePMPResCode::Refused);
                fake_gateway.Send(s1.local_addr(), &resp, sizeof(PMPUnsupportedVersionResponse));
            }

            s1.Update(16);

            time_acc += 16;
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }

        require(time_acc < 1000 + 16);
        require(s1.state() == PMPSession::eState::IdleRetrieveExternalIPError);
    }
    { // Retrieve external ip error
        char recv_buf[128];

        UDPSocket fake_gateway;
        fake_gateway.Open(30001);

        Address fake_gateway_address(127, 0, 0, 1, 30001);

        PMPSession s1(ePMPProto::UDP, fake_gateway_address, 30000, 30005);

        unsigned int time_acc = 0;

        while (time_acc < 1000 && s1.state() == PMPSession::eState::RetrieveExternalIP) {
            Address sender;
            if (fake_gateway.Receive(sender, recv_buf, sizeof(recv_buf)) == sizeof(PMPExternalIPRequest) &&
                sender == Address(127, 0, 0, 1, s1.local_addr().port())) {
                auto *req = (PMPExternalIPRequest *)recv_buf;
                require(req->vers() == 0);
                require(req->op() == 0);

                PMPExternalIPResponse resp;
                resp.set_ip(12345);
                resp.set_res_code(1);
                resp.set_time(1000);
                fake_gateway.Send(s1.local_addr(), &resp, sizeof(PMPExternalIPResponse));
            }

            s1.Update(16);

            time_acc += 16;
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }

        require(time_acc < 1000);
        require(s1.state() == PMPSession::eState::IdleRetrieveExternalIPError);
    }
    { // Create port mapping timeout
        char recv_buf[128];

        UDPSocket fake_gateway;
        fake_gateway.Open(30001);

        Address fake_gateway_address(127, 0, 0, 1, 30001);

        PMPSession s1(ePMPProto::UDP, fake_gateway_address, 30000, 30005);

        unsigned int time_acc = 0;

        while (time_acc < 1000 && s1.state() == PMPSession::eState::RetrieveExternalIP) {
            Address sender;
            if (fake_gateway.Receive(sender, recv_buf, sizeof(recv_buf)) == sizeof(PMPExternalIPRequest) &&
                sender == Address(127, 0, 0, 1, s1.local_addr().port())) {
                auto *req = (PMPExternalIPRequest *)recv_buf;
                require(req->vers() == 0);
                require(req->op() == 0);

                PMPExternalIPResponse resp;
                resp.set_ip(12345);
                resp.set_res_code(0);
                resp.set_time(1000);
                fake_gateway.Send(s1.local_addr(), &resp, sizeof(PMPExternalIPResponse));
            }

            s1.Update(16);

            time_acc += 16;
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }

        require(time_acc < 1000);
        require(s1.external_ip() == Address(12345, 0));
        require(s1.state() == PMPSession::eState::CreatePortMapping);
        require(s1.time() == 1000);

        s1.Update(64000);

        require(s1.state() == PMPSession::eState::IdleUnsupported);
    }
    { // Create port mapping error
        char recv_buf[128];

        UDPSocket fake_gateway;
        fake_gateway.Open(30001);

        Address fake_gateway_address(127, 0, 0, 1, 30001);

        PMPSession s1(ePMPProto::UDP, fake_gateway_address, 30000, 30005);

        unsigned int time_acc = 0;

        while (time_acc < 1000 && s1.state() == PMPSession::eState::RetrieveExternalIP) {
            Address sender;
            if (fake_gateway.Receive(sender, recv_buf, sizeof(recv_buf)) == sizeof(PMPExternalIPRequest) &&
                sender == Address(127, 0, 0, 1, s1.local_addr().port())) {
                auto *req = (PMPExternalIPRequest *)recv_buf;
                require(req->vers() == 0);
                require(req->op() == 0);

                PMPExternalIPResponse resp;
                resp.set_ip(12345);
                resp.set_res_code(0);
                resp.set_time(1000);
                fake_gateway.Send(s1.local_addr(), &resp, sizeof(PMPExternalIPResponse));
            }

            s1.Update(16);

            time_acc += 16;
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }

        time_acc = 0;

        while (time_acc < 1000 && s1.state() == PMPSession::eState::CreatePortMapping) {
            Address sender;
            if (fake_gateway.Receive(sender, recv_buf, sizeof(recv_buf)) == sizeof(PMPMappingRequest) &&
                sender == Address(127, 0, 0, 1, s1.local_addr().port())) {
                auto *req = (PMPMappingRequest *)recv_buf;
                require(req->vers() == 0);
                require(req->op() == 1);
                require(req->internal_port() == 30000);
                require(req->external_port() == 30005);
                require(req->lifetime() == 7200);

                PMPMappingResponse resp(ePMPProto::UDP);
                resp.set_res_code(ePMPResCode::Failure);

                fake_gateway.Send(s1.local_addr(), &resp, sizeof(PMPMappingResponse));
            }

            s1.Update(16);

            time_acc += 16;
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }

        require(time_acc < 1000);
        require(s1.state() == PMPSession::eState::IdleCreatePortMappingError);
    }
    { // Create port mapping success
        char recv_buf[128];

        UDPSocket fake_gateway;
        fake_gateway.Open(30001);

        Address fake_gateway_address(127, 0, 0, 1, 30001);

        PMPSession s1(ePMPProto::UDP, fake_gateway_address, 30000, 30005);

        unsigned int time_acc = 0;

        while (time_acc < 1000 && s1.state() == PMPSession::eState::RetrieveExternalIP) {
            Address sender;
            if (fake_gateway.Receive(sender, recv_buf, sizeof(recv_buf)) == sizeof(PMPExternalIPRequest) &&
                sender == Address(127, 0, 0, 1, s1.local_addr().port())) {
                auto *req = (PMPExternalIPRequest *)recv_buf;
                require(req->vers() == 0);
                require(req->op() == 0);

                PMPExternalIPResponse resp;
                resp.set_ip(12345);
                resp.set_res_code(0);
                resp.set_time(1000);
                fake_gateway.Send(s1.local_addr(), &resp, sizeof(PMPExternalIPResponse));
            }

            s1.Update(16);

            time_acc += 16;
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }

        time_acc = 0;

        while (time_acc < 1000 && s1.state() == PMPSession::eState::CreatePortMapping) {
            Address sender;
            if (fake_gateway.Receive(sender, recv_buf, sizeof(recv_buf)) == sizeof(PMPMappingRequest) &&
                sender == Address(127, 0, 0, 1, s1.local_addr().port())) {
                auto *req = (PMPMappingRequest *)recv_buf;
                require(req->vers() == 0);
                require(req->op() == 1);
                require(req->internal_port() == 30000);
                require(req->external_port() == 30005);
                require(req->lifetime() == 7200);

                PMPMappingResponse resp(ePMPProto::UDP);
                resp.set_res_code(ePMPResCode::Success);
                resp.set_internal_port(30000);
                resp.set_external_port(30005);
                resp.set_time(1000);
                resp.set_lifetime(7200);

                fake_gateway.Send(s1.local_addr(), &resp, sizeof(PMPMappingResponse));
            }

            s1.Update(16);

            time_acc += 16;
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }

        require(time_acc < 1000);
        require(s1.state() == PMPSession::eState::IdleMapped);

        // simulate nat restart

        time_acc = 0;

        while (time_acc < 1000 && s1.state() == PMPSession::eState::IdleMapped) {
            PMPExternalIPResponse resp;
            resp.set_ip(12345);
            resp.set_res_code(0);
            resp.set_time(0);

            fake_gateway.Send(Address(224, 0, 0, 1, 5350), &resp, sizeof(PMPExternalIPResponse));

            s1.Update(16);

            time_acc += 16;
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }

        require(time_acc < 1000);
        require(s1.state() == PMPSession::eState::CreatePortMapping);
    }

    printf("OK\n");
}
