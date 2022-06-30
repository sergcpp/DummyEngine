#include "DummyApp.h"

#undef main
int main(int argc, char *argv[]) {
    return DummyApp().Run(argc, argv);
}

