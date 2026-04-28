#include "test_common.h"

#include "../String.h"

void test_string() {
    using namespace Snd;

    printf("Test string             | ");

    { // Basic usage
        String s1, s2;

        s1 = String{"abcd"};
        s2 = String{"12345"};

        require(s1.length() == 4);
        require(s2.length() == 5);

        require(!(s1 == s2));
        require(s1 != s2);

        require(strcmp(s1.c_str(), "abcd") == 0);
        require(strcmp(s2.c_str(), "12345") == 0);

        String s3 = s1, s4 = s2;

        require(s3.c_str() == s1.c_str());
        require(s4.c_str() == s2.c_str());

        s1 = String{"another"};
        s2 = String{"another"};

        require(strcmp(s3.c_str(), "abcd") == 0);
        require(strcmp(s4.c_str(), "12345") == 0);
    }

    { // Move
        String s1 = String{"test_string"}, s2;

        require(s1.length() == 11);
        require(s2.length() == 0);

        const char *p_str = s1.c_str();
        s2 = std::move(s1);

        require(s2.length() == 11);
        require(s1.length() == 0);

        s1 = {};

        require(s2.c_str() == p_str);
        require(strcmp(s2.c_str(), "test_string") == 0);
    }

    { // Extra things
        auto s1 = String{"my_image.png"};

        require(s1.EndsWith(".png"));
        require(!s1.EndsWith(".ang"));
    }

    { // Part of string
        auto s1 = String{"test_string"}, s2 = String{"another_test_string_bla_bla"};

        std::string_view s3 = {&s2[8], 11}, s4 = {&s2[8], 12};

        require(s1 == s3);
        require(s1 != s4);
        require(s3 == s1);
        require(s4 != s1);

        auto s5 = String{s3}, s6 = String{s4};

        require(s1 == s5);
        require(s1 != s6);
        require(s5 == s1);
        require(s6 != s1);
    }

    { // Detach via non-const operator[]
        String s1 = String{"hello"};
        String s2 = s1;

        require(s1.c_str() == s2.c_str()); // shared buffer before mutation

        s2[0] = 'H'; // triggers detach

        require(s1.c_str() != s2.c_str()); // independent buffers after mutation
        require(strcmp(s1.c_str(), "hello") == 0);
        require(strcmp(s2.c_str(), "Hello") == 0);
    }

    { // operator+=
        String s1 = String{"hello"};

        s1 += " world";
        require(s1.length() == 11);
        require(strcmp(s1.c_str(), "hello world") == 0);

        s1 += String{" foo"};
        require(strcmp(s1.c_str(), "hello world foo") == 0);

        s1 += '!';
        require(strcmp(s1.c_str(), "hello world foo!") == 0);

        // append to default-constructed (null) string
        String s2;
        s2 += "test";
        require(strcmp(s2.c_str(), "test") == 0);
        require(s2.length() == 4);

        // self-append
        String s3 = String{"ab"};
        s3 += s3;
        require(strcmp(s3.c_str(), "abab") == 0);
        require(s3.length() == 4);
    }

    { // operator+
        String s1 = String{"hello"};

        String s2 = s1 + " world";
        require(strcmp(s2.c_str(), "hello world") == 0);
        require(strcmp(s1.c_str(), "hello") == 0); // s1 unchanged

        std::string_view sv{" there"};
        String s3 = sv + s1;
        require(strcmp(s3.c_str(), " therehello") == 0);

        String s4 = String{"hi"} + String{" there"};
        require(strcmp(s4.c_str(), "hi there") == 0);

        String s5 = "greeting: " + s1;
        require(strcmp(s5.c_str(), "greeting: hello") == 0);
    }

    { // Self-assignment
        String s1 = String{"self"};
        const char *p = s1.c_str();

        s1 = s1;
        require(s1.c_str() == p);
        require(strcmp(s1.c_str(), "self") == 0);
    }

    { // StartsWith / EndsWith edge cases
        String s1 = String{"hi"};

        require(!s1.EndsWith("hello"));   // suffix longer than string
        require(!s1.StartsWith("hello")); // prefix longer than string

        require(s1.EndsWith("hi"));
        require(s1.StartsWith("hi"));
        require(s1.EndsWith("i"));
        require(s1.StartsWith("h"));
        require(!s1.EndsWith("h"));
        require(!s1.StartsWith("i"));

        require(s1.EndsWith(""));
        require(s1.StartsWith(""));
    }

    printf("OK\n");
}
