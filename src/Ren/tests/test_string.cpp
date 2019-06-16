#include "test_common.h"

#include "../String.h"

void test_string() {
    {   // Basic usage
        Ren::String s1, s2;

        s1 = "abcd";
        s2 = "12345";

        require(s1.length() == 4);
        require(s2.length() == 5);

        require(!(s1 == s2));
        require(s1 != s2);

        require(strcmp(s1.c_str(), "abcd") == 0);
        require(strcmp(s2.c_str(), "12345") == 0);
    }

    {   // Move
        Ren::String s1 = "test_string", s2;

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

    {   // Extra things
        Ren::String s1 = "my_image.png";

        require(s1.EndsWith(".png"));
        require(!s1.EndsWith(".ang"));
    }
}