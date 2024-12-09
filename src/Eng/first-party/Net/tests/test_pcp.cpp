#include "test_common.h"

#include <chrono>
#include <thread>

#include "../NAT_PCP.h"

void test_pcp() {
    using namespace Net;

    printf("Test pcp                | ");

    { // Request read/write
        char buf[128];

        PCPNonce nonce;
        GenPCPNonce(&nonce, sizeof(PCPNonce));

        Address client_address(127, 1, 2, 3, 0);
        Address external_address(77, 4, 5, 6, 0);
        Address remote_address(196, 4, 5, 6, 0);

        {
            PCPRequest req;
            req.MakeAnnounceRequest(client_address);

            int sz = req.Write(buf, sizeof(buf));
            require(sz != -1);

            PCPRequest req1;

            require(req1.Read(buf, sz));
            require(req1.opcode() == ePCPOpCode::Announce);
            require(req1.lifetime() == 0);
            require(req1.client_address().address() == client_address.address());
        }
        {
            PCPRequest req;
            req.MakeMapRequest(ePCPProto::UDP, 12345, 17891, 7200, client_address, nonce);

            int sz = req.Write(buf, sizeof(buf));
            require(sz != -1);

            PCPRequest req1;

            require(req1.Read(buf, sz));
            require(req1.opcode() == ePCPOpCode::Map);
            require(req1.internal_port() == 12345);
            require(req1.external_port() == 17891);
            require(req1.lifetime() == 7200);
            require(req1.client_address().address() == client_address.address());
            require(req1.nonce() == nonce);
            require(req1.proto() == ePCPProto::UDP);
            require(req1.external_address() == Address());
        }
        {
            PCPRequest req;
            req.MakePeerRequest(ePCPProto::UDP, 12345, 17891, 7200, external_address, 55765, remote_address, nonce);

            int sz = req.Write(buf, sizeof(buf));
            require(sz != -1);

            PCPRequest req1;

            require(req1.Read(buf, sz));
            require(req1.opcode() == ePCPOpCode::Peer);
            require(req1.internal_port() == 12345);
            require(req1.external_port() == 17891);
            require(req1.lifetime() == 7200);
            require(req1.external_address() == external_address);
            require(req1.remote_port() == 55765);
            require(req1.proto() == ePCPProto::UDP);
            require(req1.remote_address() == remote_address);
            require(req1.nonce() == nonce);
        }
    }
    { // Response read/write
        char buf[128];

        PCPNonce nonce;
        GenPCPNonce(&nonce, sizeof(PCPNonce));

        Address external_address(77, 78, 79, 3, 0), remote_address(111, 12, 14, 3, 0);

        {
            PCPResponse resp;
            resp.MakeAnnounceResponse(ePCPResCode::Success);

            int sz = resp.Write(buf, sizeof(buf));
            require(sz != -1);

            PCPResponse resp1;

            require(resp1.Read(buf, sz));
            require(resp1.opcode() == ePCPOpCode::Announce);
            require(resp1.res_code() == ePCPResCode::Success);
            require(resp1.lifetime() == 0);
        }
        {
            PCPResponse resp;
            resp.MakeMapResponse(ePCPProto::UDP, ePCPResCode::Success, 12345, 17891, 7200, 120, external_address,
                                 nonce);

            int sz = resp.Write(buf, sizeof(buf));
            require(sz != -1);

            PCPResponse resp1;

            require(resp1.Read(buf, sz));
            require(resp1.opcode() == ePCPOpCode::Map);
            require(resp1.res_code() == ePCPResCode::Success);
            require(resp1.lifetime() == 7200);
            require(resp1.time() == 120);
            require(resp1.external_address().address() == external_address.address());
            require(resp1.nonce() == nonce);
        }
        {
            PCPResponse resp;
            resp.MakePeerResponse(ePCPProto::UDP, ePCPResCode::Success, 12345, 17891, 7200, 120, external_address,
                                  61478, remote_address, nonce);

            int sz = resp.Write(buf, sizeof(buf));
            require(sz != -1);

            PCPResponse resp1;

            require(resp1.Read(buf, sz));
            require(resp1.opcode() == ePCPOpCode::Peer);
            require(resp1.res_code() == ePCPResCode::Success);
            require(resp1.lifetime() == 7200);
            require(resp1.time() == 120);
            require(resp1.external_address().address() == external_address.address());
            require(resp1.nonce() == nonce);
        }
    }
    { // Retransmit
        char buf[128];

        UDPSocket fake_pcp_srv;
        fake_pcp_srv.Open(30001);

        Address fake_pcp_address(127, 0, 0, 1, 30001);

        PCPSession ses(ePCPProto::UDP, fake_pcp_address, 30000, 30001, 7200);
        require(ses.state() == PCPSession::eState::RequestMapping);

        ses.Update(16);

        Address sender;
        require(fake_pcp_srv.Receive(sender, buf, sizeof(buf)));

        ses.Update(16);
        require(!fake_pcp_srv.Receive(sender, buf, sizeof(buf)));

        ses.Update(4000);
        require(fake_pcp_srv.Receive(sender, buf, sizeof(buf)));

        ses.Update(16);
        require(!fake_pcp_srv.Receive(sender, buf, sizeof(buf)));
    }
    {
        char recv_buf[128];
        int size = 0;

        UDPSocket pcp_server;
        pcp_server.Open(30001);

        Address pcp_server_address(127, 0, 0, 1, 30001);
        Address external_address(77, 22, 44, 55, 0);

        PCPSession s1(ePCPProto::UDP, pcp_server_address, 30000, 30005);

        unsigned int time_acc = 0;

        while (time_acc < 1000 && s1.state() == PCPSession::eState::RequestMapping) {
            Address sender;
            PCPRequest req;
            if ((size = pcp_server.Receive(sender, recv_buf, sizeof(recv_buf))) && req.Read(recv_buf, size)) {
                PCPResponse resp;
                resp.MakeMapResponse(ePCPProto::UDP, ePCPResCode::Success, 30000, 30001, 7200, 100, external_address,
                                     req.nonce());

                size = resp.Write(recv_buf, sizeof(recv_buf));
                pcp_server.Send(s1.local_addr(), &resp, size);
            }

            s1.Update(16);

            time_acc += 16;
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }

        require(time_acc < 1000);
        require(s1.state() == PCPSession::eState::IdleMapped);
    }

    printf("OK\n");
}
