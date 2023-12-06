#include "test_common.h"

#include "../VarContainer.h"

void test_var() {
    using namespace Net;

    { // Var test1
        Var<int> v1 = {"V1"};
        Var<int> v2 = {"V2"};

        v1 = 12;
        v2 = 12;

        assert(v1 == v2);
        assert(v2.val() == 12);
        assert(v1.hash() != v2.hash());
    }

    { // Var test2
        Var<int> v1 = {"VAR"};
        Var<int> v2 = {"VAR"};

        v1 = 13;
        v2 = 12;

        assert(v1 != v2);
        assert(v1.hash() == v2.hash());
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

        assert(v1.a == 1);
        assert(v2.dd == 10.0);
    }

    { // VarContainer save/load test
        struct S1 {
            float x;
            short s;
            double d;
        };
        std::vector<unsigned char> pack;
        Net::Var<int> v1 = {"V1", 12};
        Net::Var<int> v2 = {"V2", 13};
        Net::Var<float> v3 = {"V3", 25.251f};
        Net::Var<S1> v4 = {"V4"};
        v4 = {4.5f, 11, 5.6};

        {
            VarContainer cnt;

            cnt.SaveVar(v1);
            cnt.SaveVar(v2);
            cnt.SaveVar(v3);
            cnt.SaveVar(v4);

            assert(cnt.size() == 4);
            pack = cnt.Pack();
            assert(pack.size() == sizeof(VarContainer::int_type) * 2 + 2 * 4 * sizeof(VarContainer::int_type) +
                                      2 * sizeof(int) + sizeof(float) + sizeof(S1));
        }

        v1 = 11;
        v2 = 14;
        v3 = 15.044f;
        v4 = {-4.5f, -11, -5.6};
        assert(v1 == 11);
        assert(v2 == 14);
        assert(v3 == 15.044f);
        assert(v4.d == -5.6);

        Net::VarContainer cnt;
        cnt.UnPack(pack);
        assert(cnt.size() == 4);

        assert(cnt.LoadVar(v1));
        assert(cnt.LoadVar(v2));
        assert(cnt.LoadVar(v3));
        assert(cnt.LoadVar(v4));

        assert(v1 == 12);
        assert(v2 == 13);
        assert(v3 == 25.251f);
        assert(v4.d == 5.6);
    }

    { // VarContainer update test
        std::vector<unsigned char> pack;
        Var<int> v1 = {"V1", 12};
        Var<int> v2 = {"V2", 13};
        Var<float> v3 = {"V3", 25.251f};
        {
            Net::VarContainer cnt;

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
        assert(cnt.size() == 3);

        assert(cnt.LoadVar(v1));
        assert(cnt.LoadVar(v2));
        assert(cnt.LoadVar(v3));

        assert(v1 == 11);
        assert(v2 == 14);
        assert(v3 == 15.044f);
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

        assert(c1.size() == 2);
        assert(c2.size() == 2);
        assert(cnt.size() == 2);

        v1 = 123;
        v2 = 557575;
        v3 = 32.058f;
        v4 = {0.25f, 45, 7.56};

        c1.clear();
        c2.clear();
        cnt.clear();

        cnt.UnPack(packet);

        assert(cnt.LoadVar(c1));
        assert(cnt.LoadVar(c2));

        assert(c1.LoadVar(v1));
        assert(c1.LoadVar(v2));

        assert(c2.LoadVar(v3));
        assert(c2.LoadVar(v4));

        assert(c1.size() == 2);
        assert(c2.size() == 2);
        assert(cnt.size() == 2);

        assert(v1.val() == 12);
        assert(v2.val() == 14);
        assert(v3 == 25.251f);
        assert(v4.x == 4.5f);
        assert(v4.s == 11);
        assert(v4.d == 5.6);
    }

    { // VarContainer large struct
        struct SomeLargeGameState {
            char data[5000];
            int number;
        };
        SomeLargeGameState state;
        state.number = 178;

        Net::VarContainer cnt;
        Net::Var<SomeLargeGameState> s1 = {"Variable", state};
        assert(s1.number == state.number);
        cnt.SaveVar(s1);

        Net::Var<SomeLargeGameState> s2 = {"Variable", state};
        cnt.LoadVar(s2);
        assert(s2.number == state.number);
    }

    { // VarContainer string
        Net::VarContainer cnt;

        Net::Var<std::string> s1 = {"String", "qwe"};
        cnt.SaveVar(s1);

        Net::Var<std::string> s2 = {"String"};
        cnt.LoadVar(s2);

        assert(s1 == s2);
    }
}
