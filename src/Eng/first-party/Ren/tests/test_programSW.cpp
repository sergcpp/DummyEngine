#include "test_common.h"

#include <string>

#include "../Context.h"
#include "../Program.h"
#include "../SW/SW.h"

class ProgramTest : public Ren::Context {
public:
    ProgramTest() {
        Ren::Context::Init(1, 1);
    }
};

VSHADER vshader(VS_IN, VS_OUT) {}

FSHADER fshader(FS_IN, FS_OUT) {}

void test_program() {
    {
        // Create program
        ProgramTest test;

        Ren::eProgLoadStatus status;
        Ren::Attribute _attrs[] = { {} };
        Ren::Uniform _unifs[] = { {} };
        Ren::ProgramRef p = test.LoadProgramSW("constant", nullptr, nullptr, 0, _attrs, _unifs, &status);

        require(p);
        require(status == Ren::ProgSetToDefault);

        {
            Ren::Program *pp = p.get();

            require(pp != nullptr);
            require(std::string(pp->name()) == "constant");
            require(pp->prog_id() == 0); // default value
            require(!pp->ready());
        }

        Ren::Uniform unifs[] = { {"unif1", 0, SW_FLOAT, 1}, {"unif2", 1, SW_VEC3, 1}, {} };
        Ren::Attribute attrs[] = { {"attr1", 0, -1, 1}, {"attr2", 1, -1, 1}, {} };

        test.LoadProgramSW("constant", (void*)vshader, (void*)fshader, 0, attrs, unifs, &status);

        require(status == Ren::ProgCreatedFromData);

        Ren::Program *pp = p.get();

        require(pp != nullptr);
        require(std::string(pp->name()) == "constant");

        require(std::string(pp->uniform(0).name) == "unif1");
        require(pp->uniform(0).loc == 0);
        require(std::string(pp->uniform(1).name) == "unif2");
        require(pp->uniform(1).loc == 1);

        require(std::string(pp->attribute(0).name) == "attr1");
        require(pp->attribute(0).loc == 0);
        require(std::string(pp->attribute(1).name) == "attr2");
        require(pp->attribute(1).loc == 1);

        require(pp->ready());
    }
}
