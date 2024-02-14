#include "test_common.h"

#include <chrono>
#include <thread>

#include "../NAT_PMP.h"

void test_pmp() {

    { // Should retrieve external ip first
        Net::PMPSession s1(Net::ePMPProto::UDP, Net::Address(127, 0, 0, 1, 5351), 30000, 30005);
        assert(s1.state() == Net::PMPSession::eState::RetrieveExternalIP);
    }

    { // Retrieve external ip timeout
        Net::PMPSession s1(Net::ePMPProto::UDP, Net::Address(127, 0, 0, 1, 5351), 30000, 30005);
        s1.Update(64000);
        assert(s1.state() == Net::PMPSession::eState::IdleUnsupported);
        // should be idle forever
        s1.Update(100000000);
        assert(s1.state() == Net::PMPSession::eState::IdleUnsupported);
    }

    { // Retrieve external ip unsupported version
        char recv_buf[128];

        Net::UDPSocket fake_gateway;
        Net::Address fake_gateway_address(127, 0, 0, 1, 30001);
        fake_gateway.Open(fake_gateway_address.port());

        Net::PMPSession s1(Net::ePMPProto::UDP, fake_gateway_address, 30000, 30005);

        unsigned int time_acc = 0;

        while (time_acc < 1000 && s1.state() == Net::PMPSession::eState::RetrieveExternalIP) {
            Net::Address sender;
            if (fake_gateway.Receive(sender, recv_buf, sizeof(recv_buf)) == sizeof(Net::PMPExternalIPRequest) &&
                sender == Net::Address(127, 0, 0, 1, s1.local_addr().port())) {
                Net::PMPUnsupportedVersionResponse resp(Net::ePMPOpCode::ExternalIPRequest);
                resp.set_res_code(Net::ePMPResCode::Refused);
                fake_gateway.Send(s1.local_addr(), &resp, sizeof(Net::PMPUnsupportedVersionResponse));
            }

            s1.Update(16);

            time_acc += 16;
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }

        assert(time_acc < 1000 + 16);
        assert(s1.state() == Net::PMPSession::eState::IdleRetrieveExternalIPError);
    }

    { // Retrieve external ip error
        char recv_buf[128];

        Net::UDPSocket fake_gateway;
        fake_gateway.Open(30001);

        Net::Address fake_gateway_address(127, 0, 0, 1, 30001);

        Net::PMPSession s1(Net::ePMPProto::UDP, fake_gateway_address, 30000, 30005);

        unsigned int time_acc = 0;

        while (time_acc < 1000 && s1.state() == Net::PMPSession::eState::RetrieveExternalIP) {
            Net::Address sender;
            if (fake_gateway.Receive(sender, recv_buf, sizeof(recv_buf)) == sizeof(Net::PMPExternalIPRequest) &&
                sender == Net::Address(127, 0, 0, 1, s1.local_addr().port())) {
                auto *req = (Net::PMPExternalIPRequest *)recv_buf;
                assert(req->vers() == 0);
                assert(req->op() == 0);

                Net::PMPExternalIPResponse resp;
                resp.set_ip(12345);
                resp.set_res_code(1);
                resp.set_time(1000);
                fake_gateway.Send(s1.local_addr(), &resp, sizeof(Net::PMPExternalIPResponse));
            }

            s1.Update(16);

            time_acc += 16;
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }

        assert(time_acc < 1000);
        assert(s1.state() == Net::PMPSession::eState::IdleRetrieveExternalIPError);
    }

    { // Create port mapping timeout
        char recv_buf[128];

        Net::UDPSocket fake_gateway;
        fake_gateway.Open(30001);

        Net::Address fake_gateway_address(127, 0, 0, 1, 30001);

        Net::PMPSession s1(Net::ePMPProto::UDP, fake_gateway_address, 30000, 30005);

        unsigned int time_acc = 0;

        while (time_acc < 1000 && s1.state() == Net::PMPSession::eState::RetrieveExternalIP) {
            Net::Address sender;
            if (fake_gateway.Receive(sender, recv_buf, sizeof(recv_buf)) == sizeof(Net::PMPExternalIPRequest) &&
                sender == Net::Address(127, 0, 0, 1, s1.local_addr().port())) {
                auto *req = (Net::PMPExternalIPRequest *)recv_buf;
                assert(req->vers() == 0);
                assert(req->op() == 0);

                Net::PMPExternalIPResponse resp;
                resp.set_ip(12345);
                resp.set_res_code(0);
                resp.set_time(1000);
                fake_gateway.Send(s1.local_addr(), &resp, sizeof(Net::PMPExternalIPResponse));
            }

            s1.Update(16);

            time_acc += 16;
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }

        assert(time_acc < 1000);
        assert(s1.external_ip() == Net::Address(12345, 0));
        assert(s1.state() == Net::PMPSession::eState::CreatePortMapping);
        assert(s1.time() == 1000);

        s1.Update(64000);

        assert(s1.state() == Net::PMPSession::eState::IdleUnsupported);
    }

    { // Create port mapping error
        char recv_buf[128];

        Net::UDPSocket fake_gateway;
        fake_gateway.Open(30001);

        Net::Address fake_gateway_address(127, 0, 0, 1, 30001);

        Net::PMPSession s1(Net::ePMPProto::UDP, fake_gateway_address, 30000, 30005);

        unsigned int time_acc = 0;

        while (time_acc < 1000 && s1.state() == Net::PMPSession::eState::RetrieveExternalIP) {
            Net::Address sender;
            if (fake_gateway.Receive(sender, recv_buf, sizeof(recv_buf)) == sizeof(Net::PMPExternalIPRequest) &&
                sender == Net::Address(127, 0, 0, 1, s1.local_addr().port())) {
                auto *req = (Net::PMPExternalIPRequest *)recv_buf;
                assert(req->vers() == 0);
                assert(req->op() == 0);

                Net::PMPExternalIPResponse resp;
                resp.set_ip(12345);
                resp.set_res_code(0);
                resp.set_time(1000);
                fake_gateway.Send(s1.local_addr(), &resp, sizeof(Net::PMPExternalIPResponse));
            }

            s1.Update(16);

            time_acc += 16;
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }

        time_acc = 0;

        while (time_acc < 1000 && s1.state() == Net::PMPSession::eState::CreatePortMapping) {
            Net::Address sender;
            if (fake_gateway.Receive(sender, recv_buf, sizeof(recv_buf)) == sizeof(Net::PMPMappingRequest) &&
                sender == Net::Address(127, 0, 0, 1, s1.local_addr().port())) {
                auto *req = (Net::PMPMappingRequest *)recv_buf;
                assert(req->vers() == 0);
                assert(req->op() == 1);
                assert(req->internal_port() == 30000);
                assert(req->external_port() == 30005);
                assert(req->lifetime() == 7200);

                Net::PMPMappingResponse resp(Net::ePMPProto::UDP);
                resp.set_res_code(Net::ePMPResCode::Failure);

                fake_gateway.Send(s1.local_addr(), &resp, sizeof(Net::PMPMappingResponse));
            }

            s1.Update(16);

            time_acc += 16;
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }

        assert(time_acc < 1000);
        assert(s1.state() == Net::PMPSession::eState::IdleCreatePortMappingError);
    }

    { // Create port mapping success
        char recv_buf[128];

        Net::UDPSocket fake_gateway;
        fake_gateway.Open(30001);

        Net::Address fake_gateway_address(127, 0, 0, 1, 30001);

        Net::PMPSession s1(Net::ePMPProto::UDP, fake_gateway_address, 30000, 30005);

        unsigned int time_acc = 0;

        while (time_acc < 1000 && s1.state() == Net::PMPSession::eState::RetrieveExternalIP) {
            Net::Address sender;
            if (fake_gateway.Receive(sender, recv_buf, sizeof(recv_buf)) == sizeof(Net::PMPExternalIPRequest) &&
                sender == Net::Address(127, 0, 0, 1, s1.local_addr().port())) {
                auto *req = (Net::PMPExternalIPRequest *)recv_buf;
                assert(req->vers() == 0);
                assert(req->op() == 0);

                Net::PMPExternalIPResponse resp;
                resp.set_ip(12345);
                resp.set_res_code(0);
                resp.set_time(1000);
                fake_gateway.Send(s1.local_addr(), &resp, sizeof(Net::PMPExternalIPResponse));
            }

            s1.Update(16);

            time_acc += 16;
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }

        time_acc = 0;

        while (time_acc < 1000 && s1.state() == Net::PMPSession::eState::CreatePortMapping) {
            Net::Address sender;
            if (fake_gateway.Receive(sender, recv_buf, sizeof(recv_buf)) == sizeof(Net::PMPMappingRequest) &&
                sender == Net::Address(127, 0, 0, 1, s1.local_addr().port())) {
                auto *req = (Net::PMPMappingRequest *)recv_buf;
                assert(req->vers() == 0);
                assert(req->op() == 1);
                assert(req->internal_port() == 30000);
                assert(req->external_port() == 30005);
                assert(req->lifetime() == 7200);

                Net::PMPMappingResponse resp(Net::ePMPProto::UDP);
                resp.set_res_code(Net::ePMPResCode::Success);
                resp.set_internal_port(30000);
                resp.set_external_port(30005);
                resp.set_time(1000);
                resp.set_lifetime(7200);

                fake_gateway.Send(s1.local_addr(), &resp, sizeof(Net::PMPMappingResponse));
            }

            s1.Update(16);

            time_acc += 16;
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }

        assert(time_acc < 1000);
        assert(s1.state() == Net::PMPSession::eState::IdleMapped);

        // simulate nat restart

        time_acc = 0;

        while (time_acc < 1000 && s1.state() == Net::PMPSession::eState::IdleMapped) {
            Net::PMPExternalIPResponse resp;
            resp.set_ip(12345);
            resp.set_res_code(0);
            resp.set_time(0);

            fake_gateway.Send(Net::Address(224, 0, 0, 1, 5350), &resp, sizeof(Net::PMPExternalIPResponse));

            s1.Update(16);

            time_acc += 16;
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }

        assert(time_acc < 1000);
        assert(s1.state() == Net::PMPSession::eState::CreatePortMapping);
    }
}
