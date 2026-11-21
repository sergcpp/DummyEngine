#include "test_common.h"

#include "../VarContainer.h"

void test_var() {
    using namespace Net;

    printf("Test var                | ");

    { // Var test1
        Var<int> v1 = {"V1"};
        Var<int> v2 = {"V2"};

        v1 = 12;
        v2 = 12;

        require(v1 == v2);
        require(v2.val() == 12);
        require(v1.hash() != v2.hash());
    }

    { // Var test2
        Var<int> v1 = {"VAR"};
        Var<int> v2 = {"VAR"};

        v1 = 13;
        v2 = 12;

        require(v1 != v2);
        require(v1.hash() == v2.hash());
    }

    { // Var test3
        struct S1 {
            int a, b, c;
            float f;
        };

        struct S2 {
            float f1, f2;
            double dd;
        };

        Var<S1> v1 = {"VAR"};
        Var<S2> v2 = {"VAR"};

        v1 = {1, 2, 3, 5.13f};
        v2 = {11.1f, 12.2f, 10.0};

        require(v1.a == 1);
        require(v2.dd == 10.0);
    }

    { // VarContainer save/load test
        struct S1 {
            float x;
            short s;
            double d;
        };
        std::vector<unsigned char> pack;
        Var<int> v1 = {"V1", 12};
        Var<int> v2 = {"V2", 13};
        Var<float> v3 = {"V3", 25.251f};
        Var<S1> v4 = {"V4"};
        v4 = {4.5f, 11, 5.6};

        {
            VarContainer cnt;

            cnt.SaveVar(v1);
            cnt.SaveVar(v2);
            cnt.SaveVar(v3);
            cnt.SaveVar(v4);

            require(cnt.size() == 4);
            pack = cnt.Pack();
            require(pack.size() == sizeof(VarContainer::int_type) * 2 + 2 * 4 * sizeof(VarContainer::int_type) +
                                       2 * sizeof(int) + sizeof(float) + sizeof(S1));
        }

        v1 = 11;
        v2 = 14;
        v3 = 15.044f;
        v4 = {-4.5f, -11, -5.6};
        require(v1 == 11);
        require(v2 == 14);
        require(v3 == 15.044f);
        require(v4.d == -5.6);

        VarContainer cnt;
        cnt.UnPack(pack);
        require(cnt.size() == 4);

        require(cnt.LoadVar(v1));
        require(cnt.LoadVar(v2));
        require(cnt.LoadVar(v3));
        require(cnt.LoadVar(v4));

        require(v1 == 12);
        require(v2 == 13);
        require(v3 == 25.251f);
        require(v4.d == 5.6);
    }

    { // VarContainer update test
        std::vector<unsigned char> pack;
        Var<int> v1 = {"V1", 12};
        Var<int> v2 = {"V2", 13};
        Var<float> v3 = {"V3", 25.251f};
        {
            VarContainer cnt;

            cnt.SaveVar(v1);
            cnt.SaveVar(v2);
            cnt.SaveVar(v3);

            v1 = 110;
            v2 = 140;
            v3 = 150.044f;

            cnt.UpdateVar(v1);
            cnt.UpdateVar(v2);
            cnt.UpdateVar(v3);

            v1 = 11;
            v2 = 14;
            v3 = 15.044f;

            cnt.UpdateVar(v1);
            cnt.UpdateVar(v2);
            cnt.UpdateVar(v3);

            pack = cnt.Pack();
        }
        VarContainer cnt;
        cnt.UnPack(pack);
        require(cnt.size() == 3);

        require(cnt.LoadVar(v1));
        require(cnt.LoadVar(v2));
        require(cnt.LoadVar(v3));

        require(v1 == 11);
        require(v2 == 14);
        require(v3 == 15.044f);
    }

    { // VarContainer nested test
        VarContainer cnt;
        Var<VarContainer> c1("CONT1"), c2("CONT2");
        struct S1 {
            float x;
            short s;
            double d;
        };
        Var<int> v1 = {"V1", 12};
        Var<int> v2 = {"V2", 14};
        Var<float> v3 = {"V3", 25.251f};
        Var<S1> v4 = {"V4"};
        v4 = {4.5f, 11, 5.6};

        c1.SaveVar(v1);
        c1.SaveVar(v2);
        c2.SaveVar(v3);
        c2.SaveVar(v4);

        cnt.SaveVar(c1);
        cnt.SaveVar(c2);

        Packet packet = cnt.Pack();

        require(c1.size() == 2);
        require(c2.size() == 2);
        require(cnt.size() == 2);

        v1 = 123;
        v2 = 557575;
        v3 = 32.058f;
        v4 = {0.25f, 45, 7.56};

        c1.clear();
        c2.clear();
        cnt.clear();

        cnt.UnPack(packet);

        require(cnt.LoadVar(c1));
        require(cnt.LoadVar(c2));

        require(c1.LoadVar(v1));
        require(c1.LoadVar(v2));

        require(c2.LoadVar(v3));
        require(c2.LoadVar(v4));

        require(c1.size() == 2);
        require(c2.size() == 2);
        require(cnt.size() == 2);

        require(v1.val() == 12);
        require(v2.val() == 14);
        require(v3 == 25.251f);
        require(v4.x == 4.5f);
        require(v4.s == 11);
        require(v4.d == 5.6);
    }

    { // VarContainer large struct
        struct SomeLargeGameState {
            char data[5000];
            int number;
        };
        static SomeLargeGameState state;
        state.number = 178;

        VarContainer cnt;
        static Var<SomeLargeGameState> s1 = {"Variable", state};
        require(s1.number == state.number);
        cnt.SaveVar(s1);

        static Var<SomeLargeGameState> s2 = {"Variable", state};
        cnt.LoadVar(s2);
        require(s2.number == state.number);
    }

    { // VarContainer string
        VarContainer cnt;

        Var<std::string> s1 = {"String", "qwe"};
        cnt.SaveVar(s1);

        Var<std::string> s2 = {"String"};
        cnt.LoadVar(s2);

        require(s1 == s2);
    }

    { // malformed packed containers must be rejected, never read past the buffer
        auto append16 = [](Packet &p, uint16_t v) {
            p.push_back((unsigned char) (v & 0xFFu));
            p.push_back((unsigned char) (v >> 8u));
        };
        {
            Packet bad;
            append16(bad, 1);
            append16(bad, 0xFFFF); // data_size runs past the buffer
            append16(bad, 0x1234);
            append16(bad, 0);
            VarContainer cnt;
            require(!cnt.UnPack(bad));
        }
        {
            Packet bad;
            append16(bad, 0xFFFF); // header claiming 128 KiB the buffer does not have
            append16(bad, 0);
            VarContainer cnt;
            require(!cnt.UnPack(bad));
        }
        {
            Packet bad;
            append16(bad, 1);
            append16(bad, 2);
            append16(bad, 5); // truncated: the hash/offset pair is incomplete
            VarContainer cnt;
            require(!cnt.UnPack(bad));
        }
        { // structurally valid, but the entry offset points past the data
            Var<std::string> s1 = {"String", "hello world"};
            VarContainer src;
            src.SaveVar(s1);
            Packet bad = src.Pack();
            bad[2] = 0;
            bad[3] = 0;      // data_size = 0
            bad[6] = 0xF0;
            bad[7] = 0xFF;   // offset 0xFFF0
            VarContainer cnt;
            require(cnt.UnPack(bad));
            Var<std::string> s2 = {"String"};
            require(!cnt.LoadVar(s2));
        }
        { // string after a fixed-size var: the old length math used the next
          // entry's absolute offset instead of the difference between offsets
            Var<int> v1 = {"V1", 42};
            Var<std::string> s1 = {"String", "hello world"};
            Var<std::string> s2 = {"Other", "12345"};
            VarContainer src;
            src.SaveVar(v1);
            src.SaveVar(s1);
            src.SaveVar(s2);

            VarContainer cnt;
            require(cnt.UnPack(src.Pack()));
            Var<std::string> r1 = {"String"};
            Var<std::string> r2 = {"Other"};
            require(cnt.LoadVar(r1));
            require(cnt.LoadVar(r2));
            require(r1 == s1);
            require(r2 == s2);
        }
    }

    printf("OK\n");
}
